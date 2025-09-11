#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "tsdb/core/result.h"
#include "tsdb/storage/performance_config.h"

namespace tsdb {
namespace storage {

/**
 * @brief Background task types
 */
enum class BackgroundTaskType {
    COMPRESSION,
    INDEXING,
    FLUSH,
    CLEANUP
};

/**
 * @brief Background task structure
 */
struct BackgroundTask {
    BackgroundTaskType type;
    std::function<core::Result<void>()> task_func;
    std::chrono::system_clock::time_point created_time;
    uint32_t priority;  // Lower number = higher priority
    uint64_t task_id;   // Unique task identifier
    
    BackgroundTask(BackgroundTaskType t, std::function<core::Result<void>()> func, uint32_t p = 5)
        : type(t), task_func(std::move(func)), created_time(std::chrono::system_clock::now()), priority(p), task_id(0) {}
};

/**
 * @brief Background processor configuration
 */
struct BackgroundProcessorConfig {
    uint32_t num_workers = 4;
    uint32_t max_queue_size = 10000;
    std::chrono::milliseconds task_timeout{30000};  // 30 seconds
    std::chrono::milliseconds shutdown_timeout{5000};  // 5 seconds
    std::chrono::milliseconds worker_wait_timeout{100};  // 100ms for worker polling
    bool enable_metrics = true;
    
    BackgroundProcessorConfig() = default;
};

/**
 * @brief Background processor statistics
 */
struct BackgroundProcessorStats {
    std::atomic<uint64_t> tasks_processed{0};
    std::atomic<uint64_t> tasks_failed{0};
    std::atomic<uint64_t> tasks_timeout{0};
    std::atomic<uint64_t> compression_tasks{0};
    std::atomic<uint64_t> indexing_tasks{0};
    std::atomic<uint64_t> flush_tasks{0};
    std::atomic<uint64_t> cleanup_tasks{0};
    std::atomic<uint64_t> queue_size{0};
    std::atomic<uint64_t> max_queue_size_reached{0};
    std::atomic<uint64_t> tasks_submitted{0};
    std::atomic<uint64_t> tasks_rejected{0};
    
    void reset() {
        tasks_processed.store(0);
        tasks_failed.store(0);
        tasks_timeout.store(0);
        compression_tasks.store(0);
        indexing_tasks.store(0);
        flush_tasks.store(0);
        cleanup_tasks.store(0);
        queue_size.store(0);
        max_queue_size_reached.store(0);
        tasks_submitted.store(0);
        tasks_rejected.store(0);
    }
};

struct BackgroundProcessorStatsSnapshot {
    uint64_t tasks_processed = 0;
    uint64_t tasks_failed = 0;
    uint64_t tasks_timeout = 0;
    uint64_t compression_tasks = 0;
    uint64_t indexing_tasks = 0;
    uint64_t flush_tasks = 0;
    uint64_t cleanup_tasks = 0;
    uint64_t queue_size = 0;
    uint64_t max_queue_size_reached = 0;
    uint64_t tasks_submitted = 0;
    uint64_t tasks_rejected = 0;
};

/**
 * @brief Background processor for handling non-blocking operations
 * 
 * This class manages a thread pool of workers that handle background tasks
 * such as compression, indexing, and flushing to ensure the write path
 * remains non-blocking.
 */
class BackgroundProcessor {
public:
    explicit BackgroundProcessor(const BackgroundProcessorConfig& config = BackgroundProcessorConfig{});
    ~BackgroundProcessor();
    
    // Disable copy
    BackgroundProcessor(const BackgroundProcessor&) = delete;
    BackgroundProcessor& operator=(const BackgroundProcessor&) = delete;
    
    // Disable move
    BackgroundProcessor(BackgroundProcessor&&) = delete;
    BackgroundProcessor& operator=(BackgroundProcessor&&) = delete;
    
    /**
     * @brief Initialize the background processor
     * @return Result indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Shutdown the background processor gracefully
     * @return Result indicating success or failure
     */
    core::Result<void> shutdown();
    
    /**
     * @brief Submit a background task
     * @param task Task to be executed
     * @return Result indicating success or failure
     */
    core::Result<void> submitTask(BackgroundTask task);
    
    /**
     * @brief Submit a compression task
     * @param task_func Compression function
     * @param priority Task priority (lower = higher priority)
     * @return Result indicating success or failure
     */
    core::Result<void> submitCompressionTask(std::function<core::Result<void>()> task_func, uint32_t priority = 3);
    
    /**
     * @brief Submit an indexing task
     * @param task_func Indexing function
     * @param priority Task priority (lower = higher priority)
     * @return Result indicating success or failure
     */
    core::Result<void> submitIndexingTask(std::function<core::Result<void>()> task_func, uint32_t priority = 2);
    
    /**
     * @brief Submit a flush task
     * @param task_func Flush function
     * @param priority Task priority (lower = higher priority)
     * @return Result indicating success or failure
     */
    core::Result<void> submitFlushTask(std::function<core::Result<void>()> task_func, uint32_t priority = 1);
    
    /**
     * @brief Submit a cleanup task
     * @param task_func Cleanup function
     * @param priority Task priority (lower = higher priority)
     * @return Result indicating success or failure
     */
    core::Result<void> submitCleanupTask(std::function<core::Result<void>()> task_func, uint32_t priority = 4);
    
    /**
     * @brief Wait for all pending tasks to complete
     * @param timeout Maximum time to wait
     * @return Result indicating success or failure
     */
    core::Result<void> waitForCompletion(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});
    
    /**
     * @brief Get current statistics (snapshot, not atomics)
     * @return Background processor statistics snapshot
     */
    BackgroundProcessorStatsSnapshot getStats() const;
    
    /**
     * @brief Get current statistics by reference
     * @return Reference to background processor statistics
     */
    const BackgroundProcessorStats& getStatsRef() const;
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    const BackgroundProcessorConfig& getConfig() const { return config_; }
    
private:
    std::atomic<uint32_t> active_tasks_{0};
    std::condition_variable tasks_finished_cond_;
    
    /**
     * @brief Worker thread function
     */
    void workerThread();
    
    /**
     * @brief Process a single task
     * @param task Task to process
     */
    void processTask(BackgroundTask& task);
    
    /**
     * @brief Get next task from queue
     * @return Task if available, nullptr otherwise
     */
    std::unique_ptr<BackgroundTask> getNextTask();
    
    /**
     * @brief Start worker threads
     */
    void startWorkers();
    
    /**
     * @brief Stop worker threads
     */
    void stopWorkers();
    
    /**
     * @brief Update statistics
     */
    void updateStats(const BackgroundTask& task, bool success, bool timeout = false);
    
    /**
     * @brief Check if task has timed out
     * @param task Task to check
     * @return True if task has timed out
     */
    bool isTaskTimedOut(const BackgroundTask& task) const;
    
    /**
     * @brief Priority queue comparator function
     */
    static bool taskComparator(const std::unique_ptr<BackgroundTask>& a, const std::unique_ptr<BackgroundTask>& b) {
        // Lower priority number = higher priority
        if (a->priority != b->priority) {
            return a->priority > b->priority;
        }
        // If same priority, older task (lower task_id) has higher priority
        return a->task_id > b->task_id;
    }
    
    BackgroundProcessorConfig config_;
    BackgroundProcessorStats stats_;
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<uint64_t> next_task_id_{1};
    
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::priority_queue<std::unique_ptr<BackgroundTask>, 
                       std::vector<std::unique_ptr<BackgroundTask>>,
                       std::function<bool(const std::unique_ptr<BackgroundTask>&, 
                                        const std::unique_ptr<BackgroundTask>&)>> task_queue_;
    
    std::vector<std::thread> workers_;
    std::atomic<uint32_t> active_workers_{0};
    
    // Performance tracking
    std::chrono::system_clock::time_point last_stats_update_;
    mutable std::mutex stats_mutex_;
};

} // namespace storage
} // namespace tsdb