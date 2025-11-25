#ifndef TSDB_STORAGE_WRITE_PERFORMANCE_INSTRUMENTATION_H_
#define TSDB_STORAGE_WRITE_PERFORMANCE_INSTRUMENTATION_H_

#include <chrono>
#include <string>
#include <map>
#include <atomic>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace tsdb {
namespace storage {

/**
 * @brief Performance instrumentation for write operations
 * 
 * This class provides detailed timing measurements for write operations
 * to help identify performance bottlenecks.
 */
class WritePerformanceInstrumentation {
public:
    struct TimingData {
        std::string operation;
        double time_us;  // Time in microseconds
        bool is_new_series;
        size_t num_samples;
        
        TimingData() : time_us(0.0), is_new_series(false), num_samples(0) {}
        TimingData(const std::string& op, double t, bool is_new, size_t samples)
            : operation(op), time_us(t), is_new_series(is_new), num_samples(samples) {}
    };
    
    struct WriteMetrics {
        double wal_write_us = 0.0;
        double series_id_calc_us = 0.0;
        double index_lookup_us = 0.0;
        double index_insert_us = 0.0;
        double series_creation_us = 0.0;
        double map_insert_us = 0.0;
        double sample_append_us = 0.0;
        double cache_update_us = 0.0;
        double block_seal_us = 0.0;
        double block_persist_us = 0.0;
        double mutex_lock_us = 0.0;
        double total_us = 0.0;
        bool is_new_series = false;
        size_t num_samples = 0;
        
        void reset() {
            wal_write_us = 0.0;
            series_id_calc_us = 0.0;
            index_lookup_us = 0.0;
            index_insert_us = 0.0;
            series_creation_us = 0.0;
            map_insert_us = 0.0;
            sample_append_us = 0.0;
            cache_update_us = 0.0;
            block_seal_us = 0.0;
            block_persist_us = 0.0;
            mutex_lock_us = 0.0;
            total_us = 0.0;
            is_new_series = false;
            num_samples = 0;
        }
        
        std::string to_csv() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3);
            oss << (is_new_series ? "new" : "update") << ","
                << num_samples << ","
                << total_us << ","
                << wal_write_us << ","
                << series_id_calc_us << ","
                << index_lookup_us << ","
                << index_insert_us << ","
                << series_creation_us << ","
                << map_insert_us << ","
                << sample_append_us << ","
                << cache_update_us << ","
                << block_seal_us << ","
                << block_persist_us << ","
                << mutex_lock_us;
            return oss.str();
        }
        
        static std::string csv_header() {
            return "type,num_samples,total_us,wal_write_us,series_id_calc_us,index_lookup_us,"
                   "index_insert_us,series_creation_us,map_insert_us,sample_append_us,"
                   "cache_update_us,block_seal_us,block_persist_us,mutex_lock_us";
        }
    };
    
    static WritePerformanceInstrumentation& instance() {
        static WritePerformanceInstrumentation inst;
        return inst;
    }
    
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool is_enabled() const { return enabled_; }
    
    void reset_stats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        new_series_count_ = 0;
        update_series_count_ = 0;
        new_series_total_us_ = 0.0;
        update_series_total_us_ = 0.0;
    }
    
    void record_write(const WriteMetrics& metrics) {
        if (!enabled_) return;
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        if (metrics.is_new_series) {
            new_series_count_++;
            new_series_total_us_ += metrics.total_us;
        } else {
            update_series_count_++;
            update_series_total_us_ += metrics.total_us;
        }
        
        // Debug output removed for production
    }
    
    void print_summary() {
        // Debug output removed for production - metrics still collected internally
        (void)enabled_;  // Suppress unused warning
    }
    
private:
    WritePerformanceInstrumentation() : enabled_(false) {}
    
    std::atomic<bool> enabled_;
    std::mutex stats_mutex_;
    size_t new_series_count_ = 0;
    size_t update_series_count_ = 0;
    double new_series_total_us_ = 0.0;
    double update_series_total_us_ = 0.0;
};

/**
 * @brief RAII timer for measuring operation duration
 */
class ScopedTimer {
public:
    ScopedTimer(double& output_us, bool enabled = true)
        : output_us_(output_us), enabled_(enabled), start_(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopedTimer() {
        if (enabled_) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
            output_us_ = duration.count() / 1000.0;  // Convert to microseconds
        }
    }
    
private:
    double& output_us_;
    bool enabled_;
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_WRITE_PERFORMANCE_INSTRUMENTATION_H_

