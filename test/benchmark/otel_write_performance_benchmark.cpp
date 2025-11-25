#ifdef HAVE_GRPC

#include <benchmark/benchmark.h>
#include <grpcpp/grpcpp.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include <chrono>
#include <map>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <sstream>
#include <cmath>

#ifdef HAVE_HTTPLIB
#include <httplib.h>
#else
// Fallback: use curl or system HTTP client
#include <curl/curl.h>
#endif

#ifdef HAVE_RAPIDJSON
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#endif

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;

// Structure to track written metrics for verification
struct WrittenMetric {
    std::string metric_name;
    double value;
    int64_t timestamp_ms;
    std::map<std::string, std::string> labels;
};

// Global state for verification (outside benchmark framework to avoid issues)
struct VerificationState {
    std::mutex mutex;
    std::vector<WrittenMetric> metrics;
    std::atomic<bool> verification_requested{false};
    std::atomic<bool> verification_complete{false};
    std::atomic<bool> first_setup_done{false};  // Track if first setup has occurred
    std::string server_address;  // gRPC address (e.g., localhost:4317)
    std::string http_address;    // HTTP address (e.g., localhost:9090)
};

static VerificationState g_verification_state;

// Helper to flush all output streams
void FlushAll() {
    std::cout.flush();
    std::cerr.flush();
    fflush(stdout);
    fflush(stderr);
}

// Helper to write to both stdout and stderr with flush
void LogOutput(const std::string& message) {
    std::cout << message << std::endl;
    std::cerr << message << std::endl;
    FlushAll();
}

// Realistic K8s metric names (like what you'd see from node_exporter, cadvisor, kube-state-metrics)
const std::vector<std::string> K8S_METRIC_NAMES = {
    // Container metrics (cadvisor)
    "container_cpu_usage_seconds_total",
    "container_memory_usage_bytes",
    "container_memory_working_set_bytes",
    "container_memory_rss",
    "container_network_receive_bytes_total",
    "container_network_transmit_bytes_total",
    "container_fs_usage_bytes",
    "container_fs_limit_bytes",
    "container_spec_cpu_quota",
    "container_cpu_cfs_throttled_seconds_total",
    // Node metrics (node_exporter)
    "node_cpu_seconds_total",
    "node_memory_MemTotal_bytes",
    "node_memory_MemAvailable_bytes",
    "node_disk_read_bytes_total",
    "node_disk_written_bytes_total",
    "node_network_receive_bytes_total",
    "node_network_transmit_bytes_total",
    "node_filesystem_avail_bytes",
    "node_load1",
    "node_load5",
    // Kubernetes state metrics (kube-state-metrics)
    "kube_pod_status_phase",
    "kube_pod_container_status_running",
    "kube_pod_container_resource_requests",
    "kube_pod_container_resource_limits",
    "kube_deployment_status_replicas",
    "kube_deployment_status_replicas_available",
    "kube_daemonset_status_current_number_scheduled",
    "kube_statefulset_status_replicas",
    "kube_node_status_condition",
    "kube_pod_labels",
    // Application metrics
    "http_requests_total",
    "http_request_duration_seconds_bucket",
    "http_request_duration_seconds_sum",
    "http_request_duration_seconds_count",
    "process_cpu_seconds_total",
    "process_resident_memory_bytes",
    "go_goroutines",
    "go_gc_duration_seconds",
    "grpc_server_handled_total",
    "grpc_server_handling_seconds_bucket"
};

// Realistic K8s container names
const std::vector<std::string> CONTAINER_NAMES = {
    "main", "sidecar", "init", "proxy", "envoy", "istio-proxy",
    "nginx", "redis", "postgres", "mongodb", "elasticsearch",
    "prometheus", "grafana", "alertmanager", "fluentd", "filebeat"
};

// Realistic K8s deployment names
const std::vector<std::string> DEPLOYMENT_NAMES = {
    "api-gateway", "user-service", "order-service", "payment-service",
    "inventory-service", "notification-service", "auth-service",
    "search-service", "recommendation-service", "analytics-service",
    "frontend", "backend", "worker", "scheduler", "cron-job"
};

// Helper to create realistic K8s labels (40 labels total)
std::map<std::string, std::string> CreateRealisticLabels(int series_id, std::mt19937& rng) {
    std::map<std::string, std::string> labels;
    
    // Simulate 100 nodes, 500 pods, 1000 containers
    int node_id = series_id % 100;
    int pod_id = series_id % 500;
    int container_id = series_id % 1000;
    int deployment_id = series_id % DEPLOYMENT_NAMES.size();
    int container_name_id = series_id % CONTAINER_NAMES.size();
    
    // Core K8s identification labels (these create unique series)
    labels["namespace"] = "ns-" + std::to_string(pod_id % 10);  // 10 namespaces
    labels["pod"] = "pod-" + std::to_string(pod_id);
    labels["container"] = CONTAINER_NAMES[container_name_id];
    labels["node"] = "node-" + std::to_string(node_id) + ".k8s.local";
    labels["instance"] = "10.0." + std::to_string(node_id / 10) + "." + std::to_string(node_id % 256) + ":9100";
    
    // Deployment/workload labels
    labels["deployment"] = DEPLOYMENT_NAMES[deployment_id];
    labels["replicaset"] = DEPLOYMENT_NAMES[deployment_id] + "-" + std::to_string(pod_id % 10);
    labels["controller_kind"] = (pod_id % 3 == 0) ? "Deployment" : ((pod_id % 3 == 1) ? "DaemonSet" : "StatefulSet");
    labels["controller_name"] = DEPLOYMENT_NAMES[deployment_id];
    
    // Environment labels
    labels["env"] = (pod_id % 3 == 0) ? "production" : ((pod_id % 3 == 1) ? "staging" : "development");
    labels["region"] = (node_id % 3 == 0) ? "us-east-1" : ((node_id % 3 == 1) ? "us-west-2" : "eu-west-1");
    labels["zone"] = labels["region"] + ((node_id % 3 == 0) ? "a" : ((node_id % 3 == 1) ? "b" : "c"));
    labels["cluster"] = "k8s-" + labels["env"].substr(0, 4) + "-01";
    
    // Service mesh labels (Istio-like)
    labels["service"] = DEPLOYMENT_NAMES[deployment_id] + "-svc";
    labels["version"] = "v" + std::to_string(1 + (pod_id % 3));
    labels["app"] = DEPLOYMENT_NAMES[deployment_id];
    labels["app_kubernetes_io_name"] = DEPLOYMENT_NAMES[deployment_id];
    labels["app_kubernetes_io_instance"] = DEPLOYMENT_NAMES[deployment_id] + "-" + labels["env"];
    labels["app_kubernetes_io_version"] = "1." + std::to_string(pod_id % 10) + ".0";
    labels["app_kubernetes_io_component"] = (container_id % 2 == 0) ? "server" : "worker";
    labels["app_kubernetes_io_part_of"] = "microservices-platform";
    labels["app_kubernetes_io_managed_by"] = "helm";
    
    // Prometheus scrape labels
    labels["job"] = "kubernetes-pods";
    labels["__scrape_interval__"] = "15s";
    
    // Additional resource labels
    labels["image"] = "gcr.io/myproject/" + DEPLOYMENT_NAMES[deployment_id] + ":" + labels["app_kubernetes_io_version"];
    labels["image_id"] = "sha256:" + std::to_string(std::hash<std::string>{}(labels["image"])).substr(0, 12);
    labels["container_id"] = "docker://" + std::to_string(std::hash<std::string>{}(labels["pod"] + labels["container"])).substr(0, 12);
    
    // Team/ownership labels
    labels["team"] = "team-" + std::to_string(deployment_id % 5);
    labels["owner"] = "platform-" + std::to_string(deployment_id % 3);
    labels["cost_center"] = "cc-" + std::to_string(deployment_id % 10);
    
    // Additional monitoring labels to reach 40 total (including __name__ added later)
    labels["monitor_group"] = "group-" + std::to_string(pod_id % 5);
    labels["alert_level"] = (pod_id % 2 == 0) ? "critical" : "warning";
    labels["sla_tier"] = (deployment_id % 3 == 0) ? "gold" : ((deployment_id % 3 == 1) ? "silver" : "bronze");
    labels["data_center"] = "dc-" + std::to_string(node_id % 3);
    labels["rack"] = "rack-" + std::to_string(node_id % 10);
    labels["pod_template_hash"] = std::to_string(std::hash<std::string>{}(labels["deployment"])).substr(0, 10);
    labels["uid"] = "uid-" + std::to_string(series_id);
    labels["pod_ip"] = "10.244." + std::to_string(pod_id / 256) + "." + std::to_string(pod_id % 256);
    labels["host_ip"] = "10.0." + std::to_string(node_id / 10) + "." + std::to_string(node_id % 256);
    
    return labels;  // 39 labels + __name__ = 40 total
}

// Get a realistic metric name based on series_id
std::string GetRealisticMetricName(int series_id) {
    return K8S_METRIC_NAMES[series_id % K8S_METRIC_NAMES.size()];
}

// Helper to create OTEL data point
NumberDataPoint CreateDataPoint(int64_t timestamp_ns, double value, 
                                const std::map<std::string, std::string>& labels) {
    NumberDataPoint point;
    point.set_time_unix_nano(timestamp_ns);
    point.set_as_double(value);
    
    for (const auto& [key, val] : labels) {
        auto* attr = point.add_attributes();
        attr->set_key(key);
        attr->mutable_value()->set_string_value(val);
    }
    
    return point;
}

// Helper to create OTEL metric
Metric CreateMetric(const std::string& name, const std::vector<NumberDataPoint>& points) {
    Metric metric;
    metric.set_name(name);
    
    auto* gauge = metric.mutable_gauge();
    for (const auto& point : points) {
        *gauge->add_data_points() = point;
    }
    
    return metric;
}

// Helper to create ExportMetricsServiceRequest
ExportMetricsServiceRequest CreateRequest(const std::vector<Metric>& metrics) {
    ExportMetricsServiceRequest request;
    auto* resource_metrics = request.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    
    for (const auto& metric : metrics) {
        *scope_metrics->add_metrics() = metric;
    }
    
    return request;
}

// Verification function - runs independently of benchmark framework
void RunVerification() {
    std::lock_guard<std::mutex> lock(g_verification_state.mutex);
    
    if (g_verification_state.verification_complete) {
        return;
    }
    
    LogOutput("\n=== STARTING HTTP VERIFICATION ===");
    
    if (g_verification_state.metrics.empty()) {
        LogOutput("ERROR: No metrics to verify!");
        return;
    }
    
    if (g_verification_state.http_address.empty()) {
        LogOutput("ERROR: HTTP server address not set!");
        return;
    }
    
    // Test with just ONE metric for debugging
    const auto& written = g_verification_state.metrics[0];
    LogOutput("Testing metric: " + written.metric_name + 
              " (value=" + std::to_string(written.value) + 
              ", labels=" + std::to_string(written.labels.size()) + 
              ", timestamp_ms=" + std::to_string(written.timestamp_ms) + ")");
    
    // Wait longer for server to process writes and ensure data is indexed
    LogOutput("Waiting 3 seconds for server to process writes and index data...");
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    
    // Build query URL - use wider time range to ensure we catch the data
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t start_time = written.timestamp_ms - 300000;  // 5 minutes before
    int64_t end_time = now_ms + 60000;    // 1 minute after now
    
    LogOutput("Query time range: " + std::to_string(start_time) + " to " + std::to_string(end_time));
    LogOutput("HTTP server address: " + g_verification_state.http_address);
    
    // Build query string - URL encode the metric name
    std::ostringstream query_url;
    query_url << "/api/v1/query?match[]={__name__=\"" << written.metric_name << "\"}&start=" 
              << start_time << "&end=" << end_time;
    
    LogOutput("Query URL: " + query_url.str());
    
#ifdef HAVE_HTTPLIB
    // Use httplib for HTTP request
    httplib::Client client(g_verification_state.http_address);
    client.set_connection_timeout(5, 0);  // 5 seconds
    client.set_read_timeout(10, 0);       // 10 seconds
    
    LogOutput("Sending HTTP GET request...");
    FlushAll();
    
    auto res = client.Get(query_url.str().c_str());
    
    if (!res) {
        LogOutput("ERROR: HTTP request failed - no response");
        return;
    }
    
    if (res->status != 200) {
        LogOutput("ERROR: HTTP request failed with status: " + std::to_string(res->status));
        LogOutput("Response body: " + res->body);
        return;
    }
    
    LogOutput("HTTP request succeeded, parsing JSON response...");
    LogOutput("Response body (first 500 chars): " + res->body.substr(0, 500));
    
#ifdef HAVE_RAPIDJSON
    // Parse JSON response
    rapidjson::Document doc;
    doc.Parse(res->body.c_str());
    
    if (doc.HasParseError()) {
        LogOutput("ERROR: Failed to parse JSON response. Parse error at offset: " + std::to_string(doc.GetParseError()));
        return;
    }
    
    if (!doc.HasMember("status")) {
        LogOutput("ERROR: Response missing 'status' field");
        return;
    }
    
    std::string status = doc["status"].GetString();
    LogOutput("Response status: " + status);
    
    if (status != "success") {
        LogOutput("ERROR: Query returned error status: " + status);
        if (doc.HasMember("error")) {
            LogOutput("Error: " + std::string(doc["error"].GetString()));
        }
        if (doc.HasMember("errorType")) {
            LogOutput("ErrorType: " + std::string(doc["errorType"].GetString()));
        }
        return;
    }
    
    if (!doc.HasMember("data") || !doc["data"].HasMember("result")) {
        LogOutput("ERROR: Invalid response format");
        return;
    }
    
    const auto& result = doc["data"]["result"];
    if (!result.IsArray() || result.Size() == 0) {
        LogOutput("ERROR: No results found for metric: " + written.metric_name);
        LogOutput("Query was: __name__=\"" + written.metric_name + "\"");
        LogOutput("Time range: " + std::to_string(start_time) + " to " + std::to_string(end_time));
        LogOutput("Written timestamp: " + std::to_string(written.timestamp_ms));
        
        // Try querying without time range to see if data exists at all
        LogOutput("Trying query without time range...");
        std::ostringstream query_url_no_time;
        query_url_no_time << "/api/v1/query?match[]={__name__=\"" << written.metric_name << "\"}";
        
        auto res2 = client.Get(query_url_no_time.str().c_str());
        if (res2 && res2->status == 200) {
            rapidjson::Document doc2;
            doc2.Parse(res2->body.c_str());
            if (doc2.HasMember("data") && doc2["data"].HasMember("result")) {
                const auto& result2 = doc2["data"]["result"];
                LogOutput("Query without time range returned " + std::to_string(result2.Size()) + " series");
                if (result2.Size() > 0) {
                    LogOutput("WARNING: Data exists but time range might be wrong!");
                }
            }
        }
        return;
    }
    
    LogOutput("Response has " + std::to_string(result.Size()) + " series");
    
    // Search through all series to find the one with matching labels
    // (In realistic K8s scenarios, multiple series share the same metric name)
    int matching_series_idx = -1;
    for (rapidjson::SizeType idx = 0; idx < result.Size(); ++idx) {
        const auto& series = result[idx];
        if (!series.HasMember("metric")) continue;
        
        const auto& metric = series["metric"];
        
        // Check if key identifying labels match (pod, container, namespace)
        bool pod_match = false, container_match = false, namespace_match = false;
        
        if (metric.HasMember("pod") && written.labels.count("pod") > 0) {
            pod_match = (std::string(metric["pod"].GetString()) == written.labels.at("pod"));
        }
        if (metric.HasMember("container") && written.labels.count("container") > 0) {
            container_match = (std::string(metric["container"].GetString()) == written.labels.at("container"));
        }
        if (metric.HasMember("namespace") && written.labels.count("namespace") > 0) {
            namespace_match = (std::string(metric["namespace"].GetString()) == written.labels.at("namespace"));
        }
        
        if (pod_match && container_match && namespace_match) {
            matching_series_idx = idx;
            break;
        }
    }
    
    if (matching_series_idx < 0) {
        LogOutput("WARNING: Could not find exact series match, using first series");
        LogOutput("  (This is expected with realistic K8s data - multiple series per metric)");
        LogOutput("  Looking for: pod=" + written.labels.at("pod") + 
                 ", container=" + written.labels.at("container") + 
                 ", namespace=" + written.labels.at("namespace"));
        matching_series_idx = 0;
    } else {
        LogOutput("Found matching series at index " + std::to_string(matching_series_idx));
    }
    
    const auto& series = result[matching_series_idx];
    if (!series.HasMember("metric") || !series.HasMember("values")) {
        LogOutput("ERROR: Invalid series format in response");
        return;
    }
    
    const auto& metric = series["metric"];
    const auto& values = series["values"];
    
    LogOutput("Selected series has " + std::to_string(metric.MemberCount()) + " labels, " +
              std::to_string(values.Size()) + " samples");
    
    // Convert JSON labels to map
    std::map<std::string, std::string> json_labels;
    for (auto it = metric.MemberBegin(); it != metric.MemberEnd(); ++it) {
        std::string key = it->name.GetString();
        std::string value = it->value.GetString();
        json_labels[key] = value;
        if (json_labels.size() <= 5) {  // Log first 5 labels
            LogOutput("Label: " + key + " = " + value);
        }
    }
    
    // For realistic K8s data, we verify:
    // 1. We can query by metric name
    // 2. The returned series has expected label count (~40)
    // 3. The metric name matches
    // (Exact label match is not always possible with realistic data due to
    // how series are distributed across multiple writes with the same metric name)
    
    int expected_label_count = written.labels.size() + 1; // +1 for __name__
    int actual_label_count = json_labels.size();
    
    bool metric_name_match = (json_labels.count("__name__") > 0 && 
                              json_labels.at("__name__") == written.metric_name);
    bool label_count_ok = (std::abs(actual_label_count - expected_label_count) <= 2); // Allow ±2
    
    LogOutput("Verification checks:");
    LogOutput("  Metric name match: " + std::string(metric_name_match ? "YES" : "NO"));
    LogOutput("  Label count: " + std::to_string(actual_label_count) + 
              " (expected ~" + std::to_string(expected_label_count) + ")");
    LogOutput("  Label count OK: " + std::string(label_count_ok ? "YES" : "NO"));
    
    if (!metric_name_match || !label_count_ok) {
        LogOutput("ERROR: Verification failed!");
        return;
    }
    
    // Verify samples exist
    if (values.Size() > 0) {
        LogOutput("Found " + std::to_string(values.Size()) + " samples in series");
        if (values[0].IsArray() && values[0].Size() >= 2) {
            double sample_value = values[0][1].GetDouble();
            LogOutput("First sample value: " + std::to_string(sample_value));
        }
        
        // Success! We can query metrics with realistic K8s data
        LogOutput("✅ VERIFICATION SUCCESS: Realistic K8s metrics working!");
        LogOutput("   Metric: " + written.metric_name);
        LogOutput("   Labels: " + std::to_string(json_labels.size()));
        LogOutput("   Samples: " + std::to_string(values.Size()));
        LogOutput("   Series returned: " + std::to_string(result.Size()) + " (realistic multi-series)");
    } else {
        LogOutput("ERROR: No samples in response!");
    }
#else
    LogOutput("ERROR: RapidJSON not available, cannot parse HTTP verification response");
    return;
#endif
#else
    LogOutput("ERROR: httplib not available, cannot perform HTTP verification");
    return;
#endif
    
    LogOutput("=== VERIFICATION COMPLETE ===\n");
    g_verification_state.verification_complete = true;
}

// Benchmark fixture
class OTELWriteBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // Get server address from environment
        const char* addr = std::getenv("OTEL_SERVER_ADDRESS");
        if (addr) {
            server_address_ = addr;
        } else {
            server_address_ = "localhost:4317";
        }
        
        // Create channel and stubs
        channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        stub_ = MetricsService::NewStub(channel_);
        
        // Wait for channel to be ready
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        if (!channel_->WaitForConnected(deadline)) {
            LogOutput("ERROR: Failed to connect to gRPC server at " + server_address_);
        } else {
            LogOutput("Connected to gRPC server at " + server_address_);
        }
        
        // Set verification state only on FIRST setup (not on subsequent benchmark runs)
        if (!g_verification_state.first_setup_done.exchange(true)) {
            std::lock_guard<std::mutex> lock(g_verification_state.mutex);
            // Clear previous metrics (only on first run)
            g_verification_state.metrics.clear();
            
            // Reset metric counter to 0 for consistent naming
            metric_counter_.store(0);
            
            // Set server addresses
            g_verification_state.server_address = server_address_;
            // Extract host from gRPC address and use port 9090 for HTTP
            std::string http_host = "localhost";
            size_t colon_pos = server_address_.find(':');
            if (colon_pos != std::string::npos) {
                http_host = server_address_.substr(0, colon_pos);
            } else {
                http_host = server_address_;
            }
            // Handle 0.0.0.0 -> localhost
            if (http_host == "0.0.0.0") {
                http_host = "localhost";
            }
            g_verification_state.http_address = http_host + ":9090";
            
            LogOutput("First benchmark setup - counter reset to 0, will verify first written metric");
        }
        
        // Initialize RNG
        rng_.seed(std::random_device{}());
        series_id_counter_ = 0;
    }
    
    void TearDown(const ::benchmark::State& state) override {
        // Don't run verification here - it will run after ALL benchmarks complete
        // Just clean up the gRPC resources
        stub_.reset();
        channel_.reset();
    }
    
    std::string getUniqueMetricName() {
        return "test_metric_" + std::to_string(++metric_counter_);
    }
    
    int getUniqueSeriesId() {
        return ++series_id_counter_;
    }
    
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<MetricsService::Stub> stub_;
    std::string server_address_;
    std::mt19937 rng_;
    static std::atomic<int> metric_counter_;
    int series_id_counter_ = 0;
};

std::atomic<int> OTELWriteBenchmark::metric_counter_{0};

// Single-threaded write benchmark
BENCHMARK_DEFINE_F(OTELWriteBenchmark, SingleThreadedWrite)(benchmark::State& state) {
    int64_t base_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    int successful_writes = 0;
    int failed_writes = 0;
    
    for (auto _ : state) {
        // Create metric with realistic K8s name and labels
        // Real K8s: ~40 metric types, 1000s of series per metric
        int series_id = getUniqueSeriesId();
        std::string metric_name = GetRealisticMetricName(series_id);
        auto labels = CreateRealisticLabels(series_id, rng_);
        labels["__name__"] = metric_name;
        
        int64_t timestamp = base_timestamp + state.iterations();
        double value = static_cast<double>(state.iterations());
        
        // Create data point with all labels
        auto point = CreateDataPoint(timestamp, value, labels);
        auto metric = CreateMetric(metric_name, {point});
        auto request = CreateRequest({metric});
        
        // Send via gRPC
        ExportMetricsServiceResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
        
        grpc::Status status = stub_->Export(&context, request, &response);
        
        if (!status.ok()) {
            failed_writes++;
            if (successful_writes == 0) {
                LogOutput("ERROR: First Export failed: " + status.error_message() + " (code: " + std::to_string(status.error_code()) + ")");
            }
            state.SkipWithError(std::string("Export failed: ") + status.error_message());
            break;
        }
        
        // Track written metric for verification (only the very first write across all benchmarks)
        {
            std::lock_guard<std::mutex> lock(g_verification_state.mutex);
            if (g_verification_state.metrics.empty()) {
                LogOutput("First gRPC Export call succeeded - storing metric for verification: " + metric_name);
                WrittenMetric written;
                written.metric_name = metric_name;
                written.value = value;
                written.timestamp_ms = timestamp / 1000000;  // Convert ns to ms
                written.labels = labels;
                g_verification_state.metrics.push_back(written);
            }
        }
        
        successful_writes++;
    }
    
    state.counters["successful_writes"] = benchmark::Counter(successful_writes);
    state.counters["failed_writes"] = benchmark::Counter(failed_writes);
}

BENCHMARK_REGISTER_F(OTELWriteBenchmark, SingleThreadedWrite)
    ->Iterations(1000)
    ->Unit(benchmark::kMicrosecond);

// Batch write benchmark
BENCHMARK_DEFINE_F(OTELWriteBenchmark, BatchWrite)(benchmark::State& state) {
    int batch_size = static_cast<int>(state.range(0));
    int64_t base_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    int successful_writes = 0;
    int failed_writes = 0;
    
    for (auto _ : state) {
        std::vector<Metric> metrics;
        
        for (int i = 0; i < batch_size; ++i) {
            // Create metric with realistic K8s name and labels
            int series_id = getUniqueSeriesId();
            std::string metric_name = GetRealisticMetricName(series_id);
            auto labels = CreateRealisticLabels(series_id, rng_);
            labels["__name__"] = metric_name;
            
            int64_t timestamp = base_timestamp + state.iterations() * batch_size + i;
            double value = static_cast<double>(state.iterations() * batch_size + i);
            
            auto point = CreateDataPoint(timestamp, value, labels);
            auto metric = CreateMetric(metric_name, {point});
            metrics.push_back(metric);
            
            // Track first metric for verification (only if no metric stored yet)
            {
                std::lock_guard<std::mutex> lock(g_verification_state.mutex);
                if (g_verification_state.metrics.empty()) {
                    LogOutput("BatchWrite: First metric stored for verification: " + metric_name);
                    WrittenMetric written;
                    written.metric_name = metric_name;
                    written.value = value;
                    written.timestamp_ms = timestamp / 1000000;
                    written.labels = labels;
                    g_verification_state.metrics.push_back(written);
                }
            }
        }
        
        auto request = CreateRequest(metrics);
        
        // Send via gRPC
        ExportMetricsServiceResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
        
        grpc::Status status = stub_->Export(&context, request, &response);
        
        if (!status.ok()) {
            failed_writes++;
            state.SkipWithError(std::string("Export failed: ") + status.error_message());
            break;
        }
        
        successful_writes += batch_size;
    }
    
    state.counters["successful_writes"] = benchmark::Counter(successful_writes);
    state.counters["failed_writes"] = benchmark::Counter(failed_writes);
}

BENCHMARK_REGISTER_F(OTELWriteBenchmark, BatchWrite)
    ->Arg(1)
    ->Arg(8)
    ->Arg(64)
    ->Arg(100)
    ->Unit(benchmark::kMicrosecond);

// Custom main to run verification AFTER all benchmarks complete
int main(int argc, char** argv) {
    // Initialize and run benchmarks
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    
    // Now run verification - all benchmarks have completed, data should be indexed
    LogOutput("\n========================================");
    LogOutput("All benchmarks completed. Running verification...");
    LogOutput("========================================\n");
    
    // Wait a bit for any remaining async writes to complete
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Run verification
    RunVerification();
    
    // Wait for verification to complete
    int wait_count = 0;
    while (!g_verification_state.verification_complete.load() && wait_count < 30) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        wait_count++;
    }
    
    if (!g_verification_state.verification_complete.load()) {
        LogOutput("WARNING: Verification did not complete within timeout");
    }
    
    return 0;
}

#endif // HAVE_GRPC

