#pragma once

#include "tsdb/core/types.h"
#include "tsdb/core/error.h"
#include "tsdb/storage/cache_types.h"
#include <unordered_map>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <atomic>
#include <string>

namespace tsdb {
namespace storage {

/**
 * @brief LRU cache for frequently accessed time series data
 * 
 * Implements a thread-safe Least Recently Used (LRU) cache for storing
 * frequently accessed TimeSeries objects. This reduces disk I/O and
 * improves read performance for hot data.
 */
class WorkingSetCache {
public:
    /**
     * @brief Construct a new Working Set Cache
     * @param max_size Maximum number of entries in the cache
     */
    explicit WorkingSetCache(size_t max_size = 1000);
    
    /**
     * @brief Destructor
     */
    ~WorkingSetCache() = default;
    
    // Disable copy constructor and assignment
    WorkingSetCache(const WorkingSetCache&) = delete;
    WorkingSetCache& operator=(const WorkingSetCache&) = delete;
    
    // Disable move constructor and assignment (due to mutex)
    WorkingSetCache(WorkingSetCache&&) = delete;
    WorkingSetCache& operator=(WorkingSetCache&&) = delete;
    
    /**
     * @brief Get a time series from cache
     * @param series_id The series ID to look up
     * @return Pointer to cached TimeSeries if found, nullptr otherwise
     */
    std::shared_ptr<core::TimeSeries> get(core::SeriesID series_id);
    
    /**
     * @brief Put a time series into cache
     * @param series_id The series ID
     * @param series The time series to cache
     */
    void put(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series);
    
    /**
     * @brief Remove a time series from cache
     * @param series_id The series ID to remove
     * @return true if found and removed, false otherwise
     */
    bool remove(core::SeriesID series_id);
    
    /**
     * @brief Clear all entries from cache
     */
    void clear();
    
    /**
     * @brief Get current cache size
     * @return Number of entries currently in cache
     */
    size_t size() const;
    
    /**
     * @brief Get maximum cache size
     * @return Maximum number of entries allowed
     */
    size_t max_size() const;
    
    /**
     * @brief Check if cache is full
     * @return true if cache is at maximum capacity
     */
    bool is_full() const;
    
    /**
     * @brief Get cache statistics
     * @return String containing cache hit/miss ratios and other metrics
     */
    std::string stats() const;
    
    /**
     * @brief Get cache hit count
     * @return Total number of cache hits
     */
    uint64_t hit_count() const;
    
    /**
     * @brief Get cache miss count
     * @return Total number of cache misses
     */
    uint64_t miss_count() const;
    
    /**
     * @brief Get cache hit ratio
     * @return Hit ratio as a percentage (0.0 to 100.0)
     */
    double hit_ratio() const;
    
    /**
     * @brief Reset cache statistics
     */
    void reset_stats();
    
    /**
     * @brief Get all series IDs currently in the cache
     * @return Vector of series IDs
     */
    std::vector<core::SeriesID> get_all_series_ids() const;
    
    /**
     * @brief Get the least recently used series ID
     * @return The LRU series ID, or 0 if cache is empty
     */
    core::SeriesID get_lru_series_id() const;
    
    /**
     * @brief Evict the least recently used item and return it
     * @return The evicted series, or nullptr if cache is empty
     */
    std::shared_ptr<core::TimeSeries> evict_lru_and_get();
    
    /**
     * @brief Evict the least recently used item and return both ID and series
     * @return Pair of (series_id, series), or (0, nullptr) if cache is empty
     */
    std::pair<core::SeriesID, std::shared_ptr<core::TimeSeries>> evict_lru_and_get_with_id();
    
    /**
     * @brief Get metadata for a series in the cache
     * @param series_id The series ID to get metadata for
     * @return Copy of metadata if found, std::nullopt otherwise
     */
    std::optional<CacheEntryMetadata> get_metadata(core::SeriesID series_id) const;

private:
    // Cache entry structure
    struct CacheEntry {
        core::SeriesID series_id;
        std::shared_ptr<core::TimeSeries> series;
        CacheEntryMetadata metadata;
        
        CacheEntry(core::SeriesID id, std::shared_ptr<core::TimeSeries> ts)
            : series_id(id), series(std::move(ts)) {}
    };
    
    // LRU list node type
    using LRUList = std::list<CacheEntry>;
    using LRUIterator = LRUList::iterator;
    
    // Cache map type
    using CacheMap = std::unordered_map<core::SeriesID, LRUIterator>;
    
    // Member variables
    mutable std::mutex mutex_;
    size_t max_size_;
    LRUList lru_list_;
    CacheMap cache_map_;
    
    // Statistics
    mutable std::atomic<uint64_t> hit_count_{0};
    mutable std::atomic<uint64_t> miss_count_{0};
    
    /**
     * @brief Move entry to front of LRU list (mark as recently used)
     * @param it Iterator to the entry to move
     */
    void move_to_front(LRUIterator it);
    
    /**
     * @brief Evict least recently used entry
     */
    void evict_lru();
    
    /**
     * @brief Add new entry to cache
     * @param series_id The series ID
     * @param series The time series to cache
     */
    void add_entry(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series);
};

} // namespace storage
} // namespace tsdb 