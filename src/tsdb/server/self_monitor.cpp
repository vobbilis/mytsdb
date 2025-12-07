#include "tsdb/server/self_monitor.h"
#include "tsdb/prometheus/promql/query_metrics.h"
#include "tsdb/storage/atomic_metrics.h"
#include "tsdb/storage/read_performance_instrumentation.h"
#include "tsdb/storage/write_performance_instrumentation.h"
#include "tsdb/core/types.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <cmath> // For std::isinf

namespace tsdb {
namespace server {

SelfMonitor::SelfMonitor(std::shared_ptr<storage::Storage> storage, 
                         std::shared_ptr<storage::BackgroundProcessor> background_processor)
    : storage_(storage), background_processor_(background_processor) {}

void SelfMonitor::Start() {
    if (running_.exchange(true)) {
        std::cout << "[SelfMonitor] Already running, skipping start" << std::endl;
        return;
    }
    
    std::cout << "[SelfMonitor] Starting self-monitoring thread..." << std::endl;
    
    // Schedule periodic scrape
    std::thread([this]() {
        std::cout << "[SelfMonitor] Thread started, will scrape every 1 second" << std::endl;
        int iteration = 0;
        while (running_) {
            // Scrape every 1 second for better responsiveness
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!running_) break;
            
            iteration++;
            std::cout << "[SelfMonitor] Iteration " << iteration << ": Submitting scrape task..." << std::endl;
            
            // Submit scrape task to background processor
            background_processor_->submitTask(storage::BackgroundTask(
                storage::BackgroundTaskType::CLEANUP, // Use CLEANUP type for now
                [this, iteration]() {
                    std::cout << "[SelfMonitor] Executing scrape task (iteration " << iteration << ")" << std::endl;
                    ScrapeAndWrite();
                    std::cout << "[SelfMonitor] Scrape task completed (iteration " << iteration << ")" << std::endl;
                    return core::Result<void>();
                },
                10 // Low priority
            ));
        }
        std::cout << "[SelfMonitor] Thread exiting" << std::endl;
    }).detach();
}

void SelfMonitor::Stop() {
    std::cout << "[SelfMonitor] Stopping..." << std::endl;
    running_ = false;
}

void SelfMonitor::ScrapeAndWrite() {
    std::cout << "[SelfMonitor] ScrapeAndWrite called" << std::endl;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto query_snapshot = prometheus::promql::QueryMetrics::GetInstance().GetSnapshot();
    auto storage_snapshot = storage::internal::GlobalMetrics::getInstance().getSnapshot();

    std::cout << "[SelfMonitor] Query count: " << query_snapshot.query_count 
              << ", Storage writes: " << storage_snapshot.write_count << std::endl;

    std::vector<core::TimeSeries> metrics;

    auto add_metric = [&](const std::string& name, double value, const std::string& type = "gauge") {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("job", "mytsdb_self_monitor");
        labels.add("instance", "localhost");
        
        core::TimeSeries ts(labels);
        ts.add_sample(core::Sample(now, value));
        metrics.push_back(std::move(ts));
    };

    // Query Metrics
    add_metric("mytsdb_query_count_total", query_snapshot.query_count, "counter");
    add_metric("mytsdb_query_errors_total", query_snapshot.query_errors, "counter");
    add_metric("mytsdb_query_duration_seconds_total", query_snapshot.total_query_time_ns / 1e9, "counter");
    add_metric("mytsdb_query_parse_duration_seconds_total", query_snapshot.total_parse_time_ns / 1e9, "counter");
    add_metric("mytsdb_query_eval_duration_seconds_total", query_snapshot.total_eval_time_ns / 1e9, "counter");
    add_metric("mytsdb_query_exec_duration_seconds_total", query_snapshot.total_exec_time_ns / 1e9, "counter");
    add_metric("mytsdb_query_storage_read_duration_seconds_total", query_snapshot.total_storage_read_time_ns / 1e9, "counter");
    add_metric("mytsdb_query_samples_scanned_total", query_snapshot.samples_scanned, "counter");
    add_metric("mytsdb_query_series_scanned_total", query_snapshot.series_scanned, "counter");
    add_metric("mytsdb_query_bytes_scanned_total", query_snapshot.bytes_scanned, "counter");

    add_metric("mytsdb_query_bytes_scanned_total", query_snapshot.bytes_scanned, "counter");

    // Histogram Buckets (Accumulate for Prometheus LE)
    uint64_t cumulative_count = 0;
    for (const auto& bucket : query_snapshot.query_duration_buckets) {
        cumulative_count += bucket.second;
        std::string le;
        if (std::isinf(bucket.first)) {
            le = "+Inf";
        } else {
            // Avoid scientific notation/trailing zeros for cleanliness
	          std::ostringstream out;
	          out.precision(3); // Match bounds precision
            out << std::fixed << bucket.first;
            le = out.str();
            // Trim trailing zeros if needed, or just let it be. Standard Prom usage is fine with 0.100 or 0.1.
            // Let's use simple extraction.
            size_t dot_pos = le.find('.');
            if (dot_pos != std::string::npos) {
                // Remove trailing zeros
                le.erase(le.find_last_not_of('0') + 1, std::string::npos); 
                // Remove trailing dot
                if (le.back() == '.') le.pop_back();
            }
        }
        
        core::Labels labels;
        labels.add("__name__", "mytsdb_query_duration_seconds_bucket");
        labels.add("job", "mytsdb_self_monitor");
        labels.add("instance", "localhost");
        labels.add("le", le);
        
        core::TimeSeries ts(labels);
        ts.add_sample(core::Sample(now, cumulative_count));
        metrics.push_back(std::move(ts));
    }
    
    // Histogram Count and Sum
    add_metric("mytsdb_query_duration_seconds_count", query_snapshot.query_count, "counter");
    add_metric("mytsdb_query_duration_seconds_sum", query_snapshot.total_query_time_ns / 1e9, "counter");
    add_metric("mytsdb_storage_writes_total", storage_snapshot.write_count, "counter");
    add_metric("mytsdb_storage_reads_total", storage_snapshot.read_count, "counter");
    add_metric("mytsdb_storage_cache_hits_total", storage_snapshot.cache_hits, "counter");
    add_metric("mytsdb_storage_cache_misses_total", storage_snapshot.cache_misses, "counter");
    add_metric("mytsdb_storage_bytes_written_total", storage_snapshot.bytes_written, "counter");
    add_metric("mytsdb_storage_bytes_read_total", storage_snapshot.bytes_read, "counter");
    add_metric("mytsdb_storage_net_memory_usage_bytes", storage_snapshot.net_memory_usage, "gauge");

    // Write Path Performance Metrics
    auto write_stats = storage::WritePerformanceInstrumentation::instance().get_stats();
    add_metric("mytsdb_write_otel_conversion_seconds_total", write_stats.otel_conversion_total_us / 1e6, "counter");
    add_metric("mytsdb_write_grpc_handling_seconds_total", write_stats.grpc_handling_total_us / 1e6, "counter");
    
    // Detailed Write Path Breakdown
    add_metric("mytsdb_write_wal_write_seconds_total", write_stats.wal_write_total_us / 1e6, "counter");
    add_metric("mytsdb_write_series_id_calc_seconds_total", write_stats.series_id_calc_total_us / 1e6, "counter");
    add_metric("mytsdb_write_index_insert_seconds_total", write_stats.index_insert_total_us / 1e6, "counter");
    add_metric("mytsdb_write_series_creation_seconds_total", write_stats.series_creation_total_us / 1e6, "counter");
    add_metric("mytsdb_write_map_insert_seconds_total", write_stats.map_insert_total_us / 1e6, "counter");
    add_metric("mytsdb_write_sample_append_seconds_total", write_stats.sample_append_total_us / 1e6, "counter");
    add_metric("mytsdb_write_cache_update_seconds_total", write_stats.cache_update_total_us / 1e6, "counter");
    add_metric("mytsdb_write_block_seal_seconds_total", write_stats.block_seal_total_us / 1e6, "counter");
    add_metric("mytsdb_write_block_persist_seconds_total", write_stats.block_persist_total_us / 1e6, "counter");
    add_metric("mytsdb_write_mutex_lock_seconds_total", write_stats.mutex_lock_total_us / 1e6, "counter");

    // Granular OTEL Metrics
    add_metric("mytsdb_write_otel_resource_processing_seconds_total", write_stats.otel_resource_processing_total_us / 1e6, "counter");
    add_metric("mytsdb_write_otel_scope_processing_seconds_total", write_stats.otel_scope_processing_total_us / 1e6, "counter");
    add_metric("mytsdb_write_otel_metric_processing_seconds_total", write_stats.otel_metric_processing_total_us / 1e6, "counter");
    add_metric("mytsdb_write_otel_label_conversion_seconds_total", write_stats.otel_label_conversion_total_us / 1e6, "counter");
    add_metric("mytsdb_write_otel_point_conversion_seconds_total", write_stats.otel_point_conversion_total_us / 1e6, "counter");

    // Read Path Performance Metrics
    auto read_stats = storage::ReadPerformanceInstrumentation::instance().get_stats();
    add_metric("mytsdb_read_total", read_stats.total_reads, "counter");
    add_metric("mytsdb_read_duration_seconds_total", read_stats.total_time_us / 1e6, "counter");
    add_metric("mytsdb_read_index_search_seconds_total", read_stats.total_index_search_us / 1e6, "counter");
    add_metric("mytsdb_read_block_lookup_seconds_total", read_stats.total_block_lookup_us / 1e6, "counter");
    add_metric("mytsdb_read_block_read_seconds_total", read_stats.total_block_read_us / 1e6, "counter");
    add_metric("mytsdb_read_decompression_seconds_total", read_stats.total_decompression_us / 1e6, "counter");
    add_metric("mytsdb_read_samples_scanned_total", read_stats.total_samples_scanned, "counter");
    add_metric("mytsdb_read_blocks_accessed_total", read_stats.total_blocks_accessed, "counter");
    add_metric("mytsdb_read_cache_hits_total", read_stats.cache_hits, "counter");
    
    // Secondary Index Metrics (Phase A: B+ Tree)
    add_metric("mytsdb_secondary_index_lookups_total", read_stats.secondary_index_lookups, "counter");
    add_metric("mytsdb_secondary_index_hits_total", read_stats.secondary_index_hits, "counter");
    add_metric("mytsdb_secondary_index_misses_total", read_stats.secondary_index_misses, "counter");
    add_metric("mytsdb_secondary_index_lookup_seconds_total", read_stats.secondary_index_lookup_time_us / 1e6, "counter");
    add_metric("mytsdb_secondary_index_build_seconds_total", read_stats.secondary_index_build_time_us / 1e6, "counter");
    add_metric("mytsdb_secondary_index_row_groups_selected_total", read_stats.secondary_index_row_groups_selected, "counter");

    std::cout << "[SelfMonitor] Writing " << metrics.size() << " metric series to storage..." << std::endl;
    
    // Write to storage
    int written = 0;
    for (const auto& ts : metrics) {
        auto result = storage_->write(ts);
        if (result.ok()) {
            written++;
        } else {
            std::cerr << "[SelfMonitor] Failed to write metric: " << result.error() << std::endl;
        }
    }
    
    std::cout << "[SelfMonitor] Successfully wrote " << written << "/" << metrics.size() << " metrics" << std::endl;
}

} // namespace server
} // namespace tsdb
