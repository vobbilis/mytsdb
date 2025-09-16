/**
 * @file high_concurrency_storage.h
 * @brief High-concurrency storage interface that combines sharding and write queues
 */

#pragma once

#include "tsdb/storage/storage.h"
#include "tsdb/storage/sharded_storage_manager.h"
#include <memory>

namespace tsdb {
namespace storage {

/**
 * @brief High-concurrency storage implementation
 * 
 * This class provides a high-concurrency storage interface that:
 * - Uses sharded storage for horizontal scaling
 * - Implements write queues for asynchronous processing
 * - Provides load balancing across shards
 * - Handles high-concurrency workloads efficiently
 */
class HighConcurrencyStorage : public Storage {
public:
    explicit HighConcurrencyStorage(const ShardedStorageConfig& config = ShardedStorageConfig::Default());
    ~HighConcurrencyStorage() override = default;
    
    // Core Storage interface
    core::Result<void> init(const core::StorageConfig& config) override;
    core::Result<void> write(const core::TimeSeries& series) override;
    core::Result<core::TimeSeries> read(const core::Labels& labels, 
                                      int64_t start_time, int64_t end_time) override;
    core::Result<std::vector<core::TimeSeries>> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time, int64_t end_time) override;
    core::Result<std::vector<std::string>> label_names() override;
    core::Result<std::vector<std::string>> label_values(const std::string& label_name) override;
    core::Result<void> delete_series(const std::vector<std::pair<std::string, std::string>>& matchers) override;
    core::Result<void> compact() override;
    core::Result<void> flush() override;
    core::Result<void> close() override;
    std::string stats() const override;
    
    // High-concurrency specific methods
    core::Result<void> write_async(const core::TimeSeries& series, 
                                 std::function<void(const core::Result<void>&)> callback = nullptr);
    ShardedStorageStats get_detailed_stats() const;
    bool is_healthy() const;
    
private:
    std::unique_ptr<ShardedStorageManager> sharded_manager_;
    ShardedStorageConfig config_;
};

} // namespace storage
} // namespace tsdb
