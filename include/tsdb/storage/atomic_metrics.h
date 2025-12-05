#pragma once

#include <atomic>
#include <cstdint>
#include <chrono>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace tsdb {
namespace storage {
namespace internal {

/**
 * @brief Configuration for atomic metrics
 */
struct AtomicMetricsConfig {
    bool enable_tracking = true;           // Enable/disable metrics tracking
    bool enable_timing = true;             // Enable timing measurements
    bool enable_cache_metrics = true;      // Enable cache hit/miss tracking
    bool enable_compression_metrics = true; // Enable compression ratio tracking
    uint32_t sample_interval = 1000;       // Sample every N operations
    bool use_relaxed_ordering = true;      // Use relaxed memory ordering for performance
};

/**
 * @brief Atomic metrics for zero-overhead performance tracking
 * 
 * This class provides atomic metrics tracking with minimal performance impact.
 * It uses relaxed memory ordering and atomic operations to ensure thread safety
 * without blocking or significant overhead.
 */
class AtomicMetrics {
public:
    explicit AtomicMetrics(const AtomicMetricsConfig& config = AtomicMetricsConfig{});
    
    /**
     * @brief Track a write operation
     * @param bytes_written Number of bytes written
     * @param duration_ns Duration in nanoseconds
     */
    void recordWrite(size_t bytes_written, uint64_t duration_ns = 0);
    
    /**
     * @brief Track a read operation
     * @param bytes_read Number of bytes read
     * @param duration_ns Duration in nanoseconds
     */
    void recordRead(size_t bytes_read, uint64_t duration_ns = 0);
    
    /**
     * @brief Track a cache hit
     */
    void recordCacheHit();
    
    /**
     * @brief Track a cache miss
     */
    void recordCacheMiss();
    
    /**
     * @brief Track compression operation
     * @param original_size Original data size in bytes
     * @param compressed_size Compressed data size in bytes
     * @param duration_ns Duration in nanoseconds
     */
    void recordCompression(size_t original_size, size_t compressed_size, uint64_t duration_ns = 0);
    
    /**
     * @brief Track decompression operation
     * @param compressed_size Compressed data size in bytes
     * @param decompressed_size Decompressed data size in bytes
     * @param duration_ns Duration in nanoseconds
     */
    void recordDecompression(size_t compressed_size, size_t decompressed_size, uint64_t duration_ns = 0);
    
    /**
     * @brief Track memory allocation
     * @param bytes_allocated Number of bytes allocated
     */
    void recordAllocation(size_t bytes_allocated);
    
    /**
     * @brief Track memory deallocation
     * @param bytes_deallocated Number of bytes deallocated
     */
    void recordDeallocation(size_t bytes_deallocated);

    /**
     * @brief Track a dropped sample (filtering)
     */
    void recordDroppedSample();

    /**
     * @brief Track a derived sample (derived metrics)
     */
    void recordDerivedSample();
    
    /**
     * @brief Track rule check duration
     * @param duration_ns Duration in nanoseconds
     */
    void recordRuleCheck(uint64_t duration_ns);
    
    /**
     * @brief Get current metrics snapshot
     * @return Metrics snapshot
     */
    struct MetricsSnapshot {
        // Operation counts
        uint64_t write_count = 0;
        uint64_t read_count = 0;
        uint64_t cache_hits = 0;
        uint64_t cache_misses = 0;
        uint64_t compression_count = 0;
        uint64_t decompression_count = 0;
        uint64_t allocation_count = 0;
        uint64_t deallocation_count = 0;
        
        // Data volumes
        uint64_t bytes_written = 0;
        uint64_t bytes_read = 0;
        uint64_t bytes_compressed = 0;
        uint64_t bytes_decompressed = 0;
        uint64_t bytes_allocated = 0;
        uint64_t bytes_deallocated = 0;
        
        // Filtering & Derived Metrics
        uint64_t dropped_samples = 0;
        uint64_t derived_samples = 0;
        uint64_t total_rule_check_time = 0;
        
        // Timing (in nanoseconds)
        uint64_t total_write_time = 0;
        uint64_t total_read_time = 0;
        uint64_t total_compression_time = 0;
        uint64_t total_decompression_time = 0;
        
        // Calculated metrics
        double cache_hit_ratio = 0.0;
        double average_compression_ratio = 0.0;
        double average_write_latency_ns = 0.0;
        double average_read_latency_ns = 0.0;
        double average_compression_latency_ns = 0.0;
        double average_decompression_latency_ns = 0.0;
        double write_throughput_mbps = 0.0;
        double read_throughput_mbps = 0.0;
        double compression_throughput_mbps = 0.0;
        double decompression_throughput_mbps = 0.0;
        
        // Memory usage
        int64_t net_memory_usage = 0;  // allocated - deallocated
    };
    
    MetricsSnapshot getSnapshot() const;
    
    /**
     * @brief Reset all metrics to zero
     */
    void reset();
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    const AtomicMetricsConfig& getConfig() const { return config_; }
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void updateConfig(const AtomicMetricsConfig& config) { config_ = config; }
    
    /**
     * @brief Check if metrics tracking is enabled
     * @return True if tracking is enabled
     */
    bool isEnabled() const { return config_.enable_tracking; }
    
    /**
     * @brief Get metrics as formatted string
     * @return Formatted metrics string
     */
    std::string getFormattedMetrics() const;
    
    /**
     * @brief Get metrics as JSON string
     * @return JSON formatted metrics
     */
    std::string getJsonMetrics() const;

private:
    AtomicMetricsConfig config_;
    
    // Atomic counters for thread-safe metrics
    std::atomic<uint64_t> write_count_{0};
    std::atomic<uint64_t> read_count_{0};
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};
    std::atomic<uint64_t> compression_count_{0};
    std::atomic<uint64_t> decompression_count_{0};
    std::atomic<uint64_t> allocation_count_{0};
    std::atomic<uint64_t> deallocation_count_{0};
    
    // Atomic data volume counters
    std::atomic<uint64_t> bytes_written_{0};
    std::atomic<uint64_t> bytes_read_{0};
    std::atomic<uint64_t> bytes_compressed_{0};
    std::atomic<uint64_t> bytes_decompressed_{0};
    std::atomic<uint64_t> bytes_allocated_{0};
    std::atomic<uint64_t> bytes_deallocated_{0};
    
    // Filtering & Derived Metrics
    std::atomic<uint64_t> dropped_samples_{0};
    std::atomic<uint64_t> derived_samples_{0};
    std::atomic<uint64_t> total_rule_check_time_{0};
    
    // Atomic timing counters
    std::atomic<uint64_t> total_write_time_{0};
    std::atomic<uint64_t> total_read_time_{0};
    std::atomic<uint64_t> total_compression_time_{0};
    std::atomic<uint64_t> total_decompression_time_{0};
    
    // Memory ordering for atomic operations
    std::memory_order memory_order_;
    
    /**
     * @brief Get current timestamp in nanoseconds
     * @return Current timestamp
     */
    uint64_t getCurrentTimestamp() const;
    
    /**
     * @brief Calculate derived metrics
     * @param snapshot Metrics snapshot to update
     */
    void calculateDerivedMetrics(MetricsSnapshot& snapshot) const;
    
    /**
     * @brief Format duration in human-readable format
     * @param duration_ns Duration in nanoseconds
     * @return Formatted duration string
     */
    std::string formatDuration(uint64_t duration_ns) const;
    
    /**
     * @brief Format bytes in human-readable format
     * @param bytes Number of bytes
     * @return Formatted bytes string
     */
    std::string formatBytes(uint64_t bytes) const;
};

/**
 * @brief Global metrics instance for easy access
 */
class GlobalMetrics {
public:
    /**
     * @brief Get global metrics instance
     * @return Reference to global metrics
     */
    static AtomicMetrics& getInstance();
    
    /**
     * @brief Initialize global metrics with configuration
     * @param config Configuration for global metrics
     */
    static void initialize(const AtomicMetricsConfig& config = AtomicMetricsConfig{});
    
    /**
     * @brief Reset global metrics
     */
    static void reset();
    
    /**
     * @brief Get global metrics snapshot
     * @return Global metrics snapshot
     */
    static AtomicMetrics::MetricsSnapshot getSnapshot();
    
    /**
     * @brief Get formatted global metrics
     * @return Formatted global metrics string
     */
    static std::string getFormattedMetrics();
    
    /**
     * @brief Get global metrics as JSON
     * @return JSON formatted global metrics
     */
    static std::string getJsonMetrics();

private:
    static std::unique_ptr<AtomicMetrics> global_instance_;
    static std::mutex instance_mutex_;
};

/**
 * @brief RAII wrapper for timing measurements
 */
class ScopedTimer {
public:
    explicit ScopedTimer(AtomicMetrics& metrics, const std::string& operation = "");
    ~ScopedTimer();
    
    /**
     * @brief Stop timing and record the measurement
     * @param additional_data Additional data to record
     */
    void stop(size_t additional_data = 0);

private:
    AtomicMetrics& metrics_;
    std::string operation_;
    std::chrono::high_resolution_clock::time_point start_time_;
    bool stopped_;
};

// Convenience macros for easy metrics tracking
#ifdef TSDB_ENABLE_METRICS
    #define TSDB_METRICS_WRITE(bytes, duration) \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordWrite(bytes, duration)
    
    #define TSDB_METRICS_READ(bytes, duration) \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordRead(bytes, duration)
    
    #define TSDB_METRICS_CACHE_HIT() \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordCacheHit()
    
    #define TSDB_METRICS_CACHE_MISS() \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordCacheMiss()
    
    #define TSDB_METRICS_COMPRESSION(orig, comp, duration) \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordCompression(orig, comp, duration)
    
    #define TSDB_METRICS_DECOMPRESSION(comp, decomp, duration) \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordDecompression(comp, decomp, duration)
    
    #define TSDB_METRICS_ALLOCATION(bytes) \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordAllocation(bytes)
    
    #define TSDB_METRICS_DEALLOCATION(bytes) \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordDeallocation(bytes)
    
    #define TSDB_METRICS_TIMER(operation) \
        tsdb::storage::internal::ScopedTimer timer(tsdb::storage::internal::GlobalMetrics::getInstance(), operation)

    #define TSDB_METRICS_DROPPED_SAMPLE() \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordDroppedSample()

    #define TSDB_METRICS_DERIVED_SAMPLE() \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordDerivedSample()

    #define TSDB_METRICS_RULE_CHECK(duration) \
        tsdb::storage::internal::GlobalMetrics::getInstance().recordRuleCheck(duration)
#else
    #define TSDB_METRICS_WRITE(bytes, duration) ((void)0)
    #define TSDB_METRICS_READ(bytes, duration) ((void)0)
    #define TSDB_METRICS_CACHE_HIT() ((void)0)
    #define TSDB_METRICS_CACHE_MISS() ((void)0)
    #define TSDB_METRICS_COMPRESSION(orig, comp, duration) ((void)0)
    #define TSDB_METRICS_DECOMPRESSION(comp, decomp, duration) ((void)0)
    #define TSDB_METRICS_ALLOCATION(bytes) ((void)0)
    #define TSDB_METRICS_DEALLOCATION(bytes) ((void)0)
    #define TSDB_METRICS_TIMER(operation) ((void)0)
    #define TSDB_METRICS_DROPPED_SAMPLE() ((void)0)
    #define TSDB_METRICS_DERIVED_SAMPLE() ((void)0)
    #define TSDB_METRICS_RULE_CHECK(duration) ((void)0)
#endif

} // namespace internal
} // namespace storage
} // namespace tsdb 