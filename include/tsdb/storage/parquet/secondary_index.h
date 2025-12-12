#ifndef TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H
#define TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H

#include "tsdb/core/types.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <optional>

namespace tsdb {
namespace storage {
namespace parquet {

/**
 * @brief Location of a series within a Parquet file
 * 
 * This struct represents the physical location of a time series
 * within the Parquet file, enabling O(1) lookup after index scan.
 */
struct RowLocation {
    int32_t row_group_id;      // Which row group contains this series
    int64_t row_offset;        // Row offset within the row group (for future optimization)
    int64_t min_timestamp;     // Min timestamp in this chunk (for time filtering)
    int64_t max_timestamp;     // Max timestamp in this chunk (for time filtering)
    
    RowLocation() 
        : row_group_id(-1), row_offset(0), min_timestamp(0), max_timestamp(0) {}
    
    RowLocation(int32_t rg_id, int64_t offset, int64_t min_ts, int64_t max_ts)
        : row_group_id(rg_id), row_offset(offset), 
          min_timestamp(min_ts), max_timestamp(max_ts) {}
    
    bool is_valid() const { return row_group_id >= 0; }
};

/**
 * @brief Secondary Index for Parquet files
 * 
 * This class provides O(1) average-case lookup from SeriesID to RowLocation,
 * dramatically improving cold storage read performance by avoiding
 * full file scans.
 * 
 * Architecture:
 * - Uses std::unordered_map as the core index structure
 * - Supports multiple row locations per series (series spanning row groups)
 * - Thread-safe with read-write locking
 * - Persists index to sidecar file for fast startup
 * 
 * Performance:
 * - Lookup: O(1) average-case where n = number of unique series
 * - Insert: O(1) average-case
 * - Space: O(n * sizeof(RowLocation))
 * 
 * Usage:
 *   SecondaryIndex index;
 *   index.BuildFromParquetFile("data.parquet");
 *   auto locations = index.Lookup(series_id);
 *   // Now read only the specific row groups
 */
class SecondaryIndex {
public:
    SecondaryIndex();
    ~SecondaryIndex();
    
    // Disable copy (index can be large)
    SecondaryIndex(const SecondaryIndex&) = delete;
    SecondaryIndex& operator=(const SecondaryIndex&) = delete;
    
    // Move operations
    SecondaryIndex(SecondaryIndex&&) noexcept;
    SecondaryIndex& operator=(SecondaryIndex&&) noexcept;
    
    /**
     * @brief Build index by scanning a Parquet file
     * 
     * Scans all row groups in the Parquet file and builds the
     * SeriesID -> RowLocation mapping. This is done once when
     * the file is first opened.
     * 
     * @param parquet_path Path to the Parquet file
     * @return true on success, false on failure
     */
    bool BuildFromParquetFile(const std::string& parquet_path);
    
    /**
     * @brief Load index from persisted sidecar file
     * 
     * Loads a previously saved index from disk. This is much
     * faster than rebuilding from the Parquet file.
     * 
     * @param index_path Path to the index sidecar file
     * @return true on success, false if file doesn't exist or is corrupt
     */
    bool LoadFromFile(const std::string& index_path);
    
    /**
     * @brief Save index to sidecar file
     * 
     * Persists the index to disk for fast loading on restart.
     * File format: simple binary with header + entries.
     * 
     * @param index_path Path to save the index
     * @return true on success, false on failure
     */
    bool SaveToFile(const std::string& index_path) const;
    
    /**
     * @brief Look up row locations for a series
     * 
     * O(log n) lookup to find all row groups containing data
     * for the specified series.
     * 
     * @param series_id The series identifier (hash of labels)
     * @return Vector of RowLocations, empty if series not found
     */
    std::vector<RowLocation> Lookup(core::SeriesID series_id) const;
    
    /**
     * @brief Look up row locations for a series within time range
     * 
     * O(log n) lookup with time-based filtering. Returns only
     * row locations that overlap with the specified time range.
     * 
     * @param series_id The series identifier
     * @param start_time Query start time (inclusive)
     * @param end_time Query end time (inclusive)
     * @return Vector of RowLocations overlapping time range
     */
    std::vector<RowLocation> LookupInTimeRange(
        core::SeriesID series_id,
        int64_t start_time,
        int64_t end_time) const;
    
    /**
     * @brief Add an entry to the index
     * 
     * @param series_id The series identifier
     * @param location The row location for this series
     */
    void Insert(core::SeriesID series_id, const RowLocation& location);
    
    /**
     * @brief Check if the index contains a series
     * 
     * @param series_id The series identifier
     * @return true if series exists in index
     */
    bool Contains(core::SeriesID series_id) const;
    
    /**
     * @brief Get the number of unique series in the index
     */
    size_t Size() const;
    
    /**
     * @brief Get the total number of row locations (>= Size())
     */
    size_t TotalLocations() const;
    
    /**
     * @brief Clear all entries from the index
     */
    void Clear();
    
    /**
     * @brief Check if index is empty
     */
    bool Empty() const;
    
    /**
     * @brief Get all series IDs in the index
     * 
     * Useful for debugging and iteration.
     */
    std::vector<core::SeriesID> GetAllSeriesIDs() const;
    
    /**
     * @brief Get index statistics for monitoring
     */
    struct IndexStats {
        size_t num_series;           // Unique series count
        size_t num_locations;        // Total row locations
        size_t memory_bytes;         // Estimated memory usage
        int64_t build_time_us;       // Time to build/load index
        std::string source_file;     // Parquet file path
    };
    IndexStats GetStats() const;

private:
    // Hash index: Key: SeriesID, Value: vector of RowLocations (series can span row groups)
    std::unordered_map<core::SeriesID, std::vector<RowLocation>> index_;
    
    // Thread safety
    mutable std::shared_mutex mutex_;
    
    // Statistics
    mutable IndexStats stats_;
    
    // Helper to compute SeriesID from labels
    static core::SeriesID ComputeSeriesID(const std::string& labels_str);
    
    // Index file magic number and version for validation
    static constexpr uint32_t INDEX_MAGIC = 0x54534458;  // "TSDX"
    static constexpr uint32_t INDEX_VERSION = 1;
};

/**
 * @brief Global index cache for Parquet files
 * 
 * Maintains a cache of SecondaryIndex objects for all open
 * Parquet files. Indices are built on first access and
 * cached for subsequent queries.
 */
class SecondaryIndexCache {
public:
    static SecondaryIndexCache& Instance();
    
    /**
     * @brief Get or create index for a Parquet file
     * 
     * Returns cached index if available, otherwise builds
     * a new index and caches it.
     * 
     * @param parquet_path Path to the Parquet file
     * @return Shared pointer to the index, nullptr on error
     */
    std::shared_ptr<SecondaryIndex> GetOrCreate(const std::string& parquet_path);
    
    /**
     * @brief Invalidate cached index for a file
     * 
     * Call this when a Parquet file is modified or deleted.
     */
    void Invalidate(const std::string& parquet_path);
    
    /**
     * @brief Clear all cached indices
     */
    void ClearAll();
    
    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t num_cached_indices;
        size_t total_memory_bytes;
        size_t cache_hits;
        size_t cache_misses;
    };
    CacheStats GetStats() const;

private:
    SecondaryIndexCache() = default;
    
    std::unordered_map<std::string, std::shared_ptr<SecondaryIndex>> cache_;
    mutable std::mutex mutex_;
    
    // Stats
    mutable size_t hits_ = 0;
    mutable size_t misses_ = 0;
};

}  // namespace parquet
}  // namespace storage
}  // namespace tsdb

#endif  // TSDB_STORAGE_PARQUET_SECONDARY_INDEX_H
