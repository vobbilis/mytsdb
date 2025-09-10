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
    StorageImpl();
    explicit StorageImpl(const core::StorageConfig& config);
    ~StorageImpl() override;
    
    // Implement pure virtual methods from Storage interface
    core::Result<void> init(const core::StorageConfig& config) override;
    core::Result<void> write(const core::TimeSeries& series) override;
    core::Result<core::TimeSeries> read(
        const core::Labels& labels,
        int64_t start_time,
        int64_t end_time) override;
    core::Result<std::vector<core::TimeSeries>> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time,
        int64_t end_time) override;
    core::Result<std::vector<std::string>> label_names() override;
    core::Result<std::vector<std::string>> label_values(
        const std::string& label_name) override;
    core::Result<void> delete_series(
        const std::vector<std::pair<std::string, std::string>>& matchers) override;
    core::Result<void> compact() override;
    core::Result<void> flush() override;
    core::Result<void> close() override;
    std::string stats() const override;

private:
    mutable std::shared_mutex mutex_;
    std::shared_ptr<BlockManager> block_manager_;
    bool initialized_;
    core::StorageConfig config_;  // Storage configuration
    std::vector<core::TimeSeries> stored_series_;  // In-memory storage for series
    std::vector<std::vector<uint8_t>> compressed_data_;  // Compressed data storage
    
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
    
    // Helper methods for compression integration
    std::vector<uint8_t> compress_series_data(const core::TimeSeries& series);
    core::TimeSeries decompress_series_data(const std::vector<uint8_t>& compressed_data);
    void extract_series_data(const core::TimeSeries& series, 
                           std::vector<int64_t>& timestamps, 
                           std::vector<double>& values);
    void initialize_compressors();
    
    // Helper methods for background processing integration
    void initialize_background_processor();
    void schedule_background_compaction();
    void schedule_background_cleanup();
    void schedule_background_metrics_collection();
    core::Result<void> execute_background_compaction();
    core::Result<void> execute_background_cleanup();
    core::Result<void> execute_background_metrics_collection();
    
    // Helper methods for predictive caching integration
    void initialize_predictive_cache();
    void record_access_pattern(const core::Labels& labels);
    void prefetch_predicted_series(core::SeriesID current_series);
    std::vector<core::SeriesID> get_prefetch_candidates(core::SeriesID current_series);
};

} // namespace storage
} // namespace tsdb 

#endif  // TSDB_STORAGE_STORAGE_IMPL_H_ 