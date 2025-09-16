#include "tsdb/storage/background_processor.h"
#include "tsdb/core/error.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

namespace tsdb {
namespace storage {

BackgroundProcessor::BackgroundProcessor(const BackgroundProcessorConfig& config)
    : config_(config), task_queue_(taskComparator) {
}

BackgroundProcessor::~BackgroundProcessor() {
    if (initialized_.load()) {
        shutdown();
    }
}

core::Result<void> BackgroundProcessor::initialize() {
    if (initialized_.load()) {
        return core::Result<void>::error("BackgroundProcessor already initialized");
    }
    
    if (config_.num_workers == 0) {
        return core::Result<void>::error("Invalid number of workers: 0");
    }
    
    if (config_.max_queue_size == 0) {
        return core::Result<void>::error("Invalid max queue size: 0");
    }
    
    // Reset statistics
    stats_.reset();
    
    // Start worker threads
    startWorkers();
    
    initialized_.store(true);
    return core::Result<void>();
}

core::Result<void> BackgroundProcessor::shutdown() {
    if (!initialized_.load() || shutdown_requested_.load()) {
        return core::Result<void>();
    }
    
    // Set shutdown flag first
    shutdown_requested_.store(true);
    queue_condition_.notify_all();
    
    // Wait for all active tasks to complete WITHOUT holding the queue mutex
    // This prevents deadlock where main thread holds queue_mutex while workers need it
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        // Release the lock immediately after checking
    }
    
    // Wait for active tasks to complete with a timeout to avoid infinite wait
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(5000); // 5 second timeout
    
    while (active_tasks_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > timeout) {
            // Force shutdown if timeout exceeded
            break;
        }
    }
    
    // Stop workers
    stopWorkers();
    
    initialized_.store(false);
    return core::Result<void>();
}

core::Result<void> BackgroundProcessor::submitTask(BackgroundTask task) {
    if (shutdown_requested_.load()) {
        return core::Result<void>::error("BackgroundProcessor is shutting down");
    }
    
    if (!initialized_.load()) {
        return core::Result<void>::error("BackgroundProcessor not initialized");
    }
    
    // Check if task has already timed out
    if (isTaskTimedOut(task)) {
        stats_.tasks_timeout.fetch_add(1);
        return core::Result<void>::error("Task already timed out");
    }
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (task_queue_.size() >= config_.max_queue_size) {
        stats_.tasks_rejected.fetch_add(1);
        stats_.max_queue_size_reached.fetch_add(1);
        return core::Result<void>::error("Queue is full");
    }
    
    // Assign unique task ID
    task.task_id = next_task_id_.fetch_add(1);
    
    auto task_ptr = std::make_unique<BackgroundTask>(std::move(task));
    task_queue_.push(std::move(task_ptr));
    
    // Update statistics
    stats_.tasks_submitted.fetch_add(1);
    stats_.queue_size.store(task_queue_.size());
    
    queue_condition_.notify_one();
    return core::Result<void>();
}

core::Result<void> BackgroundProcessor::submitCompressionTask(std::function<core::Result<void>()> task_func, uint32_t priority) {
    BackgroundTask task(BackgroundTaskType::COMPRESSION, std::move(task_func), priority);
    return submitTask(std::move(task));
}

core::Result<void> BackgroundProcessor::submitIndexingTask(std::function<core::Result<void>()> task_func, uint32_t priority) {
    BackgroundTask task(BackgroundTaskType::INDEXING, std::move(task_func), priority);
    return submitTask(std::move(task));
}

core::Result<void> BackgroundProcessor::submitFlushTask(std::function<core::Result<void>()> task_func, uint32_t priority) {
    BackgroundTask task(BackgroundTaskType::FLUSH, std::move(task_func), priority);
    return submitTask(std::move(task));
}

core::Result<void> BackgroundProcessor::submitCleanupTask(std::function<core::Result<void>()> task_func, uint32_t priority) {
    BackgroundTask task(BackgroundTaskType::CLEANUP, std::move(task_func), priority);
    return submitTask(std::move(task));
}

core::Result<void> BackgroundProcessor::waitForCompletion(std::chrono::milliseconds timeout) {
    auto start_time = std::chrono::system_clock::now();
    
    while (true) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            // Check if queue is empty and all submitted tasks have been processed
            if (task_queue_.empty() && stats_.tasks_processed.load() >= stats_.tasks_submitted.load()) {
                return core::Result<void>();
            }
        }
        
        auto now = std::chrono::system_clock::now();
        auto elapsed = now - start_time;
        if (elapsed >= timeout) {
            return core::Result<void>::error("Wait for completion timed out");
        }
        
        // Sleep for a short time to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

BackgroundProcessorStatsSnapshot BackgroundProcessor::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    // Update queue size
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    const_cast<BackgroundProcessorStats&>(stats_).queue_size.store(task_queue_.size());
    
    BackgroundProcessorStatsSnapshot snap;
    snap.tasks_processed = stats_.tasks_processed.load();
    snap.tasks_failed = stats_.tasks_failed.load();
    snap.tasks_timeout = stats_.tasks_timeout.load();
    snap.compression_tasks = stats_.compression_tasks.load();
    snap.indexing_tasks = stats_.indexing_tasks.load();
    snap.flush_tasks = stats_.flush_tasks.load();
    snap.cleanup_tasks = stats_.cleanup_tasks.load();
    snap.queue_size = stats_.queue_size.load();
    snap.max_queue_size_reached = stats_.max_queue_size_reached.load();
    snap.tasks_submitted = stats_.tasks_submitted.load();
    snap.tasks_rejected = stats_.tasks_rejected.load();
    return snap;
}

const BackgroundProcessorStats& BackgroundProcessor::getStatsRef() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    // Update queue size
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    const_cast<BackgroundProcessorStats&>(stats_).queue_size.store(task_queue_.size());
    return stats_;
}

void BackgroundProcessor::workerThread() {
    active_workers_.fetch_add(1);
    
    while (!shutdown_requested_.load()) {
        auto task = getNextTask();
        if (!task) {
            // If no task and shutdown not requested, continue waiting
            continue;
        }
        processTask(*task);
    }
    
    // Process any remaining tasks in the queue during shutdown
    // But limit the number of iterations to prevent infinite loops
    int remaining_attempts = 100; // Reasonable limit
    while (remaining_attempts > 0 && !shutdown_requested_.load()) {
        auto task = getNextTask();
        if (!task) {
            break; // No more tasks, exit
        }
        processTask(*task);
        remaining_attempts--;
    }
    
    active_workers_.fetch_sub(1);
}

void BackgroundProcessor::processTask(BackgroundTask& task) {
    active_tasks_.fetch_add(1);
    try {
        // Check for timeout before processing
        if (isTaskTimedOut(task)) {
            updateStats(task, false, true);
            active_tasks_.fetch_sub(1);
            tasks_finished_cond_.notify_all();
            return;
        }
        
        // Execute task
        auto result = task.task_func();
        
        // Check for timeout after execution
        if (isTaskTimedOut(task)) {
            updateStats(task, false, true);
            active_tasks_.fetch_sub(1);
            tasks_finished_cond_.notify_all();
            return;
        }
        
        bool success = result.ok();
        updateStats(task, success, false);
    } catch (const std::exception& e) {
        updateStats(task, false, false);
    }
    active_tasks_.fetch_sub(1);
    tasks_finished_cond_.notify_all();
}

std::unique_ptr<BackgroundTask> BackgroundProcessor::getNextTask() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // Wait for tasks or shutdown with timeout
    bool has_task = queue_condition_.wait_for(lock, config_.worker_wait_timeout, [this] {
        return !task_queue_.empty() || shutdown_requested_.load();
    });
    
    // If shutdown is requested, return immediately if no tasks
    if (shutdown_requested_.load()) {
        if (task_queue_.empty()) {
            return nullptr;
        }
    } else if (task_queue_.empty()) {
        return nullptr;
    }
    
    // Get the next task
    auto task = std::move(const_cast<std::unique_ptr<BackgroundTask>&>(task_queue_.top()));
    task_queue_.pop();
    stats_.queue_size.store(task_queue_.size());
    
    return task;
}

void BackgroundProcessor::startWorkers() {
    workers_.clear();
    workers_.reserve(config_.num_workers);
    
    for (uint32_t i = 0; i < config_.num_workers; ++i) {
        workers_.emplace_back(&BackgroundProcessor::workerThread, this);
    }
}

void BackgroundProcessor::stopWorkers() {
    // Signal workers to stop (shutdown_requested_ is already set)
    queue_condition_.notify_all();
    
    // Wait for all workers to finish
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers_.clear();
    active_workers_.store(0);
}

void BackgroundProcessor::updateStats(const BackgroundTask& task, bool success, bool timeout) {
    stats_.tasks_processed.fetch_add(1);
    
    if (!success) {
        if (timeout) {
            stats_.tasks_timeout.fetch_add(1);
        } else {
            stats_.tasks_failed.fetch_add(1);
        }
    }
    
    // Update task type counters
    switch (task.type) {
        case BackgroundTaskType::COMPRESSION:
            stats_.compression_tasks.fetch_add(1);
            break;
        case BackgroundTaskType::INDEXING:
            stats_.indexing_tasks.fetch_add(1);
            break;
        case BackgroundTaskType::FLUSH:
            stats_.flush_tasks.fetch_add(1);
            break;
        case BackgroundTaskType::CLEANUP:
            stats_.cleanup_tasks.fetch_add(1);
            break;
    }
}

bool BackgroundProcessor::isTaskTimedOut(const BackgroundTask& task) const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - task.created_time;
    return elapsed > config_.task_timeout;
}

bool BackgroundProcessor::isHealthy() const {
    if (!initialized_.load()) {
        return false;
    }
    
    if (shutdown_requested_.load()) {
        return false;
    }
    
    // Check if workers are running
    if (active_workers_.load() == 0 && config_.num_workers > 0) {
        return false;
    }
    
    return true;
}

uint32_t BackgroundProcessor::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return static_cast<uint32_t>(task_queue_.size());
}

core::Result<void> BackgroundProcessor::updateConfig(const BackgroundProcessorConfig& new_config) {
    // Allow config updates before initialization
    if (!initialized_.load()) {
        config_ = new_config;
        return core::Result<void>();
    }
    
    // Don't allow config updates while running (after initialization)
    if (initialized_.load() && !shutdown_requested_.load()) {
        return core::Result<void>::error("Cannot update config while processor is running");
    }
    
    // For now, we only allow updating certain config parameters
    // that don't require restarting workers
    config_.task_timeout = new_config.task_timeout;
    config_.shutdown_timeout = new_config.shutdown_timeout;
    config_.worker_wait_timeout = new_config.worker_wait_timeout;
    config_.enable_metrics = new_config.enable_metrics;
    
    // Note: num_workers and max_queue_size changes would require
    // restarting the processor, which is not implemented yet
    
    return core::Result<void>();
}

} // namespace storage
} // namespace tsdb