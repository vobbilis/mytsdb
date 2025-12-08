#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>
#include <parquet/bloom_filter.h>
#include <parquet/bloom_filter_reader.h>
#include <arrow/io/file.h>
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {
namespace parquet {

/**
 * @brief Manages Bloom filters for Parquet files
 * 
 * Architecture:
 * - Phase 1 (Bloom Filter): Quick "definitely not present" check - O(1)
 * - Phase 2 (B+ Tree): Precise row group location lookup - O(log n)
 * 
 * The Bloom filter is stored alongside each Parquet file as a .bloom file.
 * It contains series_id hashes to quickly determine if a series MIGHT be
 * present in the file before doing the more expensive B+ Tree lookup.
 */
class BloomFilterManager {
public:
    // Bloom filter parameters
    // NDV = Number of Distinct Values (estimated series per file)
    // FPP = False Positive Probability
    static constexpr uint32_t DEFAULT_NDV = 100000;  // 100K distinct series
    static constexpr double DEFAULT_FPP = 0.01;      // 1% false positive rate

    BloomFilterManager() = default;
    ~BloomFilterManager() = default;

    // =========================================================================
    // WRITE PATH: Build and save Bloom filter during Parquet write
    // =========================================================================
    
    /**
     * @brief Create a new Bloom filter for writing
     * @param estimated_entries Expected number of unique series in this file
     * @param fpp False positive probability (default 1%)
     */
    void CreateFilter(uint32_t estimated_entries = DEFAULT_NDV, 
                      double fpp = DEFAULT_FPP);
    
    /**
     * @brief Add a series_id to the Bloom filter
     * @param series_id Hash of the series labels
     */
    void AddSeriesId(core::SeriesID series_id);
    
    /**
     * @brief Add series by computing hash from labels string
     * @param labels_str Canonical label string (e.g., "name=cpu,pod=p1")
     */
    void AddSeriesByLabels(const std::string& labels_str);
    
    /**
     * @brief Save Bloom filter to disk alongside Parquet file
     * @param parquet_path Path to the Parquet file
     * @return true if successful
     */
    bool SaveFilter(const std::string& parquet_path);

    // =========================================================================
    // READ PATH: Load and query Bloom filter
    // =========================================================================
    
    /**
     * @brief Load Bloom filter from disk
     * @param parquet_path Path to the Parquet file (will load .bloom file)
     * @return true if filter was loaded successfully
     */
    bool LoadFilter(const std::string& parquet_path);
    
    /**
     * @brief Check if series MIGHT be present (Phase 1 filtering)
     * @param series_id Hash of the series labels
     * @return true if series might be present (proceed to B+ Tree lookup)
     *         false if series is DEFINITELY NOT present (skip this file)
     */
    bool MightContain(core::SeriesID series_id) const;
    
    /**
     * @brief Check by labels string
     * @param labels_str Canonical label string
     * @return true if might be present, false if definitely not
     */
    bool MightContainLabels(const std::string& labels_str) const;
    
    /**
     * @brief Get statistics about the filter
     */
    size_t GetFilterSizeBytes() const;
    size_t GetEntriesAdded() const { return entries_added_; }
    
    /**
     * @brief Check if filter is loaded/valid
     */
    bool IsValid() const { return filter_ != nullptr; }

    // =========================================================================
    // UTILITY
    // =========================================================================
    
    /**
     * @brief Get the .bloom file path for a Parquet file
     */
    static std::string GetBloomPath(const std::string& parquet_path);
    
    /**
     * @brief Compute series_id from labels string (same hash as used elsewhere)
     */
    static core::SeriesID ComputeSeriesId(const std::string& labels_str);

private:
    std::unique_ptr<::parquet::BlockSplitBloomFilter> filter_;
    size_t entries_added_ = 0;
    mutable std::mutex mutex_;
};

/**
 * @brief Global cache for Bloom filters (one per Parquet file)
 * 
 * This prevents reloading filters from disk on every query.
 */
class BloomFilterCache {
public:
    static BloomFilterCache& Instance();
    
    /**
     * @brief Get or load Bloom filter for a Parquet file
     * @return Shared pointer to filter, or nullptr if no filter exists
     */
    std::shared_ptr<BloomFilterManager> GetOrLoad(const std::string& parquet_path);
    
    /**
     * @brief Invalidate cached filter (e.g., when file is rewritten)
     */
    void Invalidate(const std::string& parquet_path);
    
    /**
     * @brief Clear all cached filters
     */
    void Clear();
    
    /**
     * @brief Get cache statistics
     */
    size_t Size() const;
    size_t TotalMemoryBytes() const;

private:
    BloomFilterCache() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<BloomFilterManager>> cache_;
};

}  // namespace parquet
}  // namespace storage
}  // namespace tsdb
