/**
 * @file sharded_storage_manager.h
 * @brief High-concurrency sharded storage manager for TSDB
 * 
 * This file implements a sharded storage manager that provides:
 * - Horizontal partitioning of data across multiple storage shards
 * - Write queue system for asynchronous processing
 * - Load balancing and failover capabilities
 * - High-concurrency write operations
 * 
 * Architecture:
 * 1. Multiple StorageImpl shards (configurable count)
 * 2. Write queue with background workers
 * 3. Shard selection based on series labels hash
 * 4. Batch processing for efficiency
 * 5. Graceful degradation and error handling
 */

#pragma once

#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/config.h"
#include "tsdb/storage/storage_impl.h"
#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <functional>

namespace tsdb {
namespace storage {

/**
 * @brief Configuration for sharded storage manager
 */
struct ShardedStorageConfig {
    size_t num_shards = 4;                    // Number of storage shards
    size_t queue_size = 10000;                // Write queue size per shard
    size_t batch_size = 100;                  // Batch size for processing
    size_t num_workers = 2;                   // Background workers per shard
    std::chrono::milliseconds flush_interval{100}; // Flush interval
    std::chrono::milliseconds retry_delay{10};     // Retry delay for failed writes
    size_t max_retries = 3;                   // Maximum retry attempts
    
    static ShardedStorageConfig Default() {
        return ShardedStorageConfig{};
    }
};

/**
 * @brief Write operation for the queue
 */
struct WriteOperation {
    core::TimeSeries series;
    std::function<void(const core::Result<void>&)> callback;
    std::chrono::steady_clock::time_point timestamp;
    size_t retry_count = 0;
    
    WriteOperation(const core::TimeSeries& s, std::function<void(const core::Result<void>&)> cb = nullptr)
        : series(s), callback(cb), timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Statistics for sharded storage
 */
struct ShardedStorageStats {
    std::atomic<size_t> total_writes{0};
    std::atomic<size_t> successful_writes{0};
    std::atomic<size_t> failed_writes{0};
    std::atomic<size_t> queued_writes{0};
    std::atomic<size_t> dropped_writes{0};
    std::atomic<size_t> retry_count{0};
    
    // Copy constructor
    ShardedStorageStats() = default;
    ShardedStorageStats(const ShardedStorageStats& other) {
        total_writes.store(other.total_writes.load());
        successful_writes.store(other.successful_writes.load());
        failed_writes.store(other.failed_writes.load());
        queued_writes.store(other.queued_writes.load());
        dropped_writes.store(other.dropped_writes.load());
        retry_count.store(other.retry_count.load());
    }
    
    // Assignment operator
    ShardedStorageStats& operator=(const ShardedStorageStats& other) {
        if (this != &other) {
            total_writes.store(other.total_writes.load());
            successful_writes.store(other.successful_writes.load());
            failed_writes.store(other.failed_writes.load());
            queued_writes.store(other.queued_writes.load());
            dropped_writes.store(other.dropped_writes.load());
            retry_count.store(other.retry_count.load());
        }
        return *this;
    }
    
    double get_success_rate() const {
        auto total = total_writes.load();
        return total > 0 ? (double)successful_writes.load() / total : 0.0;
    }
    
    double get_queue_utilization() const {
        return (double)queued_writes.load() / 10000.0; // Assuming max queue size
    }
};

/**
 * @brief High-concurrency sharded storage manager
 * 
 * This class provides:
 * - Multiple storage shards for horizontal scaling
 * - Write queue system for asynchronous processing
 * - Background workers for batch processing
 * - Load balancing across shards
 * - Comprehensive error handling and retry logic
 */
class ShardedStorageManager {
public:
    explicit ShardedStorageManager(const ShardedStorageConfig& config = ShardedStorageConfig::Default());
    ~ShardedStorageManager();
    
    // Core operations
    core::Result<void> init(const core::StorageConfig& config);
    core::Result<void> write(const core::TimeSeries& series, 
                           std::function<void(const core::Result<void>&)> callback = nullptr);
    core::Result<core::TimeSeries> read(const core::Labels& labels, 
                                      int64_t start_time, int64_t end_time);
    core::Result<void> flush();
    core::Result<void> close();
    
    // Statistics and monitoring
    ShardedStorageStats get_stats() const;
    std::string get_stats_string() const;
    
    // Configuration
    void set_config(const ShardedStorageConfig& config);
    ShardedStorageConfig get_config() const;
    
    // Health and diagnostics
    bool is_healthy() const;
    std::vector<bool> get_shard_health() const;
    
private:
    // Shard management
    size_t get_shard_id(const core::TimeSeries& series) const;
    size_t get_shard_id(const core::Labels& labels) const;
    std::shared_ptr<StorageImpl> get_shard(size_t shard_id);
    
    // Write queue management
    bool enqueue_write(size_t shard_id, WriteOperation&& op);
    void process_write_queue(size_t shard_id);
    void flush_shard_queue(size_t shard_id, bool force = false);
    
    // Background workers
    void start_workers();
    void stop_workers();
    void worker_thread(size_t shard_id);
    
    // Error handling and retry
    void handle_write_error(size_t shard_id, WriteOperation& op, const core::Result<void>& result);
    bool should_retry(const WriteOperation& op) const;
    
    // Configuration
    ShardedStorageConfig config_;
    core::StorageConfig storage_config_;
    
    // Shards
    std::vector<std::shared_ptr<StorageImpl>> shards_;
    std::vector<std::atomic<bool>> shard_health_;
    
    // Write queues and workers
    std::vector<std::queue<WriteOperation>> write_queues_;
    std::vector<std::mutex> queue_mutexes_;
    std::vector<std::condition_variable> queue_cvs_;
    std::vector<std::vector<std::thread>> workers_;
    
    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    // Statistics
    mutable ShardedStorageStats stats_;
    
    // Thread safety
    mutable std::mutex config_mutex_;
};

} // namespace storage
} // namespace tsdb
