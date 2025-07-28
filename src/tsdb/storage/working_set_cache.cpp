#include "tsdb/storage/working_set_cache.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace tsdb {
namespace storage {

WorkingSetCache::WorkingSetCache(size_t max_size)
    : max_size_(max_size) {
    if (max_size == 0) {
        throw core::InvalidArgumentError("Cache max size must be greater than 0");
    }
}

std::shared_ptr<core::TimeSeries> WorkingSetCache::get(core::SeriesID series_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(series_id);
    if (it == cache_map_.end()) {
        // Cache miss
        miss_count_.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    
    // Cache hit - move to front of LRU list
    move_to_front(it->second);
    hit_count_.fetch_add(1, std::memory_order_relaxed);
    
    return it->second->series;
}

void WorkingSetCache::put(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series) {
    if (!series) {
        throw core::InvalidArgumentError("Cannot cache null time series");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(series_id);
    if (it != cache_map_.end()) {
        // Update existing entry
        it->second->series = std::move(series);
        move_to_front(it->second);
    } else {
        // Add new entry
        if (cache_map_.size() >= max_size_) {
            evict_lru();
        }
        add_entry(series_id, std::move(series));
    }
}

bool WorkingSetCache::remove(core::SeriesID series_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(series_id);
    if (it == cache_map_.end()) {
        return false;
    }
    
    // Remove from LRU list
    lru_list_.erase(it->second);
    cache_map_.erase(it);
    
    return true;
}

void WorkingSetCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    lru_list_.clear();
    cache_map_.clear();
}

size_t WorkingSetCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.size();
}

size_t WorkingSetCache::max_size() const {
    return max_size_;
}

bool WorkingSetCache::is_full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.size() >= max_size_;
}

std::string WorkingSetCache::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "WorkingSetCache Stats:\n";
    oss << "  Current size: " << cache_map_.size() << "/" << max_size_ << "\n";
    oss << "  Hit count: " << hit_count_.load(std::memory_order_relaxed) << "\n";
    oss << "  Miss count: " << miss_count_.load(std::memory_order_relaxed) << "\n";
    
    uint64_t total_requests = hit_count_.load(std::memory_order_relaxed) + 
                             miss_count_.load(std::memory_order_relaxed);
    
    if (total_requests > 0) {
        double hit_ratio = static_cast<double>(hit_count_.load(std::memory_order_relaxed)) / 
                          total_requests * 100.0;
        oss << "  Hit ratio: " << std::fixed << std::setprecision(2) << hit_ratio << "%\n";
    } else {
        oss << "  Hit ratio: N/A (no requests yet)\n";
    }
    
    return oss.str();
}

uint64_t WorkingSetCache::hit_count() const {
    return hit_count_.load(std::memory_order_relaxed);
}

uint64_t WorkingSetCache::miss_count() const {
    return miss_count_.load(std::memory_order_relaxed);
}

double WorkingSetCache::hit_ratio() const {
    uint64_t hits = hit_count_.load(std::memory_order_relaxed);
    uint64_t misses = miss_count_.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;
    
    if (total == 0) {
        return 0.0;
    }
    
    return static_cast<double>(hits) / total * 100.0;
}

void WorkingSetCache::reset_stats() {
    hit_count_.store(0, std::memory_order_relaxed);
    miss_count_.store(0, std::memory_order_relaxed);
}

std::vector<core::SeriesID> WorkingSetCache::get_all_series_ids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<core::SeriesID> ids;
    ids.reserve(cache_map_.size());
    
    for (const auto& [id, _] : cache_map_) {
        ids.push_back(id);
    }
    
    return ids;
}

core::SeriesID WorkingSetCache::get_lru_series_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (lru_list_.empty()) {
        return 0;
    }
    
    // Return the series ID of the least recently used item (back of the list)
    return lru_list_.back().series_id;
}

std::shared_ptr<core::TimeSeries> WorkingSetCache::evict_lru_and_get() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (lru_list_.empty()) {
        return nullptr;
    }
    
    // Get the least recently used entry (back of the list)
    auto lru_it = std::prev(lru_list_.end());
    auto series = lru_it->series;
    
    // Remove from cache map and list
    cache_map_.erase(lru_it->series_id);
    lru_list_.pop_back();
    
    return series;
}

std::pair<core::SeriesID, std::shared_ptr<core::TimeSeries>> WorkingSetCache::evict_lru_and_get_with_id() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (lru_list_.empty()) {
        return {0, nullptr};
    }
    
    // Get the least recently used entry (back of the list)
    auto lru_it = std::prev(lru_list_.end());
    auto series_id = lru_it->series_id;
    auto series = lru_it->series;
    
    // Remove from cache map and list
    cache_map_.erase(series_id);
    lru_list_.pop_back();
    
    return {series_id, series};
}

void WorkingSetCache::move_to_front(LRUIterator it) {
    // Move the entry to the front of the LRU list
    lru_list_.splice(lru_list_.begin(), lru_list_, it);
}

void WorkingSetCache::evict_lru() {
    if (lru_list_.empty()) {
        return;
    }
    
    // Remove the least recently used entry (back of the list)
    auto lru_it = std::prev(lru_list_.end());
    cache_map_.erase(lru_it->series_id);
    lru_list_.pop_back();
}

void WorkingSetCache::add_entry(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series) {
    // Add new entry to front of LRU list
    lru_list_.emplace_front(series_id, std::move(series));
    
    // Store iterator in cache map
    cache_map_[series_id] = lru_list_.begin();
}

} // namespace storage
} // namespace tsdb 