#ifndef TSDB_STORAGE_STORAGE_IMPL_H_
#define TSDB_STORAGE_STORAGE_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <shared_mutex>
#include <atomic>

#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include "tsdb/storage/block_manager.h"
#include "tsdb/storage/granularity.h"
#include "tsdb/storage/storage.h"
#include "tsdb/core/config.h"
#include "tsdb/core/error.h"
#include "tsdb/storage/object_pool.h"
#include "tsdb/storage/working_set_cache.h"
#include "tsdb/storage/cache_hierarchy.h"
#include "tsdb/storage/cache_types.h"
#include "tsdb/storage/compression.h"
#include "tsdb/storage/background_processor.h"
#include "tsdb/storage/predictive_cache.h"
#include "tsdb/storage/internal/block_internal.h"
#include "tsdb/storage/internal/block_impl.h"

namespace tsdb {
namespace storage {

// Forward declaration
class Series;
    
    /**
 * @brief Implementation of the Storage interface
 */
class StorageImpl : public Storage {
public:
    /**
     * @brief Constructs a new StorageImpl instance
     */
    explicit StorageImpl(const core::StorageConfig& config);
    StorageImpl();

    /**
     * @brief Destructor that ensures proper cleanup of storage resources
     */
    ~StorageImpl() override;
    
    // Implement pure virtual methods from Storage interface
    /**
     * @brief Initializes the storage with the given configuration
     * @param config The storage configuration to use
     * @note This method can only be called once. Subsequent calls will return an error.
     */
    core::Result<void> init(const core::StorageConfig& config) override;

    /**
     * @brief Writes a time series to storage
     * @param series The time series data to write
     * @return Result object indicating success or failure
     */
    core::Result<void> write(const core::TimeSeries& series) override;

    /**
     * @brief Reads a time series from storage
     * @param labels The labels identifying the time series
     * @param start_time The start time for the query range
     * @param end_time The end time for the query range
     * @return Result object containing the requested time series data
     */
    core::Result<core::TimeSeries> read(
        const core::Labels& labels,
        int64_t start_time,
        int64_t end_time) override;

    /**
     * @brief Queries time series data based on label matchers
     * @param matchers The label matchers for selecting time series
     * @param start_time The start time for the query range
     * @param end_time The end time for the query range
     * @return Result object containing the matching time series data
     */
    core::Result<std::vector<core::TimeSeries>> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time,
        int64_t end_time) override;

    /**
     * @brief Retrieves the available label names in the storage
     * @return Result object containing the list of label names
     */
    core::Result<std::vector<std::string>> label_names() override;

    /**
     * @brief Retrieves the values for a specific label
     * @param label_name The name of the label
     * @return Result object containing the list of label values
     */
    core::Result<std::vector<std::string>> label_values(
        const std::string& label_name) override;

    /**
     * @brief Deletes time series data matching the given label matchers
     * @param matchers The label matchers for selecting time series to delete
     * @return Result object indicating success or failure
     */
    core::Result<void> delete_series(
        const std::vector<std::pair<std::string, std::string>>& matchers) override;

    /**
     * @brief Compacts the storage to optimize space and performance
     * @return Result object indicating success or failure
     */
    core::Result<void> compact() override;

    /**
     * @brief Flushes any pending changes to the storage
     * @return Result object indicating success or failure
     */
    core::Result<void> flush() override;

    /**
     * @brief Closes the storage and releases any resources
     * @return Result object indicating success or failure
     */
    core::Result<void> close() override;

    /**
     * @brief Returns statistics about the storage
     * @return A string containing storage statistics
     */
    std::string stats() const override;

private:
    core::Result<void> flush_nolock();
    // Thread safety
    mutable std::shared_mutex mutex_;  // Main mutex for concurrent access
    std::shared_ptr<BlockManager> block_manager_;
    std::atomic<bool> initialized_;
    core::StorageConfig config_;

    // In-memory storage for backward compatibility
    std::vector<core::TimeSeries> stored_series_;
    std::vector<std::vector<uint8_t>> compressed_data_;
    
    // Object pools for reducing memory allocations
    std::unique_ptr<TimeSeriesPool> time_series_pool_;
    std::unique_ptr<LabelsPool> labels_pool_;
    std::unique_ptr<SamplePool> sample_pool_;
    
    // Working set cache for frequently accessed data
    std::unique_ptr<WorkingSetCache> working_set_cache_;
    
    // Cache hierarchy for multi-level caching
    std::unique_ptr<CacheHierarchy> cache_hierarchy_;
    
    // Compression components
    std::unique_ptr<internal::TimestampCompressor> timestamp_compressor_;
    std::unique_ptr<internal::ValueCompressor> value_compressor_;
    std::unique_ptr<internal::LabelCompressor> label_compressor_;
    std::unique_ptr<internal::CompressorFactory> compressor_factory_;
    
    // Background processing components
    std::unique_ptr<BackgroundProcessor> background_processor_;
    
    // Predictive caching components
    std::unique_ptr<PredictiveCache> predictive_cache_;
    
    // Block management components
    std::shared_ptr<internal::BlockInternal> current_block_;
    std::map<core::SeriesID, std::vector<std::shared_ptr<internal::BlockInternal>>> series_blocks_;
    std::atomic<uint64_t> next_block_id_;
    
    // Block indexing for fast lookups
    std::map<core::Labels, std::vector<std::shared_ptr<internal::BlockInternal>>> label_to_blocks_;
    std::map<std::shared_ptr<internal::BlockInternal>, std::set<core::SeriesID>> block_to_series_;
    size_t total_blocks_created_;
    
    // Block lifecycle management
    void initialize_block_management();
    std::shared_ptr<internal::BlockInternal> create_new_block();
    core::Result<void> rotate_block_if_needed();
    core::Result<void> finalize_current_block();
    bool should_rotate_block() const;
    
    // Block-based operations
    core::Result<void> write_to_block(const core::TimeSeries& series);
    core::Result<core::TimeSeries> read_from_blocks(const core::Labels& labels, 
                                                   int64_t start_time, int64_t end_time);
    
    // Block compaction and indexing
    core::Result<void> check_and_trigger_compaction();
    core::Result<void> update_block_index(const core::TimeSeries& series, 
                                         std::shared_ptr<internal::BlockInternal> block);
    
    // Helper methods for cache integration
    core::SeriesID calculate_series_id(const core::Labels& labels) const;
    void filter_series_to_time_range(const core::TimeSeries& source, 
                                     int64_t start_time, int64_t end_time, 
                                     core::TimeSeries& result) const;
    void record_access_pattern(const core::Labels& labels);
    void prefetch_predicted_series(core::SeriesID current_series);
    std::vector<core::SeriesID> get_prefetch_candidates(core::SeriesID current_series);

    // Helper methods for compression integration
    void initialize_compressors();
    void extract_series_data(const core::TimeSeries& series,
                             std::vector<int64_t>& timestamps,
                             std::vector<double>& values);
    std::vector<uint8_t> compress_series_data(const core::TimeSeries& series);
    core::TimeSeries decompress_series_data(const std::vector<uint8_t>& compressed_data);

    // Background processing helpers
    void initialize_background_processor();
    void schedule_background_compaction();
    void schedule_background_cleanup();
    void schedule_background_metrics_collection();
    core::Result<void> execute_background_compaction();
    core::Result<void> execute_background_cleanup();
    core::Result<void> execute_background_metrics_collection();
    
    // Helper methods for predictive caching integration
    void initialize_predictive_cache();
};

} // namespace storage
} // namespace tsdb 

#endif  // TSDB_STORAGE_STORAGE_IMPL_H_