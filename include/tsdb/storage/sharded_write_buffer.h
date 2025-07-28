#pragma once

#include "tsdb/storage/storage.h"
#include "tsdb/storage/object_pool.h"
#include "tsdb/storage/performance_config.h"
#include "tsdb/core/result.h"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace tsdb {
namespace storage {

/**
 * @brief Configuration for sharded write buffer
 */
struct ShardedWriteBufferConfig {
    uint32_t num_shards = 16;                    // Number of shards (power of 2 recommended)
    size_t buffer_size_per_shard = 1024 * 1024;  // 1MB per shard default
    uint32_t flush_interval_ms = 1000;           // Flush every 1 second
    uint32_t max_flush_workers = 4;              // Maximum background flush workers
    bool enable_compression = true;              // Enable compression during flush
    bool enable_metrics = true;                  // Enable performance metrics
    double load_balance_threshold = 0.2;         // 20% load imbalance threshold
    uint32_t retry_attempts = 3;                 // Number of retry attempts on flush failure
    std::chrono::milliseconds retry_delay{100};  // Delay between retries
};

/**
 * @brief Write operation metadata
 */
struct WriteOperation {
    core::TimeSeries series;
    std::chrono::system_clock::time_point queued_time;
    uint32_t retry_count = 0;
    std::function<void(core::Result<void>)> callback;
    
    WriteOperation() = default;
    WriteOperation(const core::TimeSeries& ts, std::function<void(core::Result<void>)> cb = nullptr)
        : series(ts), queued_time(std::chrono::system_clock::now()), callback(cb) {}
};

/**
 * @brief Individual shard buffer
 */
class ShardBuffer {
public:
    explicit ShardBuffer(size_t max_size, uint32_t shard_id);
    
    /**
     * @brief Add write operation to shard buffer
     * @param op Write operation to add
     * @return True if added successfully, false if buffer full
     */
    bool addWrite(const WriteOperation& op);
    
    /**
     * @brief Get all pending writes and clear buffer
     * @return Vector of pending write operations
     */
    std::vector<WriteOperation> flush();
    
    /**
     * @brief Get current buffer size
     * @return Number of operations in buffer
     */
    size_t size() const { return operations_.size(); }
    
    /**
     * @brief Get buffer capacity
     * @return Maximum number of operations
     */
    size_t capacity() const { return max_size_; }
    
    /**
     * @brief Check if buffer is full
     * @return True if buffer is at capacity
     */
    bool isFull() const { return operations_.size() >= max_size_; }
    
    /**
     * @brief Get buffer utilization percentage
     * @return Utilization as percentage (0-100)
     */
    double utilization() const { return (static_cast<double>(operations_.size()) / max_size_) * 100.0; }
    
    /**
     * @brief Get shard ID
     * @return Shard identifier
     */
    uint32_t getShardId() const { return shard_id_; }
    
    /**
     * @brief Get last flush time
     * @return Time point of last flush
     */
    std::chrono::system_clock::time_point getLastFlushTime() const { return last_flush_time_; }

private:
    uint32_t shard_id_;
    size_t max_size_;
    std::vector<WriteOperation> operations_;
    mutable std::mutex buffer_mutex_;
    std::chrono::system_clock::time_point last_flush_time_;
};

/**
 * @brief Sharded write buffer for high-throughput writes
 * 
 * This class implements a sharded write buffer that distributes writes across
 * multiple shards to reduce contention and improve write throughput. Each shard
 * has its own buffer and can be flushed independently.
 */
class ShardedWriteBuffer {
public:
    explicit ShardedWriteBuffer(const ShardedWriteBufferConfig& config = ShardedWriteBufferConfig{});
    ~ShardedWriteBuffer();
    
    // Disable copy constructor and assignment
    ShardedWriteBuffer(const ShardedWriteBuffer&) = delete;
    ShardedWriteBuffer& operator=(const ShardedWriteBuffer&) = delete;
    
    /**
     * @brief Initialize the sharded write buffer
     * @param storage Storage implementation to use for flushing
     * @return Result indicating success or failure
     */
    core::Result<void> initialize(std::shared_ptr<Storage> storage);
    
    /**
     * @brief Write time series data
     * @param series Time series to write
     * @param callback Optional callback for completion notification
     * @return Result indicating success or failure
     */
    core::Result<void> write(const core::TimeSeries& series,
                            std::function<void(core::Result<void>)> callback = nullptr);
    
    /**
     * @brief Flush all shards
     * @param force Force flush even if buffers are not full
     * @return Result indicating success or failure
     */
    core::Result<void> flush(bool force = false);
    
    /**
     * @brief Flush specific shard
     * @param shard_id Shard to flush
     * @param force Force flush even if buffer is not full
     * @return Result indicating success or failure
     */
    core::Result<void> flushShard(uint32_t shard_id, bool force = false);
    
    /**
     * @brief Stop the write buffer and flush all pending writes
     * @return Result indicating success or failure
     */
    core::Result<void> shutdown();
    
    /**
     * @brief Get buffer statistics
     * @return Statistics about buffer usage and performance
     */
    struct BufferStats {
        uint32_t total_shards;
        uint32_t active_shards;
        size_t total_operations;
        size_t total_bytes;
        double avg_utilization;
        double max_utilization;
        uint64_t total_flushes;
        uint64_t failed_flushes;
        std::chrono::milliseconds avg_flush_time;
        std::chrono::milliseconds max_flush_time;
        uint64_t total_writes;
        uint64_t dropped_writes;
        double write_throughput;  // operations per second
    };
    
    BufferStats getStats() const;
    
    /**
     * @brief Get shard-specific statistics
     * @param shard_id Shard identifier
     * @return Statistics for specific shard
     */
    struct ShardStats {
        uint32_t shard_id;
        size_t operations;
        size_t bytes;
        double utilization;
        uint64_t flushes;
        uint64_t failed_flushes;
        std::chrono::milliseconds avg_flush_time;
        std::chrono::system_clock::time_point last_flush_time;
    };
    
    ShardStats getShardStats(uint32_t shard_id) const;
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    const ShardedWriteBufferConfig& getConfig() const { return config_; }
    
    /**
     * @brief Update configuration
     * @param new_config New configuration
     * @return Result indicating success or failure
     */
    core::Result<void> updateConfig(const ShardedWriteBufferConfig& new_config);
    
    /**
     * @brief Check if buffer is healthy
     * @return True if buffer is operating normally
     */
    bool isHealthy() const;
    
    /**
     * @brief Get load balance information
     * @return Load balance statistics
     */
    struct LoadBalanceInfo {
        double imbalance_ratio;
        uint32_t most_loaded_shard;
        uint32_t least_loaded_shard;
        double std_deviation;
        bool needs_rebalancing;
    };
    
    LoadBalanceInfo getLoadBalanceInfo() const;

private:
    ShardedWriteBufferConfig config_;
    std::shared_ptr<Storage> storage_;
    std::vector<std::unique_ptr<ShardBuffer>> shards_;
    std::vector<std::thread> flush_workers_;
    std::queue<uint32_t> flush_queue_;
    std::mutex flush_queue_mutex_;
    std::condition_variable flush_condition_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> initialized_{false};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<uint64_t> total_writes_{0};
    std::atomic<uint64_t> dropped_writes_{0};
    std::atomic<uint64_t> total_flushes_{0};
    std::atomic<uint64_t> failed_flushes_{0};
    std::chrono::system_clock::time_point start_time_;
    
    /**
     * @brief Calculate shard ID for a series
     * @param series_id Series identifier
     * @return Shard ID (0 to num_shards-1)
     */
    uint32_t getShardId(const std::string& series_id) const;
    
    /**
     * @brief Background flush worker function
     */
    void flushWorker();
    
    /**
     * @brief Process flush queue
     */
    void processFlushQueue();
    
    /**
     * @brief Flush shard with retry logic
     * @param shard_id Shard to flush
     * @return Result indicating success or failure
     */
    core::Result<void> flushShardWithRetry(uint32_t shard_id);
    
    /**
     * @brief Start background flush workers
     */
    void startFlushWorkers();
    
    /**
     * @brief Stop background flush workers
     */
    void stopFlushWorkers();
    
    /**
     * @brief Check if rebalancing is needed
     * @return True if rebalancing should be performed
     */
    bool needsRebalancing() const;
    
    /**
     * @brief Perform load rebalancing
     */
    void rebalance();
    
    /**
     * @brief Update statistics
     */
    void updateStats();
    
    /**
     * @brief Hash function for consistent shard assignment
     * @param series_id Series identifier
     * @return Hash value
     */
    uint32_t hashSeriesId(const std::string& series_id) const;
    
    /**
     * @brief Flush operations to storage
     * @param operations Operations to flush
     * @return Result indicating success or failure
     */
    core::Result<void> flushOperationsToStorage(const std::vector<WriteOperation>& operations);
};

/**
 * @brief Factory for creating sharded write buffers
 */
class ShardedWriteBufferFactory {
public:
    /**
     * @brief Create a sharded write buffer with default configuration
     * @return Shared pointer to sharded write buffer
     */
    static std::shared_ptr<ShardedWriteBuffer> create();
    
    /**
     * @brief Create a sharded write buffer with custom configuration
     * @param config Configuration for the buffer
     * @return Shared pointer to sharded write buffer
     */
    static std::shared_ptr<ShardedWriteBuffer> create(const ShardedWriteBufferConfig& config);
    
    /**
     * @brief Create a sharded write buffer optimized for specific workload
     * @param expected_throughput Expected write throughput (ops/sec)
     * @param expected_latency Expected write latency (ms)
     * @param available_memory Available memory for buffers (bytes)
     * @return Shared pointer to optimized sharded write buffer
     */
    static std::shared_ptr<ShardedWriteBuffer> createOptimized(uint32_t expected_throughput,
                                                              uint32_t expected_latency,
                                                              size_t available_memory);
};

} // namespace storage
} // namespace tsdb 