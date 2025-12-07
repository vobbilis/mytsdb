/**
 * @file working_set_cache.cpp
 * @brief LRU-based working set cache implementation for time series data
 * 
 * This file implements a Least Recently Used (LRU) cache for frequently
 * accessed time series data. The cache provides fast access to hot data
 * while automatically evicting cold data when capacity limits are reached.
 * 
 * Key Features:
 * - LRU eviction policy for optimal cache performance
 * - Thread-safe operations with mutex protection
 * - Comprehensive statistics tracking (hit/miss ratios)
 * - Configurable maximum cache size
 * - Efficient O(1) average case operations
 * 
 * Implementation Details:
 * - Uses std::list for LRU ordering (O(1) splice operations)
 * - Uses std::unordered_map for O(1) lookups
 * - Stores iterators in map for efficient list manipulation
 * - Atomic counters for thread-safe statistics
 * 
 * Performance Characteristics:
 * - Get/Put operations: O(1) average case
 * - Memory overhead: ~24 bytes per cached entry
 * - Thread safety: Full mutex protection
 */

#include "tsdb/storage/working_set_cache.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace tsdb {
namespace storage {

/**
 * @brief Constructs a WorkingSetCache with the specified maximum size
 * 
 * @param max_size Maximum number of time series that can be cached
 * 
 * This constructor initializes the LRU cache with:
 * - Maximum capacity limit for cache entries
 * - Empty LRU list and cache map
 * - Zero-initialized hit/miss counters
 * 
 * The cache will automatically evict the least recently used
 * time series when the maximum size is reached.
 * 
 * @throws core::InvalidArgumentError if max_size is 0
 * 
 * Thread Safety: Constructor is not thread-safe, should be called
 * before any concurrent access to the cache.
 */
WorkingSetCache::WorkingSetCache(size_t max_size)
    : max_size_(max_size) {
    if (max_size == 0) {
        throw core::InvalidArgumentError("Cache max size must be greater than 0");
    }
}

/**
 * @brief Retrieves a time series from the cache by its ID
 * 
 * @param series_id The unique identifier of the time series to retrieve
 * @return Shared pointer to the cached time series, or nullptr if not found
 * 
 * This method performs a cache lookup for the specified time series:
 * - Searches the cache map for the series ID
 * - Returns nullptr on cache miss (updates miss counter)
 * - On cache hit, moves the entry to the front of the LRU list
 * - Updates hit counter and returns the cached time series
 * 
 * The LRU ordering is maintained by moving accessed entries to the
 * front of the list, ensuring that frequently accessed data stays
 * in the cache longer.
 * 
 * Performance: O(1) average case lookup time
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the cache's internal data structures.
 */
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
    
    // Record access for metadata tracking
    it->second->metadata.record_access();
    
    return it->second->series;
}

/**
 * @brief Stores a time series in the cache
 * 
 * @param series_id The unique identifier for the time series
 * @param series Shared pointer to the time series to cache
 * 
 * This method stores a time series in the cache:
 * - Validates that the series pointer is not null
 * - Updates existing entry if series_id already exists
 * - Adds new entry if series_id is not in cache
 * - Automatically evicts LRU entry if cache is full
 * - Maintains LRU ordering by moving updated entries to front
 * 
 * When the cache is full, the least recently used entry is
 * automatically evicted to make room for the new entry.
 * 
 * @throws core::InvalidArgumentError if series is null
 * 
 * Performance: O(1) average case insertion time
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the cache's internal data structures.
 */
void WorkingSetCache::put(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series) {
    if (!series) {
        throw core::InvalidArgumentError("Cannot cache null time series");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(series_id);
    if (it != cache_map_.end()) {
        // Update existing entry - MERGE samples
        // Instead of overwriting, we append new samples to the existing series
        auto& existing_series = it->second->series;
        
        // Get the last timestamp from existing series to ensure chronological order
        core::Timestamp last_ts = 0;
        bool has_samples = false;
        
        auto current_samples = existing_series->samples();
        if (!current_samples.empty()) {
            last_ts = current_samples.back().timestamp();
            has_samples = true;
        }

        for (const auto& sample : series->samples()) {
            // Only add samples that are newer than the last sample
            if (!has_samples || sample.timestamp() > last_ts) {
                existing_series->add_sample(sample);
                last_ts = sample.timestamp();
                has_samples = true;
            }
        }
        move_to_front(it->second);
    } else {
        // Add new entry
        if (cache_map_.size() >= max_size_) {
            evict_lru();
        }
        add_entry(series_id, std::move(series));
    }
}

/**
 * @brief Removes a time series from the cache
 * 
 * @param series_id The unique identifier of the time series to remove
 * @return true if the series was found and removed, false otherwise
 * 
 * This method removes a time series from the cache:
 * - Searches for the series_id in the cache map
 * - Returns false if the series is not found
 * - Removes the entry from both the LRU list and cache map
 * - Returns true on successful removal
 * 
 * The removal operation maintains the integrity of both the
 * cache map and LRU list data structures.
 * 
 * Performance: O(1) average case removal time
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the cache's internal data structures.
 */
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

/**
 * @brief Removes all entries from the cache
 * 
 * This method completely clears the cache:
 * - Removes all entries from the LRU list
 * - Clears the cache map
 * - Maintains the maximum size limit for future operations
 * 
 * The cache is left in an empty state but remains functional
 * for future put/get operations.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the cache's internal data structures.
 */
void WorkingSetCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    lru_list_.clear();
    cache_map_.clear();
}

/**
 * @brief Returns the current number of entries in the cache
 * @return Number of cached time series
 * 
 * Thread Safety: Uses mutex locking to ensure accurate count
 * during concurrent access.
 */
size_t WorkingSetCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.size();
}

/**
 * @brief Returns the maximum capacity of the cache
 * @return Maximum number of entries the cache can hold
 */
size_t WorkingSetCache::max_size() const {
    return max_size_;
}

/**
 * @brief Checks if the cache is at maximum capacity
 * @return true if the cache is full, false otherwise
 * 
 * Thread Safety: Uses mutex locking to ensure accurate status
 * during concurrent access.
 */
bool WorkingSetCache::is_full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.size() >= max_size_;
}

/**
 * @brief Returns detailed statistics about the cache performance
 * 
 * @return String containing formatted cache statistics
 * 
 * This method provides comprehensive statistics about the cache's
 * performance and usage patterns, including:
 * - Current cache size vs. maximum capacity
 * - Total number of cache hits and misses
 * - Hit ratio percentage (hits / total requests)
 * - Formatted output for monitoring and debugging
 * 
 * The hit ratio is a key performance metric that indicates
 * how effectively the cache is serving frequently accessed data.
 * Higher hit ratios indicate better cache performance.
 * 
 * Thread Safety: Uses mutex locking to ensure consistent statistics
 * during concurrent access.
 */
std::string WorkingSetCache::stats() const {
    std::ostringstream oss;
    oss << "WorkingSetCache Stats:\n";
    oss << "  Current size: " << size() << "/" << max_size_ << "\n";
    oss << "  Hit count: " << hit_count() << "\n";
    oss << "  Miss count: " << miss_count() << "\n";
    oss << "  Hit ratio: " << std::fixed << std::setprecision(2) << hit_ratio() << "%\n";
    return oss.str();
}

/**
 * @brief Returns the total number of cache hits
 * @return Total hit count since last reset
 * 
 * Thread Safety: Uses atomic load for thread-safe access.
 */
uint64_t WorkingSetCache::hit_count() const {
    return hit_count_.load(std::memory_order_relaxed);
}

/**
 * @brief Returns the total number of cache misses
 * @return Total miss count since last reset
 * 
 * Thread Safety: Uses atomic load for thread-safe access.
 */
uint64_t WorkingSetCache::miss_count() const {
    return miss_count_.load(std::memory_order_relaxed);
}

/**
 * @brief Calculates the current hit ratio percentage
 * @return Hit ratio as a percentage (0.0 to 100.0)
 * 
 * Returns 0.0 if no requests have been made yet.
 * 
 * Thread Safety: Uses atomic loads for thread-safe access.
 */
double WorkingSetCache::hit_ratio() const {
    uint64_t hits = hit_count_.load(std::memory_order_relaxed);
    uint64_t misses = miss_count_.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;
    
    if (total == 0) {
        return 0.0;
    }
    
    return static_cast<double>(hits) / total * 100.0;
}

/**
 * @brief Resets all cache statistics to zero
 * 
 * This method resets both hit and miss counters to zero,
 * effectively starting fresh statistics collection.
 * 
 * Thread Safety: Uses atomic stores for thread-safe access.
 */
void WorkingSetCache::reset_stats() {
    hit_count_.store(0, std::memory_order_relaxed);
    miss_count_.store(0, std::memory_order_relaxed);
}

/**
 * @brief Returns all series IDs currently in the cache
 * 
 * @return Vector containing all cached series IDs
 * 
 * This method provides a snapshot of all time series currently
 * cached. The returned vector contains the series IDs in no
 * particular order.
 * 
 * Thread Safety: Uses mutex locking to ensure consistent snapshot
 * during concurrent access.
 */
std::vector<core::SeriesID> WorkingSetCache::get_all_series_ids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<core::SeriesID> ids;
    ids.reserve(cache_map_.size());
    
    for (const auto& [id, _] : cache_map_) {
        ids.push_back(id);
    }
    
    return ids;
}

/**
 * @brief Returns the series ID of the least recently used entry
 * 
 * @return Series ID of the LRU entry, or 0 if cache is empty
 * 
 * This method identifies which time series would be evicted next
 * if the cache reaches maximum capacity. Useful for:
 * - Cache monitoring and debugging
 * - Implementing custom eviction policies
 * - Understanding cache access patterns
 * 
 * Thread Safety: Uses mutex locking to ensure accurate LRU identification
 * during concurrent access.
 */
core::SeriesID WorkingSetCache::get_lru_series_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (lru_list_.empty()) {
        return 0;
    }
    
    // Return the series ID of the least recently used item (back of the list)
    return lru_list_.back().series_id;
}

/**
 * @brief Evicts the least recently used entry and returns the time series
 * 
 * @return Shared pointer to the evicted time series, or nullptr if cache is empty
 * 
 * This method removes the least recently used entry from the cache and
 * returns the associated time series. Useful for:
 * - Manual cache management
 * - Implementing custom eviction policies
 * - Cache monitoring and debugging
 * 
 * The evicted entry is completely removed from both the cache map
 * and LRU list.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe eviction
 * during concurrent access.
 */
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

/**
 * @brief Evicts the least recently used entry and returns both ID and time series
 * 
 * @return Pair containing the series ID and shared pointer to the evicted time series
 * 
 * This method removes the least recently used entry from the cache and
 * returns both the series ID and the associated time series. Useful for:
 * - Manual cache management with ID tracking
 * - Implementing custom eviction policies
 * - Cache monitoring and debugging
 * 
 * Returns {0, nullptr} if the cache is empty.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe eviction
 * during concurrent access.
 */
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

/**
 * @brief Moves an entry to the front of the LRU list
 * 
 * @param it Iterator pointing to the entry to move
 * 
 * This private helper method maintains the LRU ordering by moving
 * accessed entries to the front of the list. Uses std::list::splice
 * for O(1) list manipulation.
 * 
 * Thread Safety: Must be called with mutex lock held.
 */
void WorkingSetCache::move_to_front(LRUIterator it) {
    // Move the entry to the front of the LRU list
    lru_list_.splice(lru_list_.begin(), lru_list_, it);
}

/**
 * @brief Evicts the least recently used entry from the cache
 * 
 * This private helper method removes the least recently used entry
 * (back of the LRU list) from both the cache map and LRU list.
 * Used internally when the cache reaches maximum capacity.
 * 
 * Thread Safety: Must be called with mutex lock held.
 */
void WorkingSetCache::evict_lru() {
    if (lru_list_.empty()) {
        return;
    }
    
    // Remove the least recently used entry (back of the list)
    auto lru_it = std::prev(lru_list_.end());
    cache_map_.erase(lru_it->series_id);
    lru_list_.pop_back();
}

/**
 * @brief Adds a new entry to the cache
 * 
 * @param series_id The series ID for the new entry
 * @param series Shared pointer to the time series to cache
 * 
 * This private helper method adds a new entry to the cache:
 * - Inserts the entry at the front of the LRU list
 * - Stores the iterator in the cache map for O(1) lookups
 * - Maintains the LRU ordering invariant
 * 
 * Thread Safety: Must be called with mutex lock held.
 */
void WorkingSetCache::add_entry(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series) {
    // Add new entry to front of LRU list
    lru_list_.emplace_front(series_id, std::move(series));
    
    // Store iterator in cache map
    cache_map_[series_id] = lru_list_.begin();
}

/**
 * @brief Get metadata for a series in the cache
 * 
 * @param series_id The series ID to get metadata for
 * @return Pointer to metadata if found, nullptr otherwise
 * 
 * This method retrieves the access metadata for a cached time series,
 * which includes access count, last access time, and other tracking
 * information used for promotion/demotion decisions.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to cache metadata during concurrent operations.
 */
const CacheEntryMetadata* WorkingSetCache::get_metadata(core::SeriesID series_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(series_id);
    if (it == cache_map_.end()) {
        return nullptr;
    }
    
    return &it->second->metadata;
}

} // namespace storage
} // namespace tsdb 