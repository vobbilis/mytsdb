#include "tsdb/storage/memory_mapped_cache.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <filesystem>

namespace tsdb {
namespace storage {

MemoryMappedCache::MemoryMappedCache(const CacheHierarchyConfig& config)
    : config_(config) {
    // Create storage directory if it doesn't exist
    std::filesystem::create_directories(config_.l2_storage_path);
}

MemoryMappedCache::~MemoryMappedCache() = default;

std::shared_ptr<core::TimeSeries> MemoryMappedCache::get(core::SeriesID series_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(series_id);
    if (it == cache_map_.end()) {
        // Cache miss
        miss_count_.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    
    // Cache hit - update metadata and move to front of LRU
    update_metadata(series_id);
    move_to_front(series_id);
    hit_count_.fetch_add(1, std::memory_order_relaxed);
    
    return it->second;
}

bool MemoryMappedCache::put(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series) {
    if (!series) {
        throw core::InvalidArgumentError("Cannot cache null time series");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(series_id);
    if (it != cache_map_.end()) {
        // Update existing entry
        it->second = std::move(series);
        update_metadata(series_id);
        move_to_front(series_id);
    } else {
        // Add new entry
        if (cache_map_.size() >= config_.l2_max_size) {
            evict_lru();
        }
        
        // Calculate size (simplified for now)
        size_t size_bytes = series->samples().size() * sizeof(core::Sample) + 100; // Approximate

        // Add to cache map
        cache_map_[series_id] = std::move(series);
        
        // Add metadata
        metadata_map_[series_id] = CacheEntryMetadata(series_id);
        metadata_map_[series_id].size_bytes = size_bytes;
        
        // Add to LRU list
        add_to_front(series_id);
    }
    
    return true;
}

bool MemoryMappedCache::remove(core::SeriesID series_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(series_id);
    if (it == cache_map_.end()) {
        return false;
    }
    
    // Remove from cache map
    cache_map_.erase(it);
    
    // Remove metadata
    metadata_map_.erase(series_id);
    
    // Remove from LRU list
    remove_from_lru(series_id);
    
    return true;
}

void MemoryMappedCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_map_.clear();
    metadata_map_.clear();
    lru_list_.clear();
    lru_map_.clear();
}

size_t MemoryMappedCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.size();
}

size_t MemoryMappedCache::max_size() const {
    return config_.l2_max_size;
}

bool MemoryMappedCache::is_full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.size() >= config_.l2_max_size;
}

std::string MemoryMappedCache::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "MemoryMappedCache (L2) Stats:\n";
    oss << "  Current size: " << cache_map_.size() << "/" << config_.l2_max_size << "\n";
    oss << "  Hit count: " << hit_count_.load(std::memory_order_relaxed) << "\n";
    oss << "  Miss count: " << miss_count_.load(std::memory_order_relaxed) << "\n";
    oss << "  Eviction count: " << eviction_count_.load(std::memory_order_relaxed) << "\n";
    
    uint64_t total_requests = hit_count_.load(std::memory_order_relaxed) + 
                             miss_count_.load(std::memory_order_relaxed);
    
    if (total_requests > 0) {
        double hit_ratio = static_cast<double>(hit_count_.load(std::memory_order_relaxed)) / 
                          total_requests * 100.0;
        oss << "  Hit ratio: " << std::fixed << std::setprecision(2) << hit_ratio << "%\n";
    } else {
        oss << "  Hit ratio: N/A (no requests yet)\n";
    }
    
    // Calculate memory usage
    size_t total_memory = 0;
    for (const auto& [id, metadata] : metadata_map_) {
        total_memory += metadata.size_bytes;
    }
    oss << "  Memory usage: " << (total_memory / 1024 / 1024) << " MB / " 
        << config_.l2_max_memory_mb << " MB\n";
    
    return oss.str();
}

uint64_t MemoryMappedCache::hit_count() const {
    return hit_count_.load(std::memory_order_relaxed);
}

uint64_t MemoryMappedCache::miss_count() const {
    return miss_count_.load(std::memory_order_relaxed);
}

double MemoryMappedCache::hit_ratio() const {
    uint64_t hits = hit_count_.load(std::memory_order_relaxed);
    uint64_t misses = miss_count_.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;
    
    if (total == 0) {
        return 0.0;
    }
    
    return static_cast<double>(hits) / total * 100.0;
}

void MemoryMappedCache::reset_stats() {
    hit_count_.store(0, std::memory_order_relaxed);
    miss_count_.store(0, std::memory_order_relaxed);
    eviction_count_.store(0, std::memory_order_relaxed);
}

std::optional<CacheEntryMetadata> MemoryMappedCache::get_metadata(core::SeriesID series_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = metadata_map_.find(series_id);
    if (it == metadata_map_.end()) {
        return std::nullopt;
    }
    
    // Return a copy to avoid handing out pointers to internal storage
    // (which can become invalid after the lock is released).
    return it->second;
}

std::vector<core::SeriesID> MemoryMappedCache::get_all_series_ids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<core::SeriesID> ids;
    ids.reserve(cache_map_.size());
    
    for (const auto& [id, _] : cache_map_) {
        ids.push_back(id);
    }
    
    return ids;
}

void MemoryMappedCache::evict_lru() {
    if (lru_list_.empty()) {
        return;
    }
    
    // Remove the least recently used entry (back of the list)
    core::SeriesID lru_id = lru_list_.back();
    
    // Remove from cache map
    cache_map_.erase(lru_id);
    
    // Remove metadata
    metadata_map_.erase(lru_id);
    
    // Remove from LRU list
    lru_list_.pop_back();
    lru_map_.erase(lru_id);
    
    eviction_count_.fetch_add(1, std::memory_order_relaxed);
}

void MemoryMappedCache::update_metadata(core::SeriesID series_id) {
    auto it = metadata_map_.find(series_id);
    if (it != metadata_map_.end()) {
        it->second.record_access();
    }
}

void MemoryMappedCache::move_to_front(core::SeriesID series_id) {
    auto lru_it = lru_map_.find(series_id);
    if (lru_it != lru_map_.end()) {
        // Move to front of LRU list
        lru_list_.splice(lru_list_.begin(), lru_list_, lru_it->second);
        // Update the iterator to point to the new position
        lru_it->second = lru_list_.begin();
    }
}

void MemoryMappedCache::add_to_front(core::SeriesID series_id) {
    lru_list_.push_front(series_id);
    lru_map_[series_id] = lru_list_.begin();
}

void MemoryMappedCache::remove_from_lru(core::SeriesID series_id) {
    auto lru_it = lru_map_.find(series_id);
    if (lru_it != lru_map_.end()) {
        lru_list_.erase(lru_it->second);
        lru_map_.erase(lru_it);
    }
}

} // namespace storage
} // namespace tsdb 