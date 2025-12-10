/**
 * K8s gRPC/OTEL Benchmark Tool
 * 
 * Simulates a realistic K8s monitoring workload ingesting via gRPC/OTEL:
 * - Concurrent writes (ingesting K8s metrics via OTLP)
 * - Concurrent reads (Grafana dashboard queries)
 * - Performance metrics (p50/p99 latencies, throughput)
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

#include <grpcpp/grpcpp.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"

// ============================================================================
// Configuration
// ============================================================================

struct BenchmarkConfig {
    // Connection settings
    std::string grpc_host = "localhost";
    int grpc_port = 8815; // This will likely be different for gRPC/OTEL, usually 4317. User should specify.
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
    double hot_query_ratio = 0.8;
    
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
    
    // HTTP/gRPC service metrics (25)
    "http_requests_total", "http_request_duration_seconds",
    "http_request_duration_seconds_bucket",
    "http_request_size_bytes", "http_response_size_bytes",
    "http_requests_in_flight", "grpc_server_started_total",
    "grpc_server_handled_total", "grpc_server_msg_received_total",
    "grpc_server_msg_sent_total", "grpc_server_handling_seconds",
    "grpc_server_handling_seconds_bucket",
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
    std::string duration;  // Range
    std::string step;
    bool is_instant;
    QueryType type;
};

std::vector<DashboardQuery> get_dashboard_queries(const BenchmarkConfig& config) {
    // Reusing same queries as original benchmark
    return {
        {"CPU Usage", "sum(rate(container_cpu_usage_seconds_total[5m])) by (namespace)", "", "", true, QueryType::Hot},
        {"Memory Usage", "sum(container_memory_working_set_bytes) by (namespace)", "", "", true, QueryType::Hot},
        {"Pod Count", "count(kube_pod_status_phase) by (namespace, phase)", "", "", true, QueryType::Hot},
        {"Network I/O", "sum(rate(container_network_receive_bytes_total[5m]))", "", "", true, QueryType::Hot},
        {"Disk I/O", "sum(rate(container_fs_reads_bytes_total[5m]))", "", "", true, QueryType::Hot},
        {"CPU Trend 1h", "sum(rate(container_cpu_usage_seconds_total[5m])) by (namespace)", "1h", "60s", false, QueryType::Hot},
        {"Memory Trend 1h", "sum(container_memory_working_set_bytes) by (namespace)", "1h", "60s", false, QueryType::Hot},
        {"Request Rate 1h", "sum(rate(http_requests_total[5m])) by (service)", "1h", "60s", false, QueryType::Hot},
        {"Error Rate 1h", "sum(rate(http_requests_total{status=~\"5..\"}[5m]))", "1h", "60s", false, QueryType::Hot},
        {"Latency P99 1h", "histogram_quantile(0.99, rate(http_request_duration_seconds_bucket[5m]))", "1h", "60s", false, QueryType::Hot},
        
        // Cold Queries
        {"CPU Trend 6h", "avg(rate(container_cpu_usage_seconds_total[5m])) by (region)", "6h", "300s", false, QueryType::Cold},
        {"Memory Trend 6h", "avg(container_memory_working_set_bytes) by (region)", "6h", "300s", false, QueryType::Cold},
    };
}

// ============================================================================
// gRPC/OTEL Helper
// ============================================================================

void ProcessOneMetric(
    opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest& request,
    const std::string& metric_name,
    int num_samples,
    int64_t base_timestamp,
    const std::string& pod_name,
    const std::string& ns_name) {

    auto* rm = request.add_resource_metrics();
    auto* resource = rm->mutable_resource();
    
    // Resource attributes
    auto* attr = resource->add_attributes();
    attr->set_key("service.name");
    attr->mutable_value()->set_string_value("k8s-benchmark");

    auto* sm = rm->add_scope_metrics();
    sm->mutable_scope()->set_name("mytsdb-benchmark");
    
    auto* metric = sm->add_metrics();
    metric->set_name(metric_name);
    
    // Simple gauge for normal metrics
    auto* gauge = metric->mutable_gauge();
    
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_real_distribution<> val_dist(0.0, 100.0);
    
    // Attributes for data points
    auto add_attributes = [&](opentelemetry::proto::metrics::v1::NumberDataPoint* point) {
        auto* a1 = point->add_attributes();
        a1->set_key("pod");
        a1->mutable_value()->set_string_value(pod_name);
        
        auto* a2 = point->add_attributes();
        a2->set_key("namespace");
        a2->mutable_value()->set_string_value(ns_name);
    };

    for (int i = 0; i < num_samples; ++i) {
        auto* point = gauge->add_data_points();
        // OTLP uses nanoseconds
        point->set_time_unix_nano((base_timestamp + i * 15000) * 1000000LL);
        point->set_as_double(val_dist(gen));
        add_attributes(point);
    }
}

// ============================================================================
// Write Worker
// ============================================================================

class WriteWorker {
public:
    WriteWorker(const BenchmarkConfig& config, int worker_id)
        : config_(config), worker_id_(worker_id), running_(false) {
        
        std::string target = config.grpc_host + ":" + std::to_string(config.grpc_port);
        channel_ = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
        stub_ = opentelemetry::proto::collector::metrics::v1::MetricsService::NewStub(channel_);
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
        
        // Start 30 days ago (same as original benchmark)
        int64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        int64_t timestamp = current_time_ms - (30LL * 24 * 60 * 60 * 1000);
        
        while (running_) {
            std::string metric_name = K8S_METRICS[metric_dist(gen)];
            std::string pod_name = "pod-" + std::to_string(gen() % 100); 
            std::string ns_name = "ns-" + std::to_string(gen() % 10);
            
            int samples = config_.samples_per_metric; 
            if (config_.write_batch_size > samples) samples = config_.write_batch_size;
            
            // Build gRPC Request
            opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest request;
            ProcessOneMetric(request, metric_name, samples, timestamp, pod_name, ns_name);
            opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse response;
            grpc::ClientContext context;
            context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

            auto start = std::chrono::high_resolution_clock::now();
            
            grpc::Status status = stub_->Export(&context, request, &response);
            
            auto end = std::chrono::high_resolution_clock::now();
            double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
            
            if (status.ok()) {
                latencies.record(latency_ms);
                total_samples.fetch_add(samples);
                timestamp += 60000LL;  // Advance 1 minute
            } else {
                 if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
                     std::cerr << "Write timeout (deadline exceeded)\n";
                 } else if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                     std::cerr << "Write unavailable: " << status.error_message() << "\n";
                 } else {
                     // Log other failures
                     static thread_local int fail_count = 0;
                     if (++fail_count % 10 == 0) {
                          std::cerr << "Write failed: " << status.error_message() << " (" << status.error_code() << ")\n";
                     }
                 }
            }
        }
    }
    
    BenchmarkConfig config_;
    int worker_id_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<opentelemetry::proto::collector::metrics::v1::MetricsService::Stub> stub_;
};

// ============================================================================
// Read Worker (Same as combined)
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
      // Same logic as k8s_combined_benchmark logic for reads.
      // Re-implemented quickly or copied?
      // Since it's HTTP, no changes needed.
      // I'll copy the body for completeness so standalone compile works.
      
    void run(std::atomic<int64_t>& total_queries, std::atomic<int64_t>& total_samples, 
             LatencyTracker& latencies, LatencyTracker& hot_latencies, LatencyTracker& cold_latencies) {
        // Reuse original logic (simplified for brevity in this copy)
        httplib::Client client(config_.http_address);
        client.set_connection_timeout(30);
        
        auto queries = get_dashboard_queries(config_);
        std::vector<DashboardQuery> hot_queries, cold_queries;
        for (const auto& q : queries) {
            if (q.type == QueryType::Hot) hot_queries.push_back(q);
            else cold_queries.push_back(q);
        }
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> ratio_dist(0.0, 1.0);
         std::uniform_int_distribution<> hot_dist(0, hot_queries.empty() ? 0 : hot_queries.size() - 1);
        std::uniform_int_distribution<> cold_dist(0, cold_queries.empty() ? 0 : cold_queries.size() - 1);
        
        while (running_) {
            bool use_hot = ratio_dist(gen) < config_.hot_query_ratio;
             if (hot_queries.empty()) use_hot = false;
            if (cold_queries.empty()) use_hot = true;

            const auto& query = use_hot ? hot_queries[hot_dist(gen)] : cold_queries[cold_dist(gen)];
            
            // Simplified query path construction
             std::string path;
             int64_t now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
            
            if (query.is_instant) {
                path = "/api/v1/query?query=" + httplib::detail::encode_url(query.query);
            } else {
                 path = "/api/v1/query_range?query=" + httplib::detail::encode_url(query.query) +
                       "&start=" + std::to_string(now_sec - 3600) +
                       "&end=" + std::to_string(now_sec) +
                       "&step=60s";
            }

            auto start = std::chrono::high_resolution_clock::now();
            auto res = client.Get(path.c_str());
            auto end = std::chrono::high_resolution_clock::now();
            double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();

             if (res && res->status == 200) {
                latencies.record(latency_ms);
                total_queries.fetch_add(1);
             }
        }
    }

    BenchmarkConfig config_;
    int worker_id_;
    std::atomic<bool> running_;
    std::thread thread_;
};

// ============================================================================
// Main & Benchmark Class (Simplified)
// ============================================================================

class GrpcBenchmark {
public:
    GrpcBenchmark(const BenchmarkConfig& config) : config_(config) {}
    
    void run() {
        std::cout << "Starting gRPC/OTEL Benchmark..." << std::endl;
        std::cout << "Endpoint: " << config_.grpc_host << ":" << config_.grpc_port << std::endl;
        
        if (config_.generate_10m) {
             std::cout << "Skipping 10M generation for this test variant." << std::endl;
        }

        std::cout << "\n=== Phase 1: Write Warm-up ===" << std::endl;
        run_writes_only(config_.write_duration_sec / 10);
        
        std::cout << "\n=== Phase 2: Combined Write+Read ===" << std::endl;
        run_combined();
        
        print_results();
    }
    
private:
    void run_writes_only(int duration_sec) {
        if (duration_sec <= 0) return;
        std::atomic<int64_t> total_samples{0};
        std::vector<std::unique_ptr<WriteWorker>> workers;
        for(int i=0; i<config_.write_workers; ++i) {
            workers.push_back(std::make_unique<WriteWorker>(config_, i));
            workers.back()->start(total_samples, write_latencies_);
        }
        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
        for(auto& w : workers) w->stop();
    }
    
    void run_combined() {
        std::atomic<int64_t> total_write_samples{0};
        std::atomic<int64_t> total_read_queries{0};
        // Needs total_read_samples tracking to match signature
        std::atomic<int64_t> total_read_samples{0}; 
        
        std::vector<std::unique_ptr<WriteWorker>> write_workers;
        std::vector<std::unique_ptr<ReadWorker>> read_workers;
        
         for(int i=0; i<config_.write_workers; ++i) {
            write_workers.push_back(std::make_unique<WriteWorker>(config_, i));
            write_workers.back()->start(total_write_samples, write_latencies_);
        }
        for(int i=0; i<config_.read_workers; ++i) {
            read_workers.push_back(std::make_unique<ReadWorker>(config_, i));
            read_workers.back()->start(total_read_queries, total_read_samples, read_latencies_, hot_read_latencies_, cold_read_latencies_);
        }
        
        int duration = std::max(config_.write_duration_sec, config_.read_duration_sec);
        for(int i=0; i<duration; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (true) { // Always report every second
                double write_rate = (double)total_write_samples.load() / (i + 1);
                double read_rate = (double)total_read_queries.load() / (i + 1);
                
                  std::cout << "[" << (i + 1) << "s] "
                          << "Writes: " << std::fixed << std::setprecision(0) << write_rate << " samples/sec, "
                          << "Reads: " << std::fixed << std::setprecision(1) << read_rate << " queries/sec" << std::endl;
            }
        }
        
         for(auto& w : write_workers) w->stop();
         for(auto& w : read_workers) w->stop();
         
         combined_write_samples_ = total_write_samples.load();
         combined_read_queries_ = total_read_queries.load();
    }
    
    void print_results() {
         std::cout << "\n--- Write Performance (gRPC) ---" << std::endl;
         std::cout << "Total Samples: " << combined_write_samples_ << std::endl;
         std::cout << "Throughput: " << std::fixed << std::setprecision(0) 
                   << (double)combined_write_samples_ / config_.write_duration_sec << " samples/sec" << std::endl;
         std::cout << "Latency p50: " << write_latencies_.percentile(50) << " ms" << std::endl;
         std::cout << "Latency p99: " << write_latencies_.percentile(99) << " ms" << std::endl;
    }

    BenchmarkConfig config_;
    LatencyTracker write_latencies_;
    LatencyTracker read_latencies_;
    LatencyTracker hot_read_latencies_;
    LatencyTracker cold_read_latencies_;
    int64_t combined_write_samples_ = 0;
    int64_t combined_read_queries_ = 0;
};

// ============================================================================
// Main
// ============================================================================

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]" << std::endl;
    std::cout << "  --grpc-host      gRPC host (default: localhost)" << std::endl;
    std::cout << "  --grpc-port      gRPC port (default: 8815)" << std::endl;
    std::cout << "  --help           Show this help" << std::endl;
}

int main(int argc, char** argv) {
     BenchmarkConfig config;
     
     for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { print_usage(argv[0]); return 0; }
        else if (arg == "--grpc-host") { if(i+1 < argc) config.grpc_host = argv[++i]; }
        else if (arg == "--grpc-port") { if(i+1 < argc) config.grpc_port = std::stoi(argv[++i]); }
        // ... parse other args
     }

     GrpcBenchmark bench(config);
     bench.run();
     
     return 0;
}
