#ifndef TSDB_STORAGE_READ_PERFORMANCE_INSTRUMENTATION_H_
#define TSDB_STORAGE_READ_PERFORMANCE_INSTRUMENTATION_H_

#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace tsdb {
namespace storage {

/**
 * @brief Performance instrumentation for read operations
 */
class ReadPerformanceInstrumentation {
public:
    struct ReadMetrics {
        double index_search_us = 0.0;
        double block_lookup_us = 0.0;
        double block_read_us = 0.0;
        double decompression_us = 0.0;
        double total_us = 0.0;
        size_t samples_scanned = 0;
        size_t blocks_accessed = 0;
        bool cache_hit = false;

        void reset() {
            index_search_us = 0.0;
            block_lookup_us = 0.0;
            block_read_us = 0.0;
            decompression_us = 0.0;
            total_us = 0.0;
            samples_scanned = 0;
            blocks_accessed = 0;
            cache_hit = false;
        }
    };

    static ReadPerformanceInstrumentation& instance() {
        static ReadPerformanceInstrumentation inst;
        return inst;
    }

    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool is_enabled() const { return enabled_; }

    void record_read(const ReadMetrics& metrics) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_reads_++;
        total_time_us_ += metrics.total_us;
        total_index_search_us_ += metrics.index_search_us;
        total_block_lookup_us_ += metrics.block_lookup_us;
        total_block_read_us_ += metrics.block_read_us;
        total_decompression_us_ += metrics.decompression_us;
        total_samples_scanned_ += metrics.samples_scanned;
        total_blocks_accessed_ += metrics.blocks_accessed;
        if (metrics.cache_hit) cache_hits_++;
    }

    // Getters for SelfMonitor
    struct AggregateStats {
        uint64_t total_reads;
        double total_time_us;
        double total_index_search_us;
        double total_block_lookup_us;
        double total_block_read_us;
        double total_decompression_us;
        uint64_t total_samples_scanned;
        uint64_t total_blocks_accessed;
        uint64_t cache_hits;
    };

    AggregateStats get_stats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return {
            total_reads_,
            total_time_us_,
            total_index_search_us_,
            total_block_lookup_us_,
            total_block_read_us_,
            total_decompression_us_,
            total_samples_scanned_,
            total_blocks_accessed_,
            cache_hits_
        };
    }

    void reset_stats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_reads_ = 0;
        total_time_us_ = 0;
        total_index_search_us_ = 0;
        total_block_lookup_us_ = 0;
        total_block_read_us_ = 0;
        total_decompression_us_ = 0;
        total_samples_scanned_ = 0;
        total_blocks_accessed_ = 0;
        cache_hits_ = 0;
    }

private:
    ReadPerformanceInstrumentation() : enabled_(true) {}

    std::atomic<bool> enabled_;
    mutable std::mutex stats_mutex_;
    
    uint64_t total_reads_ = 0;
    double total_time_us_ = 0;
    double total_index_search_us_ = 0;
    double total_block_lookup_us_ = 0;
    double total_block_read_us_ = 0;
    double total_decompression_us_ = 0;
    uint64_t total_samples_scanned_ = 0;
    uint64_t total_blocks_accessed_ = 0;
    uint64_t cache_hits_ = 0;
};

class ReadScopedTimer {
public:
    ReadScopedTimer(double& output_us, bool enabled = true)
        : output_us_(output_us), enabled_(enabled), start_(std::chrono::high_resolution_clock::now()) {}
    
    ~ReadScopedTimer() {
        stop();
    }

    void stop() {
        if (enabled_ && !stopped_) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
            output_us_ += duration.count() / 1000.0;  // Add to existing value
            stopped_ = true;
        }
    }
    
private:
    double& output_us_;
    bool enabled_;
    std::chrono::high_resolution_clock::time_point start_;
    bool stopped_ = false;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_READ_PERFORMANCE_INSTRUMENTATION_H_
