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
#include "tsdb/storage/atomic_metrics.h"

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
        double otel_conversion_us = 0.0;
        double grpc_handling_us = 0.0;
        double total_us = 0.0;
        bool is_new_series = false;
        size_t num_samples = 0;
        
        // Granular OTEL metrics
        double otel_resource_processing_us = 0.0;
        double otel_scope_processing_us = 0.0;
        double otel_metric_processing_us = 0.0;
        double otel_label_conversion_us = 0.0;
        double otel_point_conversion_us = 0.0;
        
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
            otel_conversion_us = 0.0;
            grpc_handling_us = 0.0;
            // Granular OTEL metrics
            otel_resource_processing_us = 0.0;
            otel_scope_processing_us = 0.0;
            otel_metric_processing_us = 0.0;
            otel_label_conversion_us = 0.0;
            otel_point_conversion_us = 0.0;
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
                << mutex_lock_us << ","
                << otel_conversion_us << ","
                << grpc_handling_us;
            return oss.str();
        }
        
        static std::string csv_header() {
            return "type,num_samples,total_us,wal_write_us,series_id_calc_us,index_lookup_us,"
                   "index_insert_us,series_creation_us,map_insert_us,sample_append_us,"
                   "cache_update_us,block_seal_us,block_persist_us,mutex_lock_us,"
                   "otel_conversion_us,grpc_handling_us";
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
        otel_conversion_total_us_ = 0.0;
        grpc_handling_total_us_ = 0.0;
        
        wal_write_total_us_ = 0.0;
        series_id_calc_total_us_ = 0.0;
        index_lookup_total_us_ = 0.0;
        index_insert_total_us_ = 0.0;
        series_creation_total_us_ = 0.0;
        map_insert_total_us_ = 0.0;
        sample_append_total_us_ = 0.0;
        cache_update_total_us_ = 0.0;
        block_seal_total_us_ = 0.0;
        block_persist_total_us_ = 0.0;
        mutex_lock_total_us_ = 0.0;
        
        otel_resource_processing_total_us_ = 0.0;
        otel_scope_processing_total_us_ = 0.0;
        otel_metric_processing_total_us_ = 0.0;
        otel_label_conversion_total_us_ = 0.0;
        otel_point_conversion_total_us_ = 0.0;
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
        
        otel_conversion_total_us_ += metrics.otel_conversion_us;
        grpc_handling_total_us_ += metrics.grpc_handling_us;
        
        // Aggregate detailed component metrics
        wal_write_total_us_ += metrics.wal_write_us;
        series_id_calc_total_us_ += metrics.series_id_calc_us;
        index_lookup_total_us_ += metrics.index_lookup_us;
        index_insert_total_us_ += metrics.index_insert_us;
        series_creation_total_us_ += metrics.series_creation_us;
        map_insert_total_us_ += metrics.map_insert_us;
        sample_append_total_us_ += metrics.sample_append_us;
        cache_update_total_us_ += metrics.cache_update_us;
        block_seal_total_us_ += metrics.block_seal_us;
        block_persist_total_us_ += metrics.block_persist_us;
        mutex_lock_total_us_ += metrics.mutex_lock_us;
        
        otel_resource_processing_total_us_ += metrics.otel_resource_processing_us;
        otel_scope_processing_total_us_ += metrics.otel_scope_processing_us;
        otel_metric_processing_total_us_ += metrics.otel_metric_processing_us;
        otel_label_conversion_total_us_ += metrics.otel_label_conversion_us;
        otel_point_conversion_total_us_ += metrics.otel_point_conversion_us;
        
        // Debug output removed for production
        
        // Record to GlobalMetrics for self-monitoring
        tsdb::storage::internal::GlobalMetrics::getInstance().recordWrite(
            metrics.num_samples * 16, // Approximate bytes (timestamp+value)
            static_cast<uint64_t>(metrics.total_us * 1000) // Convert us to ns
        );
    }
    
    void print_summary() {
        auto stats = get_stats();
        size_t total_writes = stats.new_series_count + stats.update_series_count;
        if (total_writes == 0) {
            std::cout << "Write Performance Summary: No writes recorded." << std::endl;
            return;
        }

        double total_time_ms = (stats.new_series_total_us + stats.update_series_total_us) / 1000.0;
        double avg_time_us = (stats.new_series_total_us + stats.update_series_total_us) / total_writes;

        std::cout << "\n=== Write Performance Summary ===" << std::endl;
        std::cout << "Total Writes: " << total_writes << " (New: " << stats.new_series_count 
                  << ", Update: " << stats.update_series_count << ")" << std::endl;
        std::cout << "Total Time: " << std::fixed << std::setprecision(2) << total_time_ms << " ms" << std::endl;
        std::cout << "Avg Time per Write: " << avg_time_us << " us" << std::endl;
        
        std::cout << "\n--- Component Breakdown (Avg per Write) ---" << std::endl;
        auto print_metric = [&](const char* name, double total_us) {
            std::cout << std::left << std::setw(25) << name << ": " 
                      << std::right << std::setw(8) << (total_us / total_writes) << " us ("
                      << std::setw(5) << (total_us / (stats.new_series_total_us + stats.update_series_total_us) * 100.0) << "%)" << std::endl;
        };

        print_metric("WAL Write", stats.wal_write_total_us);
        print_metric("Series ID Calc", stats.series_id_calc_total_us);
        print_metric("Map Insert (Lock)", stats.map_insert_total_us);
        print_metric("Index Insert", stats.index_insert_total_us);
        print_metric("Series Creation", stats.series_creation_total_us);
        print_metric("Sample Append", stats.sample_append_total_us);
        print_metric("Block Seal", stats.block_seal_total_us);
        print_metric("Block Persist", stats.block_persist_total_us);
        print_metric("Cache Update", stats.cache_update_total_us);
        print_metric("Mutex Wait", stats.mutex_lock_total_us);
        
        std::cout << "\n--- OTEL/gRPC Overhead (Avg per Write) ---" << std::endl;
        print_metric("gRPC Handling", stats.grpc_handling_total_us);
        print_metric("OTEL Conversion", stats.otel_conversion_total_us);
        
        std::cout << "\n--- OTEL Conversion Breakdown (Avg per Write) ---" << std::endl;
        print_metric("  Resource Processing", stats.otel_resource_processing_total_us);
        print_metric("  Scope Processing", stats.otel_scope_processing_total_us);
        print_metric("  Metric Processing", stats.otel_metric_processing_total_us);
        print_metric("  Label Conversion", stats.otel_label_conversion_total_us);
        print_metric("  Point Conversion", stats.otel_point_conversion_total_us);
        
        std::cout << "=================================\n" << std::endl;
    }
    
    struct WriteStats {
        size_t new_series_count = 0;
        size_t update_series_count = 0;
        double new_series_total_us = 0.0;
        double update_series_total_us = 0.0;
        double otel_conversion_total_us = 0.0;
        double grpc_handling_total_us = 0.0;
        // Detailed component breakdown
        double wal_write_total_us = 0.0;
        double series_id_calc_total_us = 0.0;
        double index_lookup_total_us = 0.0;
        double index_insert_total_us = 0.0;
        double series_creation_total_us = 0.0;
        double map_insert_total_us = 0.0;
        double sample_append_total_us = 0.0;
        double cache_update_total_us = 0.0;
        double block_seal_total_us = 0.0;
        double block_persist_total_us = 0.0;
        double mutex_lock_total_us = 0.0;
        
        // Granular OTEL metrics
        double otel_resource_processing_total_us = 0.0;
        double otel_scope_processing_total_us = 0.0;
        double otel_metric_processing_total_us = 0.0;
        double otel_label_conversion_total_us = 0.0;
        double otel_point_conversion_total_us = 0.0;
    };

    WriteStats get_stats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        WriteStats stats;
        stats.new_series_count = new_series_count_;
        stats.update_series_count = update_series_count_;
        stats.new_series_total_us = new_series_total_us_;
        stats.update_series_total_us = update_series_total_us_;
        stats.otel_conversion_total_us = otel_conversion_total_us_;
        stats.grpc_handling_total_us = grpc_handling_total_us_;
        stats.wal_write_total_us = wal_write_total_us_;
        stats.series_id_calc_total_us = series_id_calc_total_us_;
        stats.index_lookup_total_us = index_lookup_total_us_;
        stats.index_insert_total_us = index_insert_total_us_;
        stats.series_creation_total_us = series_creation_total_us_;
        stats.map_insert_total_us = map_insert_total_us_;
        stats.sample_append_total_us = sample_append_total_us_;
        stats.cache_update_total_us = cache_update_total_us_;
        stats.block_seal_total_us = block_seal_total_us_;
        stats.block_persist_total_us = block_persist_total_us_;
        stats.mutex_lock_total_us = mutex_lock_total_us_;
        
        stats.otel_resource_processing_total_us = otel_resource_processing_total_us_;
        stats.otel_scope_processing_total_us = otel_scope_processing_total_us_;
        stats.otel_metric_processing_total_us = otel_metric_processing_total_us_;
        stats.otel_label_conversion_total_us = otel_label_conversion_total_us_;
        stats.otel_point_conversion_total_us = otel_point_conversion_total_us_;
        
        return stats;
    }
    
private:
    WritePerformanceInstrumentation() : enabled_(false) {}
    
    std::atomic<bool> enabled_;
    std::mutex stats_mutex_;
    size_t new_series_count_ = 0;
    size_t update_series_count_ = 0;
    double new_series_total_us_ = 0.0;
    double update_series_total_us_ = 0.0;
    double otel_conversion_total_us_ = 0.0;
    double grpc_handling_total_us_ = 0.0;
    
    // Detailed component breakdown
    double wal_write_total_us_ = 0.0;
    double series_id_calc_total_us_ = 0.0;
    double index_lookup_total_us_ = 0.0;
    double index_insert_total_us_ = 0.0;
    double series_creation_total_us_ = 0.0;
    double map_insert_total_us_ = 0.0;
    double sample_append_total_us_ = 0.0;
    double cache_update_total_us_ = 0.0;
    double block_seal_total_us_ = 0.0;
    double block_persist_total_us_ = 0.0;
    double mutex_lock_total_us_ = 0.0;
    
    // Granular OTEL metrics
    double otel_resource_processing_total_us_ = 0.0;
    double otel_scope_processing_total_us_ = 0.0;
    double otel_metric_processing_total_us_ = 0.0;
    double otel_label_conversion_total_us_ = 0.0;
    double otel_point_conversion_total_us_ = 0.0;
};

class WriteScopedTimer {
public:
    WriteScopedTimer(double& output_us, bool enabled = true)
        : output_us_(output_us), enabled_(enabled), start_(std::chrono::high_resolution_clock::now()) {}
    
    ~WriteScopedTimer() {
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

