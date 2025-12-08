/**
 * K8s Combined Benchmark Tool
 * 
 * Simulates a realistic K8s monitoring workload with:
 * - Concurrent writes (ingesting K8s metrics)
 * - Concurrent reads (Grafana dashboard queries)
 * - Performance metrics (p50/p99 latencies, throughput)
 * 
 * Based on the approved Large-Scale Realistic K8s Benchmark Specification:
 * - 9,000 pods (3 regions × 3 zones × 10 namespaces × 20 services × 5 pods)
 * - 100 metric types per container
 * - 12 label dimensions
 * - 25 Grafana dashboard panels with mixed hot/cold queries
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <iomanip>
#include <mutex>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <fstream>

#include <httplib.h>

#include <arrow/api.h>
#include <arrow/flight/api.h>

// ============================================================================
// Configuration
// ============================================================================

struct BenchmarkConfig {
    // Connection settings
    std::string arrow_host = "localhost";
    int arrow_port = 8815;
    std::string http_address = "localhost:9090";
    
    // K8s topology
    int regions = 3;
    int zones_per_region = 3;
    int namespaces_per_cluster = 10;
    int services_per_namespace = 20;
    int pods_per_service = 5;
    int containers_per_pod = 2;
    int metric_types = 100;
    
    // Write benchmark
    int write_workers = 4;
    int write_batch_size = 2000;
    int write_duration_sec = 60;
    int samples_per_metric = 10;
    
    // Read benchmark
    int read_workers = 4;
    int read_duration_sec = 60;
    int dashboard_panels = 25;
    double hot_query_ratio = 0.8;       // 80% hot queries (User requested "a lot more")
    
    // Presets
    std::string preset = "quick";
    
    // Data Management
    bool clean_start = false;
    bool generate_10m = false;
    
    void apply_preset() {
        if (preset == "quick") {
            regions = 1; zones_per_region = 1; namespaces_per_cluster = 2;
            services_per_namespace = 3; pods_per_service = 2;
            metric_types = 10; write_duration_sec = 10; read_duration_sec = 10;
        } else if (preset == "small") {
            regions = 1; zones_per_region = 1; namespaces_per_cluster = 5;
            services_per_namespace = 10; pods_per_service = 3;
            metric_types = 50; write_duration_sec = 30; read_duration_sec = 30;
        } else if (preset == "medium") {
            regions = 2; zones_per_region = 2; namespaces_per_cluster = 10;
            services_per_namespace = 15; pods_per_service = 4;
            metric_types = 100; write_duration_sec = 60; read_duration_sec = 60;
        } else if (preset == "large") {
            // Full 9K pod benchmark
            regions = 3; zones_per_region = 3; namespaces_per_cluster = 10;
            services_per_namespace = 20; pods_per_service = 5;
            metric_types = 100; write_duration_sec = 300; read_duration_sec = 300;
        }
    }
    
    int total_pods() const {
        return regions * zones_per_region * namespaces_per_cluster * 
               services_per_namespace * pods_per_service;
    }
    
    int total_time_series() const {
        return total_pods() * containers_per_pod * metric_types;
    }
};

// ============================================================================
// Latency Tracker
// ============================================================================

class LatencyTracker {
public:
    void record(double latency_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        latencies_.push_back(latency_ms);
    }
    
    double percentile(double p) const {
        if (latencies_.empty()) return 0.0;
        std::vector<double> sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = static_cast<size_t>(p / 100.0 * (sorted.size() - 1));
        return sorted[idx];
    }
    
    double mean() const {
        if (latencies_.empty()) return 0.0;
        return std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
    }
    
    size_t count() const { return latencies_.size(); }
    
    double min() const {
        if (latencies_.empty()) return 0.0;
        return *std::min_element(latencies_.begin(), latencies_.end());
    }
    
    double max() const {
        if (latencies_.empty()) return 0.0;
        return *std::max_element(latencies_.begin(), latencies_.end());
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        latencies_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<double> latencies_;
};

// ============================================================================
// K8s Metric Names
// ============================================================================

const std::vector<std::string> K8S_METRICS = {
    // Container metrics (20)
    "container_cpu_usage_seconds_total", "container_cpu_user_seconds_total",
    "container_cpu_system_seconds_total", "container_memory_usage_bytes",
    "container_memory_working_set_bytes", "container_memory_rss",
    "container_network_receive_bytes_total", "container_network_transmit_bytes_total",
    "container_network_receive_packets_total", "container_network_transmit_packets_total",
    "container_fs_reads_bytes_total", "container_fs_writes_bytes_total",
    "container_fs_usage_bytes", "container_fs_limit_bytes",
    "container_cpu_cfs_throttled_seconds_total", "container_cpu_cfs_periods_total",
    "container_memory_cache", "container_memory_swap",
    "container_last_seen", "container_start_time_seconds",
    
    // kube-state-metrics (20)
    "kube_pod_status_phase", "kube_pod_status_ready",
    "kube_pod_container_status_running", "kube_pod_container_status_waiting",
    "kube_pod_container_status_terminated", "kube_pod_container_status_restarts_total",
    "kube_deployment_status_replicas", "kube_deployment_status_replicas_available",
    "kube_deployment_status_replicas_unavailable", "kube_deployment_spec_replicas",
    "kube_statefulset_replicas", "kube_statefulset_replicas_ready",
    "kube_daemonset_status_number_ready", "kube_daemonset_status_desired_number_scheduled",
    "kube_service_info", "kube_endpoint_info",
    "kube_namespace_status_phase", "kube_node_status_condition",
    "kube_node_status_allocatable", "kube_node_status_capacity",
    
    // Node metrics (20)
    "node_cpu_seconds_total", "node_memory_MemTotal_bytes",
    "node_memory_MemFree_bytes", "node_memory_MemAvailable_bytes",
    "node_memory_Buffers_bytes", "node_memory_Cached_bytes",
    "node_disk_read_bytes_total", "node_disk_written_bytes_total",
    "node_disk_reads_completed_total", "node_disk_writes_completed_total",
    "node_network_receive_bytes_total", "node_network_transmit_bytes_total",
    "node_filesystem_size_bytes", "node_filesystem_free_bytes",
    "node_filesystem_avail_bytes", "node_load1",
    "node_load5", "node_load15",
    "node_context_switches_total", "node_interrupts_total",
    
    // HTTP/gRPC service metrics (25) - includes histogram buckets
    "http_requests_total", "http_request_duration_seconds",
    "http_request_duration_seconds_bucket",  // For histogram_quantile
    "http_request_size_bytes", "http_response_size_bytes",
    "http_requests_in_flight", "grpc_server_started_total",
    "grpc_server_handled_total", "grpc_server_msg_received_total",
    "grpc_server_msg_sent_total", "grpc_server_handling_seconds",
    "grpc_server_handling_seconds_bucket",  // For histogram_quantile
    "grpc_client_started_total", "grpc_client_handled_total",
    "grpc_client_msg_received_total", "grpc_client_msg_sent_total",
    "grpc_client_handling_seconds", "request_latency_seconds",
    "request_count_total", "error_count_total",
    "connection_pool_size", "connection_pool_available",
    
    // Application metrics (20)
    "process_cpu_seconds_total", "process_resident_memory_bytes",
    "process_virtual_memory_bytes", "process_open_fds",
    "process_max_fds", "process_start_time_seconds",
    "go_goroutines", "go_threads",
    "go_gc_duration_seconds", "go_memstats_alloc_bytes",
    "go_memstats_heap_alloc_bytes", "go_memstats_heap_inuse_bytes",
    "go_memstats_stack_inuse_bytes", "go_memstats_gc_cpu_fraction",
    "jvm_memory_used_bytes", "jvm_memory_committed_bytes",
    "jvm_gc_collection_seconds", "jvm_threads_current",
    "python_gc_collections_total", "python_info"
};

// Standard Prometheus histogram bucket boundaries for latency metrics
const std::vector<std::string> HISTOGRAM_LE_BUCKETS = {
    "0.005", "0.01", "0.025", "0.05", "0.1", "0.25", "0.5", "1", "2.5", "5", "10", "+Inf"
};

// ============================================================================
// Grafana Dashboard Queries
// ============================================================================

enum class QueryType { Hot, Cold, Other };

struct DashboardQuery {
    std::string name;
    std::string query;
    std::string duration;  // Range for range queries
    std::string step;
    bool is_instant;
    QueryType type;
};

std::vector<DashboardQuery> get_dashboard_queries(const BenchmarkConfig& config) {
    return {
        // =====================================================================
        // HOT TIER - Instant Queries (last 5 minutes)
        // =====================================================================
        {"CPU Usage", "sum(rate(container_cpu_usage_seconds_total[5m])) by (namespace)", "", "", true, QueryType::Hot},
        {"Memory Usage", "sum(container_memory_working_set_bytes) by (namespace)", "", "", true, QueryType::Hot},
        {"Pod Count", "count(kube_pod_status_phase) by (namespace, phase)", "", "", true, QueryType::Hot},
        {"Network I/O", "sum(rate(container_network_receive_bytes_total[5m]))", "", "", true, QueryType::Hot},
        {"Disk I/O", "sum(rate(container_fs_reads_bytes_total[5m]))", "", "", true, QueryType::Hot},
        
        // =====================================================================
        // HOT TIER - Range Queries (last 1 hour)
        // =====================================================================
        {"CPU Trend 1h", "sum(rate(container_cpu_usage_seconds_total[5m])) by (namespace)", "1h", "60s", false, QueryType::Hot},
        {"Memory Trend 1h", "sum(container_memory_working_set_bytes) by (namespace)", "1h", "60s", false, QueryType::Hot},
        {"Request Rate 1h", "sum(rate(http_requests_total[5m])) by (service)", "1h", "60s", false, QueryType::Hot},
        {"Error Rate 1h", "sum(rate(http_requests_total{status=~\"5..\"}[5m]))", "1h", "60s", false, QueryType::Hot},
        {"Latency P99 1h", "histogram_quantile(0.99, rate(http_request_duration_seconds_bucket[5m]))", "1h", "60s", false, QueryType::Hot},
        
        // =====================================================================
        // NEW: _over_time AGGREGATIONS - Test all new PromQL functions!
        // =====================================================================
        // sum_over_time - Total accumulated value over window
        {"Sum Over Time - CPU", "sum_over_time(container_cpu_usage_seconds_total[1h])", "", "", true, QueryType::Hot},
        {"Sum Over Time - Requests", "sum_over_time(http_requests_total[30m]) by (service)", "", "", true, QueryType::Hot},
        
        // avg_over_time - Average value over window
        {"Avg Over Time - Memory", "avg_over_time(container_memory_working_set_bytes[1h])", "", "", true, QueryType::Hot},
        {"Avg Over Time - Latency", "avg_over_time(http_request_duration_seconds[30m]) by (service)", "", "", true, QueryType::Hot},
        
        // min_over_time - Minimum value over window
        {"Min Over Time - Memory", "min_over_time(container_memory_working_set_bytes[1h]) by (namespace)", "", "", true, QueryType::Hot},
        {"Min Over Time - CPU", "min_over_time(node_cpu_seconds_total[1h]) by (node)", "", "", true, QueryType::Hot},
        
        // max_over_time - Maximum value over window (capacity planning)
        {"Max Over Time - Memory Peak", "max_over_time(container_memory_working_set_bytes[1h]) by (namespace)", "", "", true, QueryType::Hot},
        {"Max Over Time - CPU Peak", "max_over_time(node_load5[6h]) by (node)", "", "", true, QueryType::Cold},
        
        // count_over_time - Number of samples in window
        {"Count Over Time - Samples", "count_over_time(http_requests_total[1h]) by (service)", "", "", true, QueryType::Hot},
        {"Count Over Time - Pods", "count_over_time(kube_pod_status_ready[30m]) by (namespace)", "", "", true, QueryType::Hot},
        
        // =====================================================================
        // NEW: Counter Helpers
        // =====================================================================
        // resets - Counter reset detection
        {"Counter Resets - HTTP", "resets(http_requests_total[1h]) by (service)", "", "", true, QueryType::Hot},
        {"Counter Resets - Errors", "resets(error_count_total[6h]) by (service)", "", "", true, QueryType::Cold},
        
        // idelta - Instant delta (last two samples)
        {"Instant Delta - Requests", "idelta(http_requests_total[5m])", "", "", true, QueryType::Hot},
        {"Instant Delta - Errors", "idelta(error_count_total[5m]) by (service)", "", "", true, QueryType::Hot},
        
        // =====================================================================
        // NEW: Timestamp Function
        // =====================================================================
        {"Sample Timestamps", "timestamp(http_requests_total)", "", "", true, QueryType::Hot},
        {"Timestamp by Service", "timestamp(kube_pod_status_ready) by (namespace)", "", "", true, QueryType::Hot},
        
        // =====================================================================
        // NEW: Histogram Quantiles (P50, P90, P95, P99)
        // =====================================================================
        {"Latency P50", "histogram_quantile(0.50, rate(http_request_duration_seconds_bucket[5m]))", "", "", true, QueryType::Hot},
        {"Latency P90", "histogram_quantile(0.90, rate(http_request_duration_seconds_bucket[5m]))", "", "", true, QueryType::Hot},
        {"Latency P95", "histogram_quantile(0.95, rate(http_request_duration_seconds_bucket[5m]))", "", "", true, QueryType::Hot},
        {"Latency P99.9", "histogram_quantile(0.999, rate(http_request_duration_seconds_bucket[5m]))", "", "", true, QueryType::Hot},
        {"Latency P99 by Service", "histogram_quantile(0.99, sum(rate(http_request_duration_seconds_bucket[5m])) by (service, le))", "", "", true, QueryType::Hot},
        
        // =====================================================================
        // COLD TIER - Range Queries (6h-24h)
        // =====================================================================
        {"CPU Trend 6h", "avg(rate(container_cpu_usage_seconds_total[5m])) by (region)", "6h", "300s", false, QueryType::Cold},
        {"Memory Trend 6h", "avg(container_memory_working_set_bytes) by (region)", "6h", "300s", false, QueryType::Cold},
        {"Network Trend 6h", "sum(rate(container_network_transmit_bytes_total[5m])) by (cluster)", "6h", "300s", false, QueryType::Cold},
        {"Pod Restarts 6h", "sum(increase(kube_pod_container_status_restarts_total[1h])) by (namespace)", "6h", "300s", false, QueryType::Cold},
        {"Node Load 6h", "avg(node_load5) by (node)", "6h", "300s", false, QueryType::Cold},
        
        // Cold - 24h range
        {"CPU Trend 24h", "avg(rate(container_cpu_usage_seconds_total[1h])) by (region)", "24h", "1800s", false, QueryType::Cold},
        {"Memory Trend 24h", "avg(container_memory_working_set_bytes) by (region)", "24h", "1800s", false, QueryType::Cold},
        {"Capacity Planning 24h", "avg_over_time(container_memory_working_set_bytes[24h])", "24h", "3600s", false, QueryType::Cold},
        {"Daily Peak CPU", "max_over_time(sum(rate(container_cpu_usage_seconds_total[5m]))[24h:1h])", "24h", "3600s", false, QueryType::Cold},
        {"Daily Errors", "sum(increase(http_requests_total{status=~\"5..\"}[24h])) by (service)", "24h", "3600s", false, QueryType::Cold},
        
        // =====================================================================
        // Complex Aggregation Queries
        // =====================================================================
        {"Top 10 CPU", "topk(10, sum(rate(container_cpu_usage_seconds_total[5m])) by (pod))", "", "", true, QueryType::Hot},
        {"Bottom 10 Memory", "bottomk(10, container_memory_working_set_bytes)", "", "", true, QueryType::Hot},
        {"Namespace Summary", "count(kube_pod_status_ready) by (namespace)", "", "", true, QueryType::Hot},
        {"Service Health", "sum(up) by (job)", "", "", true, QueryType::Hot},
        {"Cluster Overview", "sum(container_cpu_usage_seconds_total) by (cluster, region)", "", "", true, QueryType::Hot},
        
        // =====================================================================
        // NEW: Complex Multi-Label Queries (high cardinality test)
        // =====================================================================
        {"Multi-Label Aggregation", "sum(rate(http_requests_total[5m])) by (namespace, service, method, status)", "", "", true, QueryType::Hot},
        {"High Cardinality Filter", "topk(5, sum(rate(container_cpu_usage_seconds_total[5m])) by (pod, namespace, node))", "", "", true, QueryType::Hot},
        {"Cross-Dimension Analysis", "sum(container_memory_working_set_bytes) by (namespace, region, cluster)", "", "", true, QueryType::Hot},
    };
}

// ============================================================================
// Arrow Helper
// ============================================================================

arrow::Result<std::shared_ptr<arrow::RecordBatch>> CreateMetricBatch(
    const std::string& metric_name,
    int num_samples,
    int64_t base_timestamp,
    const std::string& pod_name,
    const std::string& ns_name) {
    
    // Schema: timestamp (int64), value (float64), tags (Map<String, String>)
    auto key_field = arrow::field("key", arrow::utf8(), false);
    auto item_field = arrow::field("value", arrow::utf8(), true);
    auto tags_type = arrow::map(arrow::utf8(), arrow::utf8());
    
    auto schema = arrow::schema({
        arrow::field("timestamp", arrow::int64(), false),
        arrow::field("value", arrow::float64(), false),
        arrow::field("tags", tags_type, true)
    });
    
    arrow::Int64Builder ts_builder;
    arrow::DoubleBuilder val_builder;
    
    auto pool = arrow::default_memory_pool();
    arrow::MapBuilder tags_builder(pool, 
        std::make_shared<arrow::StringBuilder>(pool), 
        std::make_shared<arrow::StringBuilder>(pool)
    );
    arrow::StringBuilder* key_builder = static_cast<arrow::StringBuilder*>(tags_builder.key_builder());
    arrow::StringBuilder* item_builder = static_cast<arrow::StringBuilder*>(tags_builder.item_builder());
    
    // Optimize RNG: initialize once per thread
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_real_distribution<> val_dist(0.0, 100.0);
    
    for (int i = 0; i < num_samples; ++i) {
        ARROW_RETURN_NOT_OK(ts_builder.Append(base_timestamp + i * 15000)); // 15s interval in milliseconds
        ARROW_RETURN_NOT_OK(val_builder.Append(val_dist(gen)));
        
        ARROW_RETURN_NOT_OK(tags_builder.Append());
        
        // Add tags: metric_name, pod, namespace
        ARROW_RETURN_NOT_OK(key_builder->Append("name"));
        ARROW_RETURN_NOT_OK(item_builder->Append(metric_name));
        
        ARROW_RETURN_NOT_OK(key_builder->Append("pod"));
        ARROW_RETURN_NOT_OK(item_builder->Append(pod_name));
        
        ARROW_RETURN_NOT_OK(key_builder->Append("namespace"));
        ARROW_RETURN_NOT_OK(item_builder->Append(ns_name));
        
        ARROW_RETURN_NOT_OK(key_builder->Append("service"));
        ARROW_RETURN_NOT_OK(item_builder->Append("k8s-benchmark"));
    }
    
    std::shared_ptr<arrow::Array> ts_array;
    std::shared_ptr<arrow::Array> val_array;
    std::shared_ptr<arrow::Array> tags_array;
    
    ARROW_RETURN_NOT_OK(ts_builder.Finish(&ts_array));
    ARROW_RETURN_NOT_OK(val_builder.Finish(&val_array));
    ARROW_RETURN_NOT_OK(tags_builder.Finish(&tags_array));
    
    return arrow::RecordBatch::Make(schema, num_samples, {ts_array, val_array, tags_array});
}

// Helper to create histogram bucket samples with "le" labels
// This enables proper histogram_quantile testing
arrow::Result<std::shared_ptr<arrow::RecordBatch>> CreateHistogramBatch(
    const std::string& metric_name,
    int num_timestamps,
    int64_t base_timestamp,
    const std::string& pod_name,
    const std::string& ns_name,
    const std::string& service_name) {
    
    // Each timestamp gets one sample per bucket (12 buckets)
    int total_samples = num_timestamps * HISTOGRAM_LE_BUCKETS.size();
    
    auto tags_type = arrow::map(arrow::utf8(), arrow::utf8());
    auto schema = arrow::schema({
        arrow::field("timestamp", arrow::int64(), false),
        arrow::field("value", arrow::float64(), false),
        arrow::field("tags", tags_type, true)
    });
    
    arrow::Int64Builder ts_builder;
    arrow::DoubleBuilder val_builder;
    
    auto pool = arrow::default_memory_pool();
    arrow::MapBuilder tags_builder(pool, 
        std::make_shared<arrow::StringBuilder>(pool), 
        std::make_shared<arrow::StringBuilder>(pool)
    );
    arrow::StringBuilder* key_builder = static_cast<arrow::StringBuilder*>(tags_builder.key_builder());
    arrow::StringBuilder* item_builder = static_cast<arrow::StringBuilder*>(tags_builder.item_builder());
    
    // Generate cumulative bucket counts (realistic distribution)
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    
    for (int t = 0; t < num_timestamps; ++t) {
        int64_t timestamp = base_timestamp + t * 15000; // 15s interval in milliseconds
        
        // Generate realistic latency distribution (most in lower buckets)
        double base_count = 1000.0 + t * 10;  // Cumulative count increases
        
        for (size_t b = 0; b < HISTOGRAM_LE_BUCKETS.size(); ++b) {
            ARROW_RETURN_NOT_OK(ts_builder.Append(timestamp));
            
            // Cumulative count increases with bucket (histogram semantics)
            double bucket_pct = (b + 1.0) / HISTOGRAM_LE_BUCKETS.size();
            double cumulative_count = base_count * bucket_pct;
            ARROW_RETURN_NOT_OK(val_builder.Append(cumulative_count));
            
            ARROW_RETURN_NOT_OK(tags_builder.Append());
            
            // Add tags including le label
            ARROW_RETURN_NOT_OK(key_builder->Append("name"));
            ARROW_RETURN_NOT_OK(item_builder->Append(metric_name));
            
            ARROW_RETURN_NOT_OK(key_builder->Append("le"));
            ARROW_RETURN_NOT_OK(item_builder->Append(HISTOGRAM_LE_BUCKETS[b]));
            
            ARROW_RETURN_NOT_OK(key_builder->Append("pod"));
            ARROW_RETURN_NOT_OK(item_builder->Append(pod_name));
            
            ARROW_RETURN_NOT_OK(key_builder->Append("namespace"));
            ARROW_RETURN_NOT_OK(item_builder->Append(ns_name));
            
            ARROW_RETURN_NOT_OK(key_builder->Append("service"));
            ARROW_RETURN_NOT_OK(item_builder->Append(service_name));
        }
    }
    
    std::shared_ptr<arrow::Array> ts_array;
    std::shared_ptr<arrow::Array> val_array;
    std::shared_ptr<arrow::Array> tags_array;
    
    ARROW_RETURN_NOT_OK(ts_builder.Finish(&ts_array));
    ARROW_RETURN_NOT_OK(val_builder.Finish(&val_array));
    ARROW_RETURN_NOT_OK(tags_builder.Finish(&tags_array));
    
    return arrow::RecordBatch::Make(schema, total_samples, {ts_array, val_array, tags_array});
}

// ============================================================================
// JSON Helper
// ============================================================================

size_t count_samples(const std::string& json) {
    size_t count = 0;
    // Simple heuristic: count occurrences of '[ ' followed by digit (start of sample tuple)
    // [ <timestamp>, "<value>" ]
    for (size_t i = 0; i < json.length() - 1; ++i) {
        if (json[i] == '[' && (isdigit(json[i+1]) || json[i+1] == ' ')) {
            // Check if it's really a number start (skip whitespace)
            size_t j = i + 1;
            while (j < json.length() && isspace(json[j])) j++;
            if (j < json.length() && (isdigit(json[j]) || json[j] == '.')) {
                count++;
            }
        }
    }
    return count;
}


// ============================================================================
// Write Worker
// ============================================================================

class WriteWorker {
public:
    WriteWorker(const BenchmarkConfig& config, int worker_id)
        : config_(config), worker_id_(worker_id), running_(false) {
        
        auto location_result = arrow::flight::Location::ForGrpcTcp(config.arrow_host, config.arrow_port);
        if (!location_result.ok()) {
            std::cerr << "Worker " << worker_id << ": Failed to create location: " << location_result.status().ToString() << "\n";
            return;
        }
        
        arrow::flight::FlightClientOptions client_options;
        auto client_result = arrow::flight::FlightClient::Connect(*location_result, client_options);
        if (!client_result.ok()) {
            std::cerr << "Worker " << worker_id << ": Failed to connect: " << client_result.status().ToString() << "\n";
            return;
        }
        client_ = std::move(*client_result);
    }
    
    void start(std::atomic<int64_t>& total_samples, LatencyTracker& latencies) {
        running_ = true;
        thread_ = std::thread([this, &total_samples, &latencies]() {
            run(total_samples, latencies);
        });
    }
    
    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    void run(std::atomic<int64_t>& total_samples, LatencyTracker& latencies) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> metric_dist(0, K8S_METRICS.size() - 1);
        
        // Start timestamps 30 DAYS in the past.
        // This ensures all data is "old" and eligible for demotion to cold storage (ParquetBlock).
        // Queries can use (now - 30 days) to (now) to find all data.
        // IMPORTANT: TSDB expects timestamps in MILLISECONDS, not nanoseconds!
        int64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        // 30 days = 30 * 24 * 60 * 60 * 1000 ms = 2,592,000,000 ms
        int64_t timestamp = current_time_ms - (30LL * 24 * 60 * 60 * 1000);
        
        // Open the stream ONCE
        arrow::flight::FlightCallOptions call_options;
        // Check local vs remote connection to optimize options? No need for now.

        // We use a generic descriptor since we will send mixed metrics
        auto descriptor = arrow::flight::FlightDescriptor::Path({"mixed_metrics"});
        
        // We need the schema from the first batch to open the stream. 
        // But we want to reuse the schema.
        // Let's create a dummy batch to get the schema.
        int64_t dummy_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        auto dummy_res = CreateMetricBatch(K8S_METRICS[0], 1, dummy_ts, "init", "init");
        if (!dummy_res.ok()) {
             std::cerr << "Worker " << worker_id_ << ": Failed to create schema: " << dummy_res.status().ToString() << "\n";
             return;
        }
        auto schema = (*dummy_res)->schema();

        std::unique_ptr<arrow::flight::FlightStreamWriter> writer;
        std::unique_ptr<arrow::flight::FlightMetadataReader> reader;
        
        auto stream_res = client_->DoPut(call_options, descriptor, schema);
        if (!stream_res.ok()) {
            std::cerr << "Worker " << worker_id_ << ": Failed to open DoPut stream: " << stream_res.status().ToString() << "\n";
            return;
        }
        
        writer = std::move(stream_res->writer);
        reader = std::move(stream_res->reader);

        while (running_) {
            // Create a batch for a random metric
            std::string metric_name = K8S_METRICS[metric_dist(gen)];
            std::string pod_name = "pod-" + std::to_string(gen() % 100); 
            std::string ns_name = "ns-" + std::to_string(gen() % 10);
            
            int samples = config_.samples_per_metric; 
            // Increase batch size? Maybe config_.write_batch_size should be used here instead?
            // The user config has write_batch_size but it wasn't used. Let's use it.
            if (config_.write_batch_size > samples) samples = config_.write_batch_size;

            auto start = std::chrono::high_resolution_clock::now();
            
            auto batch_res = CreateMetricBatch(metric_name, samples, timestamp, pod_name, ns_name);
            if (!batch_res.ok()) {
                std::cerr << "Failed to create batch: " << batch_res.status().ToString() << "\n";
                continue;
            }
            auto batch = *batch_res;
            
            // Write to the EXISTING stream
            auto status = writer->WriteRecordBatch(*batch);
            
            auto end = std::chrono::high_resolution_clock::now();
            double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
            
            if (status.ok()) {
                latencies.record(latency_ms);
                total_samples.fetch_add(samples);
                // Advance time by 1 minute per batch.
                // With 30 days = 43,200 minutes available, this allows ~43k batches before
                // reaching current time. Most benchmarks write far fewer batches.
                timestamp += 60000LL;  // 60,000 ms = 1 minute per batch 
                
                // Every 10th batch, also write histogram buckets for histogram_quantile testing
                static thread_local int batch_counter = 0;
                if (++batch_counter % 10 == 0) {
                    auto hist_batch = CreateHistogramBatch(
                        "http_request_duration_seconds_bucket",
                        5,  // 5 timestamps * 12 buckets = 60 samples
                        timestamp,
                        pod_name,
                        ns_name,
                        "test-service-" + std::to_string(gen() % 5)
                    );
                    if (hist_batch.ok()) {
                        writer->WriteRecordBatch(**hist_batch);
                        total_samples.fetch_add(5 * HISTOGRAM_LE_BUCKETS.size());
                    }
                }
            } else {
                // If stream is broken, try to reconnect loop? 
                // For now, just log and break/retry.
                std::cerr << "Write failed: " << status.ToString() << "\n";
                // Simple retry logic: break inner loop to restart worker? 
                // Or try to re-open stream.
                // For benchmark simplicity, we might just log.
                // But if the server closes the stream, we're done unless we re-open.
            }
        }
        
        // Done writing
        if (writer) {
            writer->DoneWriting();
            // Read any final metadata/status
            // writer->Close();
        }
    }
    
    BenchmarkConfig config_;
    int worker_id_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::unique_ptr<arrow::flight::FlightClient> client_;
};

// ============================================================================
// Read Worker
// ============================================================================

class ReadWorker {
public:
    ReadWorker(const BenchmarkConfig& config, int worker_id)
        : config_(config), worker_id_(worker_id), running_(false) {}
    
    void start(std::atomic<int64_t>& total_queries, std::atomic<int64_t>& total_samples, 
               LatencyTracker& latencies, LatencyTracker& hot_latencies, LatencyTracker& cold_latencies) {
        running_ = true;
        thread_ = std::thread([this, &total_queries, &total_samples, &latencies, &hot_latencies, &cold_latencies]() {
            run(total_queries, total_samples, latencies, hot_latencies, cold_latencies);
        });
    }
    
    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    void run(std::atomic<int64_t>& total_queries, std::atomic<int64_t>& total_samples, 
             LatencyTracker& latencies, LatencyTracker& hot_latencies, LatencyTracker& cold_latencies) {
        httplib::Client client(config_.http_address);
        client.set_connection_timeout(30);
        client.set_read_timeout(30);
        
        auto queries = get_dashboard_queries(config_);
        
        // Split queries by type
        std::vector<DashboardQuery> hot_queries;
        std::vector<DashboardQuery> cold_queries;
        for (const auto& q : queries) {
            if (q.type == QueryType::Hot) hot_queries.push_back(q);
            else cold_queries.push_back(q);
        }
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> hot_dist(0, hot_queries.empty() ? 0 : hot_queries.size() - 1);
        std::uniform_int_distribution<> cold_dist(0, cold_queries.empty() ? 0 : cold_queries.size() - 1);
        std::uniform_real_distribution<> ratio_dist(0.0, 1.0);
        
        while (running_) {
            // Select query based on ratio
            // Default hot ratio of 0.8 means 80% chance of picking a hot query
            bool use_hot = ratio_dist(gen) < config_.hot_query_ratio;
            if (hot_queries.empty()) use_hot = false;
            if (cold_queries.empty()) use_hot = true;
            
            const auto& query = use_hot ? hot_queries[hot_dist(gen)] : cold_queries[cold_dist(gen)];
            
            std::string path;
            if (query.is_instant) {
                // For instant queries, use appropriate time based on query type
                // HOT: query recent data (within last 2 hours for hot tier)
                // COLD: query older data (15 days ago where cold data exists)
                auto now = std::chrono::system_clock::now();
                int64_t query_time_offset = use_hot ? (1LL * 60 * 60) : (15LL * 24 * 60 * 60);  // 1h vs 15 days
                auto query_time_sec = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count() - query_time_offset;
                path = "/api/v1/query?query=" + httplib::detail::encode_url(query.query) +
                       "&time=" + std::to_string(query_time_sec);
            } else {
                auto now = std::chrono::system_clock::now();
                auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count();
                
                // HOT queries: Use proper time range based on query's range field
                // e.g., "1h" means query last 1 hour, "6h" means last 6 hours
                // COLD queries: Query from 30 days ago (where cold data exists)
                int64_t start_sec;
                if (use_hot) {
                    // Parse the duration from the query (e.g., "1h", "6h")
                    // Hot queries should only access hot tier data (last 48h)
                    int64_t range_seconds = 3600;  // Default 1h
                    if (!query.duration.empty()) {
                        // Parse duration like "1h", "6h", "24h"
                        int64_t value = std::stoll(query.duration);
                        char unit = query.duration.back();
                        if (unit == 'h') range_seconds = value * 3600;
                        else if (unit == 'm') range_seconds = value * 60;
                        else if (unit == 's') range_seconds = value;
                    }
                    // Cap to hot tier (48h max to stay in hot tier)
                    range_seconds = std::min(range_seconds, 48LL * 3600);
                    start_sec = now_sec - range_seconds;
                } else {
                    // Cold queries: 30 days ago to now
                    start_sec = now_sec - (30LL * 24 * 60 * 60);
                }
                
                path = "/api/v1/query_range?query=" + httplib::detail::encode_url(query.query) +
                       "&start=" + std::to_string(start_sec) +
                       "&end=" + std::to_string(now_sec) +
                       "&step=" + query.step;
            }
            
            auto start = std::chrono::high_resolution_clock::now();
            auto res = client.Get(path.c_str());
            auto end = std::chrono::high_resolution_clock::now();
            
            double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
            
            if (res && res->status == 200) {
                latencies.record(latency_ms);
                if (query.type == QueryType::Hot) {
                    hot_latencies.record(latency_ms);
                } else {
                    cold_latencies.record(latency_ms);
                }
                
                total_queries.fetch_add(1);
                
                // Count samples in response
                size_t samples = count_samples(res->body);
                total_samples.fetch_add(samples);
            }
        }
    }
    
    BenchmarkConfig config_;
    int worker_id_;
    std::atomic<bool> running_;
    std::thread thread_;
};

// ============================================================================
// Combined Benchmark
// ============================================================================

class CombinedBenchmark {
public:
    CombinedBenchmark(const BenchmarkConfig& config) : config_(config) {}
    
    void run() {
        print_config();
        
        if (config_.clean_start) {
            std::cout << "\n=== Phase 0: Cleaning Data ===" << std::endl;
            // Send request to server to clean data? Or just rely on user knowing to restart server?
            // The user said "Delete all the data and include a write only step... lets make sure we let the server start from fresh on every restart"
            // Since this tool runs externally, it can't easily delete server files unless mapped.
            // BUT, the request implies the TOOL should coordinate this.
            // Assuming the server is running, we might need a management endpoint.
            // OR, the user might mean "When running the benchmark, I want to start fresh".
            // Since we don't have a management endpoint for "delete all data", we'll simulate "Generate 10M" by just running a write phase.
            // The "Delete all data" part might be a manual step for the user OR part of the startup script.
            // Wait, "wait until the replay is done?" implies the server restarts.
            // If the user wants to populate 10M samples *fresh*, they probably want:
            // 1. Stop server
            // 2. Delete data dir
            // 3. Start server
            // 4. Run benchmark with "populate 10M" step
            
            // We can't do 1-3 from here easily without more knowledge of deployment.
            // So we will focus on the "Write 10M samples" step.
        }
        
        if (config_.generate_10m) {
             std::cout << "\n=== Phase 0: Generating 10M Samples ===" << std::endl;
             // 10M samples. 
             // Logic: Write samples until we hit 10M.
             run_write_generation(10000000);
        }
        
        // Phase 1: Write-only warm-up (10% of duration)
        std::cout << "\n=== Phase 1: Write Warm-up ===" << std::endl;
        run_writes_only(config_.write_duration_sec / 10);
        
        // Phase 2: Combined write+read benchmark
        std::cout << "\n=== Phase 2: Combined Write+Read ===" << std::endl;
        run_combined();
        
        // Phase 3: Read-only cool-down (10% of duration)
        std::cout << "\n=== Phase 3: Read Cool-down ===" << std::endl;
        run_reads_only(config_.read_duration_sec / 10);
        
        print_results();
        report_server_side_metrics();
    }

private:
   std::string query_prometheus(httplib::Client& client, const std::string& query) {
        std::string path = "/api/v1/query?query=" + httplib::detail::encode_url(query);
        auto res = client.Get(path.c_str());
        if (res && res->status == 200) {
            // Primitive JSON parsing to extract "value":["timestamp","VALUE"]
            // value array is typicalled [123456.789, "123.45"]
            size_t value_pos = res->body.find("\"value\":[");
            if (value_pos != std::string::npos) {
                size_t start_quote = res->body.find("\"", value_pos + 8); // Skip [timestamp, 
                if (start_quote != std::string::npos) {
                    // Find *second* quote (the value is the second element string)
                     // Format: [ 173... , "0.023" ]
                     // Wait, first element is number. Second is string.
                     // The first quote we find after "value":[" should be the start of the second element?
                     // No, "value":[1733516627.874,"0"]
                     // The first " after [ is the start of the value string.
                     size_t end_quote = res->body.find("\"", start_quote + 1);
                     if (end_quote != std::string::npos) {
                         return res->body.substr(start_quote + 1, end_quote - start_quote - 1);
                     }
                }
            }
        }
        return "NaN";
    }

    void report_server_side_metrics() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "         SERVER-SIDE METRICS (Auto-Queried)" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        httplib::Client client(config_.http_address);
        client.set_connection_timeout(5);
        client.set_read_timeout(5);

        // Helper to query and parse metric value
        auto get_metric = [&](const std::string& metric_name) -> double {
            std::string path = "/api/v1/query?query=" + httplib::detail::encode_url(metric_name);
            auto res = client.Get(path.c_str());
            if (res && res->status == 200) {
                size_t value_pos = res->body.find("\"value\":[");
                if (value_pos != std::string::npos) {
                    size_t start_quote = res->body.find("\"", value_pos + 8);
                    if (start_quote != std::string::npos) {
                        size_t end_quote = res->body.find("\"", start_quote + 1);
                        if (end_quote != std::string::npos) {
                            try {
                                return std::stod(res->body.substr(start_quote + 1, end_quote - start_quote - 1));
                            } catch (...) {}
                        }
                    }
                }
            }
            return 0.0;
        };

        // Helper to format seconds or count
        auto fmt_time = [](double seconds) -> std::string {
            std::ostringstream ss;
            if (seconds < 1.0) {
                ss << std::fixed << std::setprecision(2) << (seconds * 1000) << " ms";
            } else {
                ss << std::fixed << std::setprecision(3) << seconds << " s";
            }
            return ss.str();
        };
        
        auto fmt_count = [](double count) -> std::string {
            std::ostringstream ss;
            if (count >= 1e6) ss << std::fixed << std::setprecision(2) << (count / 1e6) << "M";
            else if (count >= 1e3) ss << std::fixed << std::setprecision(1) << (count / 1e3) << "K";
            else ss << std::fixed << std::setprecision(0) << count;
            return ss.str();
        };

        // ========== QUERY METRICS ==========
        std::cout << "\n--- Query Metrics ---" << std::endl;
        double query_count = get_metric("mytsdb_query_count_total");
        double query_errors = get_metric("mytsdb_query_errors_total");
        double query_duration = get_metric("mytsdb_query_duration_seconds_total");
        double query_parse = get_metric("mytsdb_query_parse_duration_seconds_total");
        double query_eval = get_metric("mytsdb_query_eval_duration_seconds_total");
        double query_exec = get_metric("mytsdb_query_exec_duration_seconds_total");
        double query_storage = get_metric("mytsdb_query_storage_read_duration_seconds_total");
        double samples_scanned = get_metric("mytsdb_query_samples_scanned_total");
        double series_scanned = get_metric("mytsdb_query_series_scanned_total");
        double bytes_scanned = get_metric("mytsdb_query_bytes_scanned_total");
        
        std::cout << "  Total Queries:     " << fmt_count(query_count) << std::endl;
        std::cout << "  Query Errors:      " << fmt_count(query_errors) << std::endl;
        std::cout << "  Avg Query Time:    " << (query_count > 0 ? fmt_time(query_duration / query_count) : "N/A") << std::endl;
        std::cout << "  Total Parse Time:  " << fmt_time(query_parse) << std::endl;
        std::cout << "  Total Eval Time:   " << fmt_time(query_eval) << std::endl;
        std::cout << "  Total Exec Time:   " << fmt_time(query_exec) << std::endl;
        std::cout << "  Storage Read Time: " << fmt_time(query_storage) << std::endl;
        std::cout << "  Samples Scanned:   " << fmt_count(samples_scanned) << std::endl;
        std::cout << "  Series Scanned:    " << fmt_count(series_scanned) << std::endl;

        // ========== WRITE METRICS ==========
        std::cout << "\n--- Write Metrics ---" << std::endl;
        double mutex_lock = get_metric("mytsdb_write_mutex_lock_seconds_total");
        double sample_append = get_metric("mytsdb_write_sample_append_seconds_total");
        double wal_write = get_metric("mytsdb_write_wal_write_seconds_total");
        double series_id = get_metric("mytsdb_write_series_id_calc_seconds_total");
        double index_insert = get_metric("mytsdb_write_index_insert_seconds_total");
        double block_seal = get_metric("mytsdb_write_block_seal_seconds_total");
        double block_persist = get_metric("mytsdb_write_block_persist_seconds_total");
        double cache_update = get_metric("mytsdb_write_cache_update_seconds_total");
        
        double total_write_time = mutex_lock + sample_append + wal_write + series_id + 
                                  index_insert + block_seal + block_persist + cache_update;
        
        std::cout << "  Mutex Wait:        " << fmt_time(mutex_lock) 
                  << " (" << std::fixed << std::setprecision(1) 
                  << (total_write_time > 0 ? (mutex_lock / total_write_time * 100) : 0) << "%)" << std::endl;
        std::cout << "  Sample Append:     " << fmt_time(sample_append) << std::endl;
        std::cout << "  WAL Write:         " << fmt_time(wal_write) << std::endl;
        std::cout << "  Series ID Calc:    " << fmt_time(series_id) << std::endl;
        std::cout << "  Index Insert:      " << fmt_time(index_insert) << std::endl;
        std::cout << "  Block Seal:        " << fmt_time(block_seal) << std::endl;
        std::cout << "  Block Persist:     " << fmt_time(block_persist) << std::endl;
        std::cout << "  Cache Update:      " << fmt_time(cache_update) << std::endl;

        // ========== READ METRICS ==========
        std::cout << "\n--- Read Metrics ---" << std::endl;
        double read_total = get_metric("mytsdb_read_total");
        double read_duration = get_metric("mytsdb_read_duration_seconds_total");
        double read_index = get_metric("mytsdb_read_index_search_seconds_total");
        double read_block_lookup = get_metric("mytsdb_read_block_lookup_seconds_total");
        double read_block_read = get_metric("mytsdb_read_block_read_seconds_total");
        double read_decomp = get_metric("mytsdb_read_decompression_seconds_total");
        double read_cache_hits = get_metric("mytsdb_read_cache_hits_total");
        
        // Detailed Breakdown
        double read_active_lookup = get_metric("mytsdb_read_active_series_lookup_seconds_total");
        double read_active_read = get_metric("mytsdb_read_active_series_read_seconds_total");
        double read_rg_read = get_metric("mytsdb_read_row_group_read_seconds_total");
        double read_decoding = get_metric("mytsdb_read_decoding_seconds_total");
        double read_processing = get_metric("mytsdb_read_processing_seconds_total");
        
        double read_block_filter = get_metric("mytsdb_read_block_filter_seconds_total");
        double read_data_extraction = get_metric("mytsdb_read_data_extraction_seconds_total");
        double read_result_construction = get_metric("mytsdb_read_result_construction_seconds_total");
        double read_data_copy = get_metric("mytsdb_read_data_copy_seconds_total");
        
        std::cout << "  Total Reads:       " << fmt_count(read_total) << std::endl;
        std::cout << "  Avg Read Time:     " << (read_total > 0 ? fmt_time(read_duration / read_total) : "N/A") << std::endl;
        std::cout << "  Index Search:      " << fmt_time(read_index) << std::endl;
        std::cout << "  Block Lookup:      " << fmt_time(read_block_lookup) << std::endl;
        std::cout << "  Block Read I/O:    " << fmt_time(read_block_read) << std::endl;
        std::cout << "  Decompression:     " << fmt_time(read_decomp) << std::endl;
        std::cout << "  Cache Hits:        " << fmt_count(read_cache_hits) << std::endl;
        
        std::cout << "\n  --- Detailed Breakdown ---" << std::endl;
        std::cout << "  Active Series Lookup: " << fmt_time(read_active_lookup) << std::endl;
        std::cout << "  Active Series Read:   " << fmt_time(read_active_read) << std::endl;
        std::cout << "  Row Group Read:    " << fmt_time(read_rg_read) << std::endl;
        std::cout << "  Decoding (ToMap):  " << fmt_time(read_decoding) << std::endl;
        std::cout << "  Processing:        " << fmt_time(read_processing) << std::endl;
        std::cout << "  Block Filter:      " << fmt_time(read_block_filter) << std::endl;
        std::cout << "  Data Extraction:   " << fmt_time(read_data_extraction) << std::endl;
        std::cout << "  Data Copy:         " << fmt_time(read_data_copy) << std::endl;
        std::cout << "  Result Construct:  " << fmt_time(read_result_construction) << std::endl;

        // ========== SECONDARY INDEX METRICS (Phase A: B+ Tree) ==========
        std::cout << "\n--- Secondary Index Metrics ---" << std::endl;
        double idx_lookups = get_metric("mytsdb_secondary_index_lookups_total");
        double idx_hits = get_metric("mytsdb_secondary_index_hits_total");
        double idx_misses = get_metric("mytsdb_secondary_index_misses_total");
        double idx_lookup_time = get_metric("mytsdb_secondary_index_lookup_seconds_total");
        double idx_build_time = get_metric("mytsdb_secondary_index_build_seconds_total");
        double idx_rg_selected = get_metric("mytsdb_secondary_index_row_groups_selected_total");
        
        std::cout << "  Index Lookups:     " << fmt_count(idx_lookups) << std::endl;
        std::cout << "  Index Hits:        " << fmt_count(idx_hits) << std::endl;
        std::cout << "  Index Misses:      " << fmt_count(idx_misses) << std::endl;
        std::cout << "  Index Hit Rate:    " << std::fixed << std::setprecision(1) 
                  << (idx_lookups > 0 ? (idx_hits / idx_lookups * 100) : 0) << "%" << std::endl;
        std::cout << "  Lookup Time:       " << fmt_time(idx_lookup_time) << std::endl;
        std::cout << "  Avg Lookup:        " << (idx_lookups > 0 ? fmt_time(idx_lookup_time / idx_lookups) : "N/A") << std::endl;
        std::cout << "  Build Time:        " << fmt_time(idx_build_time) << std::endl;
        std::cout << "  RG Selected:       " << fmt_count(idx_rg_selected) << std::endl;

        // ========== BLOOM FILTER METRICS (Phase 0: Pre-B+ Tree) ==========
        std::cout << "\n--- Bloom Filter Metrics ---" << std::endl;
        double bloom_checks = get_metric("mytsdb_bloom_filter_checks_total");
        double bloom_skips = get_metric("mytsdb_bloom_filter_skips_total");
        double bloom_passes = get_metric("mytsdb_bloom_filter_passes_total");
        double bloom_lookup_time = get_metric("mytsdb_bloom_filter_lookup_seconds_total");

        std::cout << "  Bloom Checks:      " << fmt_count(bloom_checks) << std::endl;
        std::cout << "  Bloom Skips:       " << fmt_count(bloom_skips) << std::endl;
        std::cout << "  Bloom Passes:      " << fmt_count(bloom_passes) << std::endl;
        std::cout << "  Bloom Skip Rate:   " << std::fixed << std::setprecision(1) 
                  << (bloom_checks > 0 ? (bloom_skips / bloom_checks * 100) : 0) << "%" << std::endl;
        std::cout << "  Lookup Time:       " << fmt_time(bloom_lookup_time) << std::endl;
        std::cout << "  Avg Lookup:        " << (bloom_checks > 0 ? fmt_time(bloom_lookup_time / bloom_checks) : "N/A") << std::endl;

        // ========== STORAGE METRICS ==========
        std::cout << "\n--- Storage Metrics ---" << std::endl;
        double storage_writes = get_metric("mytsdb_storage_writes_total");
        double storage_reads = get_metric("mytsdb_storage_reads_total");
        double storage_cache_hits = get_metric("mytsdb_storage_cache_hits_total");
        double storage_cache_misses = get_metric("mytsdb_storage_cache_misses_total");
        double bytes_written = get_metric("mytsdb_storage_bytes_written_total");
        double bytes_read = get_metric("mytsdb_storage_bytes_read_total");
        double memory_usage = get_metric("mytsdb_storage_net_memory_usage_bytes");
        
        std::cout << "  Writes:            " << fmt_count(storage_writes) << std::endl;
        std::cout << "  Reads:             " << fmt_count(storage_reads) << std::endl;
        std::cout << "  Cache Hit Rate:    " << std::fixed << std::setprecision(1) 
                  << (storage_cache_hits + storage_cache_misses > 0 ? 
                      (storage_cache_hits / (storage_cache_hits + storage_cache_misses) * 100) : 0) 
                  << "%" << std::endl;
        std::cout << "  Bytes Written:     " << fmt_count(bytes_written) << " B" << std::endl;
        std::cout << "  Bytes Read:        " << fmt_count(bytes_read) << " B" << std::endl;
        std::cout << "  Memory Usage:      " << std::fixed << std::setprecision(1) 
                  << (memory_usage / 1024 / 1024) << " MB" << std::endl;
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
    }

    void print_config() {
        std::cout << "\n=== K8s Combined Benchmark ===" << std::endl;
        std::cout << "Preset: " << config_.preset << std::endl;
        std::cout << "Total Pods: " << config_.total_pods() << std::endl;
        std::cout << "Total Time Series: " << config_.total_time_series() << std::endl;
        std::cout << "Write Workers: " << config_.write_workers << std::endl;
        std::cout << "Read Workers: " << config_.read_workers << std::endl;
        std::cout << "Write Duration: " << config_.write_duration_sec << "s" << std::endl;
        std::cout << "Read Duration: " << config_.read_duration_sec << "s" << std::endl;
        std::cout << "Clean Start: " << (config_.clean_start ? "yes" : "no") << std::endl;
        std::cout << "Generate 10M: " << (config_.generate_10m ? "yes" : "no") << std::endl;
    }
    
    void run_write_generation(int64_t target_samples) {
        std::cout << "Generating " << target_samples << " samples..." << std::endl;
        std::atomic<int64_t> total_samples{0};
        std::vector<std::unique_ptr<WriteWorker>> workers;
        LatencyTracker latencies; // Local tracker
        
        for (int i = 0; i < config_.write_workers; ++i) {
            workers.push_back(std::make_unique<WriteWorker>(config_, i));
            workers.back()->start(total_samples, latencies);
        }
        
        // Wait until target is reached
        auto start_time = std::chrono::steady_clock::now();
        while (total_samples.load() < target_samples) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            int64_t current = total_samples.load();
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start_time).count();
            double rate = current / elapsed;
             std::cout << "Generated " << current << " / " << target_samples << " (" << std::fixed << std::setprecision(0) << rate << " samples/sec)\r" << std::flush;
        }
        std::cout << std::endl;
        
        for (auto& w : workers) w->stop();
        std::cout << "Generation complete." << std::endl;
    }
    
    void run_writes_only(int duration_sec) {
        if (duration_sec <= 0) return;
        
        std::atomic<int64_t> total_samples{0};
        std::vector<std::unique_ptr<WriteWorker>> workers;
        
        for (int i = 0; i < config_.write_workers; ++i) {
            workers.push_back(std::make_unique<WriteWorker>(config_, i));
            workers.back()->start(total_samples, write_latencies_);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
        
        for (auto& w : workers) w->stop();
        
        std::cout << "Warm-up writes: " << total_samples.load() << " samples" << std::endl;
    }
    
    void run_reads_only(int duration_sec) {
        if (duration_sec <= 0) return;
        
        std::atomic<int64_t> total_queries{0};
        std::vector<std::unique_ptr<ReadWorker>> workers;
        
        for (int i = 0; i < config_.read_workers; ++i) {
            workers.push_back(std::make_unique<ReadWorker>(config_, i));
            workers.back()->start(total_queries, total_read_samples_, read_latencies_, hot_read_latencies_, cold_read_latencies_);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
        
        for (auto& w : workers) w->stop();
        
        std::cout << "Cool-down queries: " << total_queries.load() << std::endl;
    }
    
    void run_combined() {
        std::atomic<int64_t> total_write_samples{0};
        std::atomic<int64_t> total_read_queries{0};
        std::atomic<int64_t> total_read_samples{0};
        
        std::vector<std::unique_ptr<WriteWorker>> write_workers;
        std::vector<std::unique_ptr<ReadWorker>> read_workers;
        
        // Start write workers
        for (int i = 0; i < config_.write_workers; ++i) {
            write_workers.push_back(std::make_unique<WriteWorker>(config_, i));
            write_workers.back()->start(total_write_samples, write_latencies_);
        }
        
        // Start read workers
        for (int i = 0; i < config_.read_workers; ++i) {
            read_workers.push_back(std::make_unique<ReadWorker>(config_, i));
            read_workers.back()->start(total_read_queries, total_read_samples, read_latencies_, hot_read_latencies_, cold_read_latencies_);
        }
        
        auto start_time = std::chrono::steady_clock::now();
        int duration = std::max(config_.write_duration_sec, config_.read_duration_sec);
        
        // Progress reporting
        for (int elapsed = 0; elapsed < duration; ++elapsed) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            if ((elapsed + 1) % 10 == 0 || elapsed == duration - 1) {
                double write_rate = (double)total_write_samples.load() / (elapsed + 1);
                double read_rate = (double)total_read_queries.load() / (elapsed + 1);
                
                  std::cout << "[" << (elapsed + 1) << "s] "
                          << "Writes: " << write_rate << " samples/sec, "
                          << "Reads: " << read_rate << " queries/sec (" 
                          << (double)total_read_samples.load() / (elapsed + 1) << " samples/sec)" << std::endl;
            }
        }
        
        // Stop all workers
        for (auto& w : write_workers) w->stop();
        for (auto& w : read_workers) w->stop();
        
        combined_write_samples_ = total_write_samples.load();
        combined_read_queries_ = total_read_queries.load();
        combined_read_samples_ = total_read_samples.load();
    }
    
    void print_results() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "         BENCHMARK RESULTS" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Write performance
        std::cout << "\n--- Write Performance ---" << std::endl;
        std::cout << "Total Samples: " << combined_write_samples_ << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(0) 
                  << (double)combined_write_samples_ / config_.write_duration_sec << " samples/sec" << std::endl;
        std::cout << "Latency p50: " << std::fixed << std::setprecision(2) 
                  << write_latencies_.percentile(50) << " ms" << std::endl;
        std::cout << "Latency p99: " << write_latencies_.percentile(99) << " ms" << std::endl;
        std::cout << "Latency max: " << write_latencies_.max() << " ms" << std::endl;
        
        // Read performance
        std::cout << "\n--- Read Performance ---" << std::endl;
        std::cout << "Total Queries: " << combined_read_queries_ << std::endl;
        std::cout << "Total Samples: " << combined_read_samples_ << std::endl;
        std::cout << "Throughput (Queries): " << std::fixed << std::setprecision(1)
                  << (double)combined_read_queries_ / config_.read_duration_sec << " queries/sec" << std::endl;
        std::cout << "Throughput (Samples): " << std::fixed << std::setprecision(0)
                  << (double)combined_read_samples_ / config_.read_duration_sec << " samples/sec" << std::endl;
        std::cout << "Latency p50: " << std::fixed << std::setprecision(2)
                  << read_latencies_.percentile(50) << " ms" << std::endl;
        std::cout << "Latency p99: " << read_latencies_.percentile(99) << " ms" << std::endl;
        std::cout << "Latency max: " << read_latencies_.max() << " ms" << std::endl;

        std::cout << "\n--- Hot Queries (Target 80%) ---" << std::endl;
        std::cout << "Count: " << hot_read_latencies_.count() << std::endl;
        std::cout << "Latency p50: " << std::fixed << std::setprecision(2) << hot_read_latencies_.percentile(50) << " ms" << std::endl;
        std::cout << "Latency p99: " << hot_read_latencies_.percentile(99) << " ms" << std::endl;

        std::cout << "\n--- Cold Queries (Target 20%) ---" << std::endl;
        std::cout << "Count: " << cold_read_latencies_.count() << std::endl;
        std::cout << "Latency p50: " << std::fixed << std::setprecision(2) << cold_read_latencies_.percentile(50) << " ms" << std::endl;
        std::cout << "Latency p99: " << cold_read_latencies_.percentile(99) << " ms" << std::endl;
        
        // SLA compliance
        std::cout << "\n--- SLA Compliance ---" << std::endl;
        bool write_sla = write_latencies_.percentile(99) < 50;   // p99 < 50ms
        bool read_p50_sla = read_latencies_.percentile(50) < 50;  // p50 < 50ms
        bool read_p99_sla = read_latencies_.percentile(99) < 50;  // p99 < 50ms
        
        std::cout << "Write p99 < 50ms:  " << (write_sla ? "PASS" : "FAIL") << std::endl;
        std::cout << "Read p50 < 50ms:   " << (read_p50_sla ? "PASS" : "FAIL") << std::endl;
        std::cout << "Read p99 < 50ms:   " << (read_p99_sla ? "PASS" : "FAIL") << std::endl;
        
        std::cout << "========================================\n" << std::endl;
    }
    
    BenchmarkConfig config_;
    LatencyTracker write_latencies_;
    LatencyTracker read_latencies_;
    LatencyTracker hot_read_latencies_;
    LatencyTracker cold_read_latencies_;
    std::atomic<int64_t> total_read_samples_{0}; // For read-only phase
    int64_t combined_write_samples_ = 0;
    int64_t combined_read_queries_ = 0;
    int64_t combined_read_samples_ = 0;
};

// ============================================================================
// Main
// ============================================================================

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]" << std::endl;
    std::cout << "\nPresets:" << std::endl;
    std::cout << "  --quick    Quick test (12 pods, 10s duration)" << std::endl;
    std::cout << "  --small    Small test (~150 pods, 30s duration)" << std::endl;
    std::cout << "  --medium   Medium test (~1800 pods, 60s duration)" << std::endl;
    std::cout << "  --large    Large test (9000 pods, 5min duration)" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --arrow-host      Arrow Flight host (default: localhost)" << std::endl;
    std::cout << "  --arrow-port      Arrow Flight port (default: 8815)" << std::endl;
    std::cout << "  --http-address    PromQL HTTP endpoint (default: localhost:9090)" << std::endl;
    std::cout << "  --write-workers   Number of write workers (default: 4)" << std::endl;
    std::cout << "  --read-workers    Number of read workers (default: 4)" << std::endl;
    std::cout << "  --duration        Test duration in seconds" << std::endl;
    std::cout << "  --clean-start     (Flag) Indicate a fresh start (informational only)" << std::endl;
    std::cout << "  --generate-10m    (Flag) Generate 10M samples before benchmark" << std::endl;
    std::cout << "  --help            Show this help message" << std::endl;
}

int main(int argc, char** argv) {
    BenchmarkConfig config;
    
    // Pass 1: Check for preset
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--quick") config.preset = "quick";
        else if (arg == "--small") config.preset = "small";
        else if (arg == "--medium") config.preset = "medium";
        else if (arg == "--large") config.preset = "large";
        else if (arg == "--preset" && i + 1 < argc) config.preset = argv[i+1];
    }
    
    // Apply preset defaults first
    config.apply_preset();
    
    // Pass 2: Apply overrides
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--arrow-host") {
            if (i + 1 < argc) config.arrow_host = argv[++i];
        } else if (arg == "--arrow-port") {
            if (i + 1 < argc) config.arrow_port = std::stoi(argv[++i]);
        } else if (arg == "--http-address") {
            if (i + 1 < argc) config.http_address = argv[++i];
        } else if (arg == "--write-workers") {
            if (i + 1 < argc) config.write_workers = std::stoi(argv[++i]);
        } else if (arg == "--read-workers") {
            if (i + 1 < argc) config.read_workers = std::stoi(argv[++i]);
        } else if (arg == "--duration") {
            if (i + 1 < argc) {
                config.write_duration_sec = std::stoi(argv[++i]);
                config.read_duration_sec = config.write_duration_sec;
            }
        } else if (arg == "--clean-start") {
            config.clean_start = true;
        } else if (arg == "--generate-10m") {
            config.generate_10m = true;
        }
        // Skip preset args in this pass (handled implicitly or benign re-assignment)
        else if (arg == "--preset") { i++; }
    }
    
    CombinedBenchmark benchmark(config);
    benchmark.run();
    
    return 0;
}
