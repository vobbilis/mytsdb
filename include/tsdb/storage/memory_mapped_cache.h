#pragma once

#include "tsdb/core/types.h"
#include "tsdb/core/error.h"
#include "tsdb/storage/cache_types.h"  // Include for shared cache types
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <list>

namespace tsdb {
namespace storage {

/**
 * @brief Memory-mapped cache for L2 level storage
 * 
 * This cache uses memory-mapped files to provide a larger cache than
 * pure in-memory storage while maintaining good performance. It's designed
 * to be used as the L2 level in a hierarchical cache system.
 */
class MemoryMappedCache {
public:
    /**
     * @brief Construct a new Memory Mapped Cache
     * @param config Configuration for the cache
     */
    explicit MemoryMappedCache(const CacheHierarchyConfig& config);
    
    /**
     * @brief Destructor
     */
    ~MemoryMappedCache();
    
    // Non-copyable, non-movable
    MemoryMappedCache(const MemoryMappedCache&) = delete;
    MemoryMappedCache& operator=(const MemoryMappedCache&) = delete;
    MemoryMappedCache(MemoryMappedCache&&) = delete;
    MemoryMappedCache& operator=(MemoryMappedCache&&) = delete;
    
    /**
     * @brief Get a time series from L2 cache
     * @param series_id The series ID to look up
     * @return Pointer to cached TimeSeries if found, nullptr otherwise
     */
    std::shared_ptr<core::TimeSeries> get(core::SeriesID series_id);
    
    /**
     * @brief Put a time series into L2 cache
     * @param series_id The series ID
     * @param series The time series to cache
     * @return true if successful, false if cache is full
     */
    bool put(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series);
    
    /**
     * @brief Remove a time series from L2 cache
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
     * @return String containing cache metrics
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
     * @brief Get metadata for a specific series
     * @param series_id The series ID
     * @return Pointer to metadata if found, nullptr otherwise
     */
    const CacheEntryMetadata* get_metadata(core::SeriesID series_id) const;
    
    /**
     * @brief Get all series IDs in the cache
     * @return Vector of series IDs
     */
    std::vector<core::SeriesID> get_all_series_ids() const;

private:
    CacheHierarchyConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<core::SeriesID, std::shared_ptr<core::TimeSeries>> cache_map_;
    std::unordered_map<core::SeriesID, CacheEntryMetadata> metadata_map_;
    
    // LRU list for eviction
    std::list<core::SeriesID> lru_list_;
    std::unordered_map<core::SeriesID, std::list<core::SeriesID>::iterator> lru_map_;
    
    // Statistics
    mutable std::atomic<uint64_t> hit_count_{0};
    mutable std::atomic<uint64_t> miss_count_{0};
    mutable std::atomic<uint64_t> eviction_count_{0};
    
    /**
     * @brief Evict least recently used entries when cache is full
     */
    void evict_lru();
    
    /**
     * @brief Update metadata for an access
     * @param series_id The series ID that was accessed
     */
    void update_metadata(core::SeriesID series_id);
    
    /**
     * @brief Move an entry to the front of the LRU list
     * @param series_id The series ID to move
     */
    void move_to_front(core::SeriesID series_id);
    
    /**
     * @brief Add an entry to the front of the LRU list
     * @param series_id The series ID to add
     */
    void add_to_front(core::SeriesID series_id);
    
    /**
     * @brief Remove an entry from the LRU list
     * @param series_id The series ID to remove
     */
    void remove_from_lru(core::SeriesID series_id);
};

} // namespace storage
} // namespace tsdb 