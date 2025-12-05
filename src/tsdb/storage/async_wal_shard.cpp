#include "tsdb/storage/async_wal_shard.h"
#include "tsdb/common/logger.h"
#include <vector>

namespace tsdb {
namespace storage {

AsyncWALShard::AsyncWALShard(const std::string& dir, size_t max_queue_size) 
    : dir_(dir), max_queue_size_(max_queue_size), running_(true) {
    wal_ = std::make_unique<WriteAheadLog>(dir);
    worker_ = std::thread(&AsyncWALShard::worker_loop, this);
}

AsyncWALShard::~AsyncWALShard() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        running_ = false;
    }
    queue_cv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
}

core::Result<void> AsyncWALShard::log(const core::TimeSeries& series) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (queue_.size() >= max_queue_size_) {
            producer_cv_.wait(lock, [this] { return queue_.size() < max_queue_size_; });
        }
        queue_.push(series);
    }
    queue_cv_.notify_one();
    return core::Result<void>();
}

core::Result<void> AsyncWALShard::replay(std::function<void(const core::TimeSeries&)> callback) {
    return wal_->replay(callback);
}

core::Result<void> AsyncWALShard::checkpoint(int last_segment_to_keep) {
    return wal_->checkpoint(last_segment_to_keep);
}

void AsyncWALShard::flush() {
    // Wake up the worker to process pending items
    queue_cv_.notify_one();
    
    // Wait until queue is empty
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (queue_.empty()) {
            break;
        }
    }
}

size_t AsyncWALShard::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.size();
}

void AsyncWALShard::Test_SetWorkerDelay(std::chrono::milliseconds delay) {
    worker_delay_ms_ = delay.count();
}

void AsyncWALShard::worker_loop() {
    const size_t BATCH_SIZE = 1000;
    std::vector<core::TimeSeries> batch;
    batch.reserve(BATCH_SIZE);

    while (true) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !queue_.empty() || !running_; });

            if (!running_ && queue_.empty()) {
                break;
            }

            // Drain queue up to BATCH_SIZE
            while (!queue_.empty() && batch.size() < BATCH_SIZE) {
                batch.push_back(std::move(queue_.front()));
                queue_.pop();
            }
            // Notify producer that space might be available
            producer_cv_.notify_all();
        }

        if (batch.empty()) continue;
        
        // Simulate delay for testing
        int64_t delay = worker_delay_ms_.load();
        if (delay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }

        TSDB_DEBUG("AsyncWALShard: Processing batch of {} items", batch.size());

        // Process batch
        for (const auto& series : batch) {
            // Log without flushing
            wal_->log(series, false);
        }
        
        // Flush once per batch
        wal_->flush();
        
        batch.clear();
    }
}

} // namespace storage
} // namespace tsdb
