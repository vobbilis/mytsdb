#include "tsdb/storage/sharded_write_buffer.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/storage/performance_config.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace tsdb {
namespace storage {

// ShardBuffer implementation
ShardBuffer::ShardBuffer(size_t max_size, uint32_t shard_id)
    : shard_id_(shard_id), max_size_(max_size), last_flush_time_(std::chrono::system_clock::now()) {
}

bool ShardBuffer::addWrite(const WriteOperation& op) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (operations_.size() >= max_size_) {
        return false;  // Buffer full
    }
    
    operations_.push_back(op);
    return true;
}

std::vector<WriteOperation> ShardBuffer::flush() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    std::vector<WriteOperation> flushed_operations;
    flushed_operations.swap(operations_);
    
    last_flush_time_ = std::chrono::system_clock::now();
    return flushed_operations;
}

// ShardedWriteBuffer implementation
ShardedWriteBuffer::ShardedWriteBuffer(const ShardedWriteBufferConfig& config)
    : config_(config), start_time_(std::chrono::system_clock::now()) {
    
    // Validate configuration
    if (config_.num_shards == 0) {
        config_.num_shards = 16;  // Default to 16 shards
    }
    
    if (config_.buffer_size_per_shard == 0) {
        config_.buffer_size_per_shard = 1024 * 1024;  // Default to 1MB
    }
    
    // Initialize shards
    shards_.reserve(config_.num_shards);
    for (uint32_t i = 0; i < config_.num_shards; ++i) {
        shards_.push_back(std::make_unique<ShardBuffer>(config_.buffer_size_per_shard, i));
    }
}

ShardedWriteBuffer::~ShardedWriteBuffer() {
    shutdown();
}

core::Result<void> ShardedWriteBuffer::initialize(std::shared_ptr<Storage> storage) {
    if (!storage) {
        return core::Result<void>::error("Storage implementation is required");
    }
    
    storage_ = storage;
    initialized_.store(true);
    
    // Start background flush workers
    startFlushWorkers();
    
    return core::Result<void>();
}

core::Result<void> ShardedWriteBuffer::write(const core::TimeSeries& series,
                                            std::function<void(core::Result<void>)> callback) {
    if (!initialized_.load()) {
        return core::Result<void>::error("ShardedWriteBuffer not initialized");
    }
    
    if (shutdown_requested_.load()) {
        return core::Result<void>::error("ShardedWriteBuffer is shutting down");
    }
    
    if (series.samples().empty()) {
        return core::Result<void>::error("Time series cannot be empty");
    }
    
    // Create write operation
    WriteOperation op(series, callback);
    
    // Determine target shard based on series labels
    std::string series_key = series.labels().to_string();
    uint32_t shard_id = getShardId(series_key);
    
    // Add to shard buffer
    if (shards_[shard_id]->addWrite(op)) {
        total_writes_.fetch_add(1);
        return core::Result<void>();
    } else {
        dropped_writes_.fetch_add(1);
        return core::Result<void>::error("Buffer full for shard " + std::to_string(shard_id));
    }
}

core::Result<void> ShardedWriteBuffer::flush(bool force) {
    if (!initialized_.load()) {
        return core::Result<void>::error("ShardedWriteBuffer not initialized");
    }
    
    core::Result<void> overall_result = core::Result<void>();
    
    // Flush all shards
    for (uint32_t i = 0; i < config_.num_shards; ++i) {
        core::Result<void> shard_result = flushShard(i, force);
        if (!shard_result.ok()) {
            return shard_result;  // Return first error encountered
        }
    }
    
    return overall_result;
}

core::Result<void> ShardedWriteBuffer::flushShard(uint32_t shard_id, bool force) {
    if (shard_id >= config_.num_shards) {
        return core::Result<void>::error("Invalid shard ID: " + std::to_string(shard_id));
    }
    
    if (!initialized_.load()) {
        return core::Result<void>::error("ShardedWriteBuffer not initialized");
    }
    
    // Check if shard needs flushing
    auto& shard = shards_[shard_id];
    if (!force && shard->size() == 0) {
        return core::Result<void>();  // Nothing to flush
    }
    
    // If force is true, flush synchronously
    if (force) {
        return flushShardWithRetry(shard_id);
    }
    
    // Otherwise, add to flush queue for background processing
    {
        std::lock_guard<std::mutex> lock(flush_queue_mutex_);
        flush_queue_.push(shard_id);
    }
    flush_condition_.notify_one();
    
    return core::Result<void>();
}

core::Result<void> ShardedWriteBuffer::shutdown() {
    if (shutdown_requested_.load()) {
        return core::Result<void>();  // Already shutting down
    }
    
    shutdown_requested_.store(true);
    
    // Only flush if the buffer is initialized
    core::Result<void> result = core::Result<void>();
    if (initialized_.load()) {
        // Flush all remaining data synchronously before stopping workers
        result = flush(true);
        
        // Stop background workers
        stopFlushWorkers();
    }
    
    initialized_.store(false);
    return result;
}

ShardedWriteBuffer::BufferStats ShardedWriteBuffer::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    BufferStats stats;
    stats.total_shards = config_.num_shards;
    stats.total_writes = total_writes_.load();
    stats.dropped_writes = dropped_writes_.load();
    stats.total_flushes = total_flushes_.load();
    stats.failed_flushes = failed_flushes_.load();
    
    // Calculate utilization statistics
    std::vector<double> utilizations;
    size_t total_operations = 0;
    size_t total_bytes = 0;
    uint32_t active_shards = 0;
    
    for (const auto& shard : shards_) {
        double utilization = shard->utilization();
        utilizations.push_back(utilization);
        total_operations += shard->size();
        
        if (shard->size() > 0) {
            active_shards++;
        }
        
        // Estimate bytes (rough calculation)
        total_bytes += shard->size() * 1024;  // Assume ~1KB per operation
    }
    
    stats.active_shards = active_shards;
    stats.total_operations = total_operations;
    stats.total_bytes = total_bytes;
    
    if (!utilizations.empty()) {
        stats.avg_utilization = std::accumulate(utilizations.begin(), utilizations.end(), 0.0) / utilizations.size();
        stats.max_utilization = *std::max_element(utilizations.begin(), utilizations.end());
    }
    
    // Calculate throughput
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    if (duration.count() > 0) {
        stats.write_throughput = static_cast<double>(total_writes_.load()) / duration.count();
    }
    
    return stats;
}

ShardedWriteBuffer::ShardStats ShardedWriteBuffer::getShardStats(uint32_t shard_id) const {
    if (shard_id >= config_.num_shards) {
        return ShardStats{};  // Return empty stats for invalid shard
    }
    
    const auto& shard = shards_[shard_id];
    ShardStats stats;
    stats.shard_id = shard_id;
    stats.operations = shard->size();
    stats.bytes = shard->size() * 1024;  // Rough estimate
    stats.utilization = shard->utilization();
    stats.last_flush_time = shard->getLastFlushTime();
    
    return stats;
}

core::Result<void> ShardedWriteBuffer::updateConfig(const ShardedWriteBufferConfig& new_config) {
    if (initialized_.load()) {
        return core::Result<void>::error("Cannot update configuration while buffer is active");
    }
    
    config_ = new_config;
    return core::Result<void>();
}

bool ShardedWriteBuffer::isHealthy() const {
    if (!initialized_.load()) {
        return false;
    }
    
    if (shutdown_requested_.load()) {
        return false;
    }
    
    // Check if any shard is critically full (>90% utilization)
    for (const auto& shard : shards_) {
        if (shard->utilization() > 90.0) {
            return false;
        }
    }
    
    return true;
}

ShardedWriteBuffer::LoadBalanceInfo ShardedWriteBuffer::getLoadBalanceInfo() const {
    LoadBalanceInfo info;
    
    std::vector<double> utilizations;
    for (const auto& shard : shards_) {
        utilizations.push_back(shard->utilization());
    }
    
    if (utilizations.empty()) {
        return info;
    }
    
    // Find min and max
    auto min_max = std::minmax_element(utilizations.begin(), utilizations.end());
    info.least_loaded_shard = std::distance(utilizations.begin(), min_max.first);
    info.most_loaded_shard = std::distance(utilizations.begin(), min_max.second);
    
    // Calculate standard deviation
    double mean = std::accumulate(utilizations.begin(), utilizations.end(), 0.0) / utilizations.size();
    double variance = 0.0;
    for (double util : utilizations) {
        variance += (util - mean) * (util - mean);
    }
    variance /= utilizations.size();
    info.std_deviation = std::sqrt(variance);
    
    // Calculate imbalance ratio
    double max_util = *min_max.second;
    double min_util = *min_max.first;
    if (max_util > 0) {
        info.imbalance_ratio = (max_util - min_util) / max_util;
    }
    
    // Check if rebalancing is needed
    info.needs_rebalancing = info.imbalance_ratio > config_.load_balance_threshold;
    
    return info;
}

uint32_t ShardedWriteBuffer::getShardId(const std::string& series_id) const {
    return hashSeriesId(series_id) % config_.num_shards;
}

void ShardedWriteBuffer::flushWorker() {
    while (!shutdown_requested_.load()) {
        // Wait for the flush interval or shutdown signal
        {
            std::unique_lock<std::mutex> lock(flush_queue_mutex_);
            flush_condition_.wait_for(lock, std::chrono::milliseconds(config_.flush_interval_ms),
                                    [this] { return shutdown_requested_.load(); });
        }
        
        if (shutdown_requested_.load()) {
            break;
        }
        
        // Check all shards for data that needs flushing
        for (uint32_t shard_id = 0; shard_id < config_.num_shards; ++shard_id) {
            auto& shard = shards_[shard_id];
            if (shard->size() > 0) {
                // Check if enough time has passed since last flush
                auto now = std::chrono::system_clock::now();
                auto time_since_last_flush = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - shard->getLastFlushTime());
                
                if (time_since_last_flush.count() >= config_.flush_interval_ms) {
                    flushShardWithRetry(shard_id);
                }
            }
        }
    }
}

void ShardedWriteBuffer::processFlushQueue() {
    while (!flush_queue_.empty()) {
        uint32_t shard_id = flush_queue_.front();
        flush_queue_.pop();
        flushShardWithRetry(shard_id);
    }
}

core::Result<void> ShardedWriteBuffer::flushShardWithRetry(uint32_t shard_id) {
    // Get operations from shard
    std::vector<WriteOperation> operations = shards_[shard_id]->flush();
    
    if (operations.empty()) {
        return core::Result<void>();
    }
    
    // Flush to storage with retry logic
    core::Result<void> result = core::Result<void>();
    uint32_t retry_count = 0;
    
    while (retry_count < config_.retry_attempts) {
        result = flushOperationsToStorage(operations);
        
        if (result.ok()) {
            break;
        }
        
        retry_count++;
        if (retry_count < config_.retry_attempts) {
            std::this_thread::sleep_for(config_.retry_delay);
        }
    }
    
    // Update statistics
    total_flushes_.fetch_add(1);
    if (!result.ok()) {
        failed_flushes_.fetch_add(1);
        
        // Put failed operations back into the buffer for later retry
        for (const auto& op : operations) {
            shards_[shard_id]->addWrite(op);
        }
    }
    
    // Call callbacks
    for (const auto& op : operations) {
        if (op.callback) {
            op.callback(std::move(result));
        }
    }
    
    return result;
}

core::Result<void> ShardedWriteBuffer::flushOperationsToStorage(const std::vector<WriteOperation>& operations) {
    if (!storage_) {
        return core::Result<void>::error("No storage implementation available");
    }
    
    // Write each operation to storage
    for (const auto& op : operations) {
        core::Result<void> result = storage_->write(op.series);
        if (!result.ok()) {
            return result;
        }
    }
    
    return core::Result<void>();
}

void ShardedWriteBuffer::startFlushWorkers() {
    uint32_t num_workers = std::min(config_.max_flush_workers, config_.num_shards);
    
    for (uint32_t i = 0; i < num_workers; ++i) {
        flush_workers_.emplace_back(&ShardedWriteBuffer::flushWorker, this);
    }
}

void ShardedWriteBuffer::stopFlushWorkers() {
    // Signal workers to stop
    shutdown_requested_.store(true);
    flush_condition_.notify_all();
    
    // Wait for all workers to finish (only if there are workers)
    if (!flush_workers_.empty()) {
        for (auto& worker : flush_workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        flush_workers_.clear();
    }
}

bool ShardedWriteBuffer::needsRebalancing() const {
    auto load_info = getLoadBalanceInfo();
    return load_info.needs_rebalancing;
}

void ShardedWriteBuffer::rebalance() {
    // This is a simplified rebalancing implementation
    // In a production system, this would be more sophisticated
    
    auto load_info = getLoadBalanceInfo();
    if (!load_info.needs_rebalancing) {
        return;
    }
    
    // For now, just log the imbalance
    // TODO: Implement actual rebalancing logic
}

void ShardedWriteBuffer::updateStats() {
    // Statistics are updated in real-time via atomic counters
    // This method could be used for periodic aggregation if needed
}

uint32_t ShardedWriteBuffer::hashSeriesId(const std::string& series_id) const {
    // Simple hash function for consistent shard assignment
    uint32_t hash = 0;
    for (char c : series_id) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

// ShardedWriteBufferFactory implementation
std::shared_ptr<ShardedWriteBuffer> ShardedWriteBufferFactory::create() {
    return std::make_shared<ShardedWriteBuffer>();
}

std::shared_ptr<ShardedWriteBuffer> ShardedWriteBufferFactory::create(const ShardedWriteBufferConfig& config) {
    return std::make_shared<ShardedWriteBuffer>(config);
}

std::shared_ptr<ShardedWriteBuffer> ShardedWriteBufferFactory::createOptimized(uint32_t expected_throughput,
                                                                               uint32_t expected_latency,
                                                                               size_t available_memory) {
    ShardedWriteBufferConfig config;
    
    // Calculate optimal number of shards based on throughput
    config.num_shards = std::max(16u, std::min(256u, expected_throughput / 1000));
    
    // Calculate buffer size per shard based on available memory
    size_t total_buffer_size = available_memory / 4;  // Use 25% of available memory
    config.buffer_size_per_shard = total_buffer_size / config.num_shards;
    
    // Adjust flush interval based on latency requirements
    config.flush_interval_ms = std::max(100u, std::min(5000u, expected_latency * 2));
    
    // Calculate number of workers based on shard count
    config.max_flush_workers = std::min(8u, config.num_shards / 2);
    
    return std::make_shared<ShardedWriteBuffer>(config);
}

} // namespace storage
} // namespace tsdb 