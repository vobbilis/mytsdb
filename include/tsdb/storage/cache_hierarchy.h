#pragma once

#include "tsdb/storage/working_set_cache.h"
#include "tsdb/storage/memory_mapped_cache.h"
#include "tsdb/storage/cache_types.h"  // Include for shared cache types
#include "tsdb/core/types.h"
#include "tsdb/core/error.h"
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>

namespace tsdb {
namespace storage {

/**
 * @brief Hierarchical cache system with L1/L2/L3 levels
 * 
 * This implements a multi-level cache hierarchy similar to CPU caches:
 * - L1: Fast in-memory cache (WorkingSetCache)
 * - L2: Memory-mapped file cache (MemoryMappedCache)
 * - L3: Disk storage (existing storage system)
 * 
 * Features:
 * - Automatic promotion/demotion based on access patterns
 * - Background processing for cache maintenance
 * - Comprehensive performance metrics
 * - Configurable policies and thresholds
 */
class CacheHierarchy {
public:
    /**
     * @brief Callback type for L3 (Parquet) persistence
     * Called when series data needs to be written to cold storage.
     * @param series_id The series ID being demoted
     * @param series The time series data to persist
     * @return true if persistence was successful
     */
    using L3PersistenceCallback = std::function<bool(core::SeriesID, std::shared_ptr<core::TimeSeries>)>;
    
    /**
     * @brief Construct a new Cache Hierarchy
     * @param config Configuration for the cache hierarchy
     */
    explicit CacheHierarchy(const CacheHierarchyConfig& config = CacheHierarchyConfig{});
    
    /**
     * @brief Destructor
     */
    ~CacheHierarchy();
    
    /**
     * @brief Set the L3 persistence callback
     * @param callback Function to call when demoting to L3 (Parquet)
     */
    void set_l3_persistence_callback(L3PersistenceCallback callback);
    
    // Non-copyable, non-movable
    CacheHierarchy(const CacheHierarchy&) = delete;
    CacheHierarchy& operator=(const CacheHierarchy&) = delete;
    CacheHierarchy(CacheHierarchy&&) = delete;
    CacheHierarchy& operator=(CacheHierarchy&&) = delete;
    
    /**
     * @brief Get a time series from the cache hierarchy
     * @param series_id The series ID to look up
     * @return Pointer to cached TimeSeries if found, nullptr otherwise
     */
    std::shared_ptr<core::TimeSeries> get(core::SeriesID series_id);
    
    /**
     * @brief Put a time series into the cache hierarchy
     * @param series_id The series ID
     * @param series The time series to cache
     * @return true if successful, false if all caches are full
     */
    bool put(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series);
    
    /**
     * @brief Remove a time series from all cache levels
     * @param series_id The series ID to remove
     * @return true if found and removed, false otherwise
     */
    bool remove(core::SeriesID series_id);
    
    /**
     * @brief Promote a series to a higher cache level
     * @param series_id The series ID to promote
     * @param target_level The target cache level (1, 2, or 3)
     * @return true if promotion was successful
     */
    bool promote(core::SeriesID series_id, int target_level);
    
    /**
     * @brief Demote a series to a lower cache level
     * @param series_id The series ID to demote
     * @param target_level The target cache level (1, 2, or 3)
     * @return true if demotion was successful
     */
    bool demote(core::SeriesID series_id, int target_level);
    
    /**
     * @brief Clear all cache levels
     */
    void clear();
    
    /**
     * @brief Get cache hierarchy statistics
     * @return String containing comprehensive cache metrics
     */
    std::string stats() const;
    
    /**
     * @brief Get hit ratio for the entire hierarchy
     * @return Overall hit ratio as a percentage (0.0 to 100.0)
     */
    double hit_ratio() const;
    
    /**
     * @brief Reset all cache statistics
     */
    void reset_stats();
    
    /**
     * @brief Start background processing for cache maintenance
     */
    void start_background_processing();
    
    /**
     * @brief Stop background processing
     */
    void stop_background_processing();
    
    /**
     * @brief Check if background processing is running
     * @return true if background processing is active
     */
    bool is_background_processing_running() const;

private:
    CacheHierarchyConfig config_;
    
    // Cache levels
    std::unique_ptr<WorkingSetCache> l1_cache_;      // Fastest, smallest
    std::unique_ptr<MemoryMappedCache> l2_cache_;    // Medium speed, medium size
    
    // L3 persistence callback (for writing to Parquet)
    L3PersistenceCallback on_l3_demotion_;
    
    // Background processing
    std::atomic<bool> background_running_{false};
    std::thread background_thread_;
    // Serialize hierarchy-level operations (including background maintenance) to avoid
    // races across multiple cache levels. This must be recursive because some public
    // methods call other public methods (e.g. get()->promote()).
    mutable std::recursive_mutex background_mutex_;
    
    // Global statistics
    mutable std::atomic<uint64_t> total_hits_{0};
    mutable std::atomic<uint64_t> total_misses_{0};
    mutable std::atomic<uint64_t> l1_hits_{0};
    mutable std::atomic<uint64_t> l2_hits_{0};
    mutable std::atomic<uint64_t> l3_hits_{0};
    mutable std::atomic<uint64_t> promotions_{0};
    mutable std::atomic<uint64_t> demotions_{0};
    
    /**
     * @brief Background processing loop for cache maintenance
     */
    void background_processing_loop();
    
    /**
     * @brief Perform cache maintenance tasks
     */
    void perform_maintenance();
    
    /**
     * @brief Update access metadata for a series
     * @param series_id The series ID that was accessed
     * @param cache_level The cache level where it was found (1, 2, or 3)
     */
    void update_access_metadata(core::SeriesID series_id, int cache_level);
    
    /**
     * @brief Check if a series should be promoted
     * @param series_id The series ID to check
     * @return true if the series should be promoted
     */
    bool should_promote(core::SeriesID series_id) const;
    
    /**
     * @brief Check if a series should be demoted
     * @param series_id The series ID to check
     * @return true if the series should be demoted
     */
    bool should_demote(core::SeriesID series_id) const;
    
    /**
     * @brief Calculate the size of a time series in bytes
     * @param series The time series to measure
     * @return Size in bytes
     */
    size_t calculate_series_size(const std::shared_ptr<core::TimeSeries>& series) const;
};

} // namespace storage
} // namespace tsdb 