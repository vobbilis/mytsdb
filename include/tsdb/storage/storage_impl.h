#ifndef TSDB_STORAGE_STORAGE_IMPL_H_
#define TSDB_STORAGE_STORAGE_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
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
    std::vector<core::TimeSeries> stored_series_;  // In-memory storage for series
    
    // Object pools for reducing memory allocations
    std::unique_ptr<TimeSeriesPool> time_series_pool_;
    std::unique_ptr<LabelsPool> labels_pool_;
    std::unique_ptr<SamplePool> sample_pool_;
    
    // Working set cache for frequently accessed data
    std::unique_ptr<WorkingSetCache> working_set_cache_;
};

} // namespace storage
} // namespace tsdb 

#endif  // TSDB_STORAGE_STORAGE_IMPL_H_ 