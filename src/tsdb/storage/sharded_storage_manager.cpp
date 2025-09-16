/**
 * @file sharded_storage_manager.cpp
 * @brief Implementation of high-concurrency sharded storage manager
 */

#include "tsdb/storage/sharded_storage_manager.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <functional>

namespace tsdb {
namespace storage {

ShardedStorageManager::ShardedStorageManager(const ShardedStorageConfig& config)
    : config_(config)
    , shards_(config.num_shards)
    , shard_health_(config.num_shards)
    , write_queues_(config.num_shards)
    , queue_mutexes_(config.num_shards)
    , queue_cvs_(config.num_shards)
    , workers_(config.num_shards) {
    
    // Initialize shard health
    for (size_t i = 0; i < config.num_shards; ++i) {
        shard_health_[i].store(true);
    }
}

ShardedStorageManager::~ShardedStorageManager() {
    close();
}

core::Result<void> ShardedStorageManager::init(const core::StorageConfig& config) {
    if (initialized_.load()) {
        return core::Result<void>::error("ShardedStorageManager already initialized");
    }
    
    storage_config_ = config;
    
    try {
        // Initialize all shards
        for (size_t i = 0; i < config_.num_shards; ++i) {
            auto shard = std::make_shared<StorageImpl>(storage_config_);
            auto result = shard->init(storage_config_);
            if (!result.ok()) {
                return core::Result<void>::error("Failed to initialize shard " + std::to_string(i) + ": " + result.error());
            }
            shards_[i] = shard;
            shard_health_[i].store(true);
        }
        
        // Start background workers
        start_workers();
        
        initialized_.store(true);
        return core::Result<void>();
        
    } catch (const std::exception& e) {
        return core::Result<void>::error("ShardedStorageManager initialization failed: " + std::string(e.what()));
    }
}

core::Result<void> ShardedStorageManager::write(const core::TimeSeries& series, 
                                               std::function<void(const core::Result<void>&)> callback) {
    if (!initialized_.load()) {
        return core::Result<void>::error("ShardedStorageManager not initialized");
    }
    
    if (shutdown_requested_.load()) {
        return core::Result<void>::error("ShardedStorageManager is shutting down");
    }
    
    if (series.empty()) {
        return core::Result<void>::error("Cannot write empty time series");
    }
    
    // Determine target shard
    size_t shard_id = get_shard_id(series);
    
    // Create write operation
    WriteOperation op(series, callback);
    
    // Enqueue the write operation
    if (enqueue_write(shard_id, std::move(op))) {
        stats_.total_writes.fetch_add(1);
        stats_.queued_writes.fetch_add(1);
        return core::Result<void>();
    } else {
        stats_.dropped_writes.fetch_add(1);
        return core::Result<void>::error("Write queue full for shard " + std::to_string(shard_id));
    }
}

core::Result<core::TimeSeries> ShardedStorageManager::read(const core::Labels& labels, 
                                                         int64_t start_time, int64_t end_time) {
    if (!initialized_.load()) {
        return core::Result<core::TimeSeries>::error("ShardedStorageManager not initialized");
    }
    
    // Determine target shard
    size_t shard_id = get_shard_id(labels);
    
    // Get the shard
    auto shard = get_shard(shard_id);
    if (!shard) {
        return core::Result<core::TimeSeries>::error("Shard " + std::to_string(shard_id) + " not available");
    }
    
    // Read from the shard
    return shard->read(labels, start_time, end_time);
}

core::Result<void> ShardedStorageManager::flush() {
    if (!initialized_.load()) {
        return core::Result<void>::error("ShardedStorageManager not initialized");
    }
    
    core::Result<void> overall_result = core::Result<void>();
    
    // Flush all shard queues
    for (size_t i = 0; i < config_.num_shards; ++i) {
        flush_shard_queue(i, true);
        
        // Flush the shard itself
        auto shard = get_shard(i);
        if (shard) {
        auto result = shard->flush();
        if (!result.ok() && overall_result.ok()) {
            overall_result = std::move(result); // Return first error
        }
        }
    }
    
    return overall_result;
}

core::Result<void> ShardedStorageManager::close() {
    if (!initialized_.load()) {
        return core::Result<void>();
    }
    
    shutdown_requested_.store(true);
    
    // Stop all workers
    stop_workers();
    
    // Flush all queues
    for (size_t i = 0; i < config_.num_shards; ++i) {
        flush_shard_queue(i, true);
    }
    
    // Close all shards
    for (auto& shard : shards_) {
        if (shard) {
            shard->close();
        }
    }
    
    initialized_.store(false);
    return core::Result<void>();
}

size_t ShardedStorageManager::get_shard_id(const core::TimeSeries& series) const {
    return get_shard_id(series.labels());
}

size_t ShardedStorageManager::get_shard_id(const core::Labels& labels) const {
    // Use hash of labels to determine shard
    std::hash<std::string> hasher;
    std::string label_string = labels.to_string();
    return hasher(label_string) % config_.num_shards;
}

std::shared_ptr<StorageImpl> ShardedStorageManager::get_shard(size_t shard_id) {
    if (shard_id >= shards_.size()) {
        return nullptr;
    }
    
    if (!shard_health_[shard_id].load()) {
        return nullptr;
    }
    
    return shards_[shard_id];
}

bool ShardedStorageManager::enqueue_write(size_t shard_id, WriteOperation&& op) {
    if (shard_id >= write_queues_.size()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(queue_mutexes_[shard_id]);
    
    if (write_queues_[shard_id].size() >= config_.queue_size) {
        return false; // Queue full
    }
    
    write_queues_[shard_id].push(std::move(op));
    queue_cvs_[shard_id].notify_one();
    
    return true;
}

void ShardedStorageManager::process_write_queue(size_t shard_id) {
    auto shard = get_shard(shard_id);
    if (!shard) {
        return;
    }
    
    std::vector<WriteOperation> batch;
    batch.reserve(config_.batch_size);
    
    // Collect batch of operations
    {
        std::lock_guard<std::mutex> lock(queue_mutexes_[shard_id]);
        
        size_t batch_count = 0;
        while (!write_queues_[shard_id].empty() && batch_count < config_.batch_size) {
            batch.push_back(std::move(write_queues_[shard_id].front()));
            write_queues_[shard_id].pop();
            batch_count++;
        }
    }
    
    if (batch.empty()) {
        return;
    }
    
    // Process batch
    for (auto& op : batch) {
        auto result = shard->write(op.series);
        
        if (result.ok()) {
            stats_.successful_writes.fetch_add(1);
        } else {
            handle_write_error(shard_id, op, result);
        }
        
        stats_.queued_writes.fetch_sub(1);
        
        // Call callback if provided
        if (op.callback) {
            op.callback(result);
        }
    }
}

void ShardedStorageManager::flush_shard_queue(size_t shard_id, bool force) {
    if (shard_id >= write_queues_.size()) {
        return;
    }
    
    // Process all remaining operations in the queue
    while (true) {
        process_write_queue(shard_id);
        
        // Check if queue is empty
        {
            std::lock_guard<std::mutex> lock(queue_mutexes_[shard_id]);
            if (write_queues_[shard_id].empty()) {
                break;
            }
        }
        
        if (!force) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void ShardedStorageManager::start_workers() {
    for (size_t shard_id = 0; shard_id < config_.num_shards; ++shard_id) {
        for (size_t worker_id = 0; worker_id < config_.num_workers; ++worker_id) {
            workers_[shard_id].emplace_back(&ShardedStorageManager::worker_thread, this, shard_id);
        }
    }
}

void ShardedStorageManager::stop_workers() {
    // Notify all workers to stop
    for (size_t i = 0; i < config_.num_shards; ++i) {
        queue_cvs_[i].notify_all();
    }
    
    // Wait for all workers to finish
    for (auto& shard_workers : workers_) {
        for (auto& worker : shard_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    workers_.clear();
    workers_.resize(config_.num_shards);
}

void ShardedStorageManager::worker_thread(size_t shard_id) {
    while (!shutdown_requested_.load()) {
        // Check if there are operations to process
        {
            std::unique_lock<std::mutex> lock(queue_mutexes_[shard_id]);
            queue_cvs_[shard_id].wait_for(lock, config_.flush_interval, 
                [this, shard_id] { 
                    return !write_queues_[shard_id].empty() || shutdown_requested_.load(); 
                });
        }
        
        if (shutdown_requested_.load()) {
            break;
        }
        
        // Process write queue
        process_write_queue(shard_id);
    }
}

void ShardedStorageManager::handle_write_error(size_t shard_id, WriteOperation& op, const core::Result<void>& result) {
    if (should_retry(op)) {
        op.retry_count++;
        stats_.retry_count.fetch_add(1);
        
        // Re-enqueue with delay
        std::this_thread::sleep_for(config_.retry_delay);
        enqueue_write(shard_id, std::move(op));
    } else {
        stats_.failed_writes.fetch_add(1);
        
        // Call callback with error
        if (op.callback) {
            op.callback(result);
        }
    }
}

bool ShardedStorageManager::should_retry(const WriteOperation& op) const {
    return op.retry_count < config_.max_retries;
}

ShardedStorageStats ShardedStorageManager::get_stats() const {
    return stats_;
}

std::string ShardedStorageManager::get_stats_string() const {
    std::ostringstream oss;
    oss << "ShardedStorageManager Stats:\n";
    oss << "  Total Writes: " << stats_.total_writes.load() << "\n";
    oss << "  Successful Writes: " << stats_.successful_writes.load() << "\n";
    oss << "  Failed Writes: " << stats_.failed_writes.load() << "\n";
    oss << "  Queued Writes: " << stats_.queued_writes.load() << "\n";
    oss << "  Dropped Writes: " << stats_.dropped_writes.load() << "\n";
    oss << "  Retry Count: " << stats_.retry_count.load() << "\n";
    oss << "  Success Rate: " << (stats_.get_success_rate() * 100.0) << "%\n";
    oss << "  Queue Utilization: " << (stats_.get_queue_utilization() * 100.0) << "%\n";
    return oss.str();
}

void ShardedStorageManager::set_config(const ShardedStorageConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

ShardedStorageConfig ShardedStorageManager::get_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

bool ShardedStorageManager::is_healthy() const {
    for (size_t i = 0; i < shard_health_.size(); ++i) {
        if (!shard_health_[i].load()) {
            return false;
        }
    }
    return initialized_.load() && !shutdown_requested_.load();
}

std::vector<bool> ShardedStorageManager::get_shard_health() const {
    std::vector<bool> health;
    for (const auto& shard_health : shard_health_) {
        health.push_back(shard_health.load());
    }
    return health;
}

} // namespace storage
} // namespace tsdb
