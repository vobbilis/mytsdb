#ifdef HAVE_GRPC

#include <benchmark/benchmark.h>
#include <grpcpp/grpcpp.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "tsdb/proto/gen/tsdb.pb.h"
#include "tsdb/proto/gen/tsdb.grpc.pb.h"
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

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace tsdb::proto;

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
    std::string server_address;
    std::unique_ptr<TSDBService::Stub> query_stub;
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

// Helper to create realistic labels (40 labels total)
std::map<std::string, std::string> CreateRealisticLabels(int series_id, std::mt19937& rng) {
    std::map<std::string, std::string> labels;
    
    // Common labels
    labels["instance"] = "instance-" + std::to_string(series_id % 100);
    labels["job"] = "test-job-" + std::to_string(series_id % 10);
    labels["host"] = "host-" + std::to_string(series_id % 50);
    labels["pod"] = "pod-" + std::to_string(series_id % 200);
    labels["namespace"] = "namespace-" + std::to_string(series_id % 5);
    labels["service"] = "service-" + std::to_string(series_id % 20);
    labels["env"] = (series_id % 2 == 0) ? "production" : "staging";
    labels["region"] = (series_id % 3 == 0) ? "us-east-1" : ((series_id % 3 == 1) ? "us-west-2" : "eu-west-1");
    labels["zone"] = "zone-" + std::to_string(series_id % 3);
    labels["cluster"] = "cluster-" + std::to_string(series_id % 5);
    
    // Additional labels to reach 40 total
    for (int i = 10; i < 40; ++i) {
        labels["label_" + std::to_string(i)] = "value_" + std::to_string(series_id % 100) + "_" + std::to_string(i);
    }
    
    return labels;
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
    
    LogOutput("\n=== STARTING VERIFICATION ===");
    
    if (g_verification_state.metrics.empty()) {
        LogOutput("ERROR: No metrics to verify!");
        return;
    }
    
    if (g_verification_state.server_address.empty()) {
        LogOutput("ERROR: Server address not set!");
        return;
    }
    
    // Test with just ONE metric for debugging
    const auto& written = g_verification_state.metrics[0];
    LogOutput("Testing metric: " + written.metric_name + 
              " (value=" + std::to_string(written.value) + 
              ", labels=" + std::to_string(written.labels.size()) + ")");
    
    // Wait for server to process writes
    LogOutput("Waiting 2 seconds for server to process writes...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Build query request
    QueryParams request;
    auto* name_matcher = request.add_matchers();
    name_matcher->set_type(LabelMatcher_Type_EQ);
    name_matcher->set_name("__name__");
    name_matcher->set_value(written.metric_name);
    
    auto* time_range = request.mutable_time_range();
    int64_t start_time = written.timestamp_ms - 60000;  // 1 minute before
    int64_t end_time = written.timestamp_ms + 60000;    // 1 minute after
    time_range->set_start_time(start_time);
    time_range->set_end_time(end_time);
    
    LogOutput("Query time range: " + std::to_string(start_time) + " to " + std::to_string(end_time));
    LogOutput("Server address: " + g_verification_state.server_address);
    
    // Create a fresh channel for query to ensure it's connected
    auto query_channel = grpc::CreateChannel(g_verification_state.server_address, grpc::InsecureChannelCredentials());
    auto query_stub = TSDBService::NewStub(query_channel);
    
    // Wait for channel to be ready
    LogOutput("Waiting for query channel to connect...");
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    if (!query_channel->WaitForConnected(deadline)) {
        LogOutput("ERROR: Query channel failed to connect to " + g_verification_state.server_address);
        return;
    }
    LogOutput("Query channel connected successfully!");
    
    // Query server
    SeriesResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
    
    LogOutput("Calling GetSeries()...");
    FlushAll();
    
    grpc::Status status = query_stub->GetSeries(&context, request, &response);
    
    std::string status_msg = status.ok() ? "OK" : ("FAILED: " + status.error_message() + " (code: " + std::to_string(status.error_code()) + ")");
    LogOutput("GetSeries() returned. Status: " + status_msg);
    
    if (!status.ok()) {
        LogOutput("ERROR: gRPC Query failed: " + status.error_message());
        return;
    }
    
    LogOutput("Response has " + std::to_string(response.series_size()) + " series");
    
    if (response.series_size() == 0) {
        LogOutput("ERROR: No results found for metric: " + written.metric_name);
        return;
    }
    
    // Check first series
    const auto& proto_series = response.series(0);
    LogOutput("First series has " + std::to_string(proto_series.labels_size()) + " labels, " +
              std::to_string(proto_series.samples_size()) + " samples");
    
    // Convert proto labels to map
    std::map<std::string, std::string> proto_labels;
    for (int j = 0; j < proto_series.labels_size(); ++j) {
        const auto& label = proto_series.labels(j);
        proto_labels[label.name()] = label.value();
        if (j < 5) {  // Log first 5 labels
            LogOutput("Label[" + std::to_string(j) + "]: " + label.name() + " = " + label.value());
        }
    }
    
    // Verify labels match
    bool labels_match = true;
    int missing_labels = 0;
    for (const auto& [key, val] : written.labels) {
        if (proto_labels.count(key) == 0 || proto_labels.at(key) != val) {
            if (missing_labels < 3) {  // Log first 3 mismatches
                LogOutput("ERROR: Label mismatch: key=" + key + 
                         ", expected=" + val + 
                         ", found=" + (proto_labels.count(key) ? proto_labels.at(key) : "<missing>"));
            }
            labels_match = false;
            missing_labels++;
        }
    }
    
    if (!labels_match) {
        LogOutput("ERROR: " + std::to_string(missing_labels) + " labels do not match!");
        return;
    }
    
    // Verify value
    if (proto_series.samples_size() > 0) {
        const auto& sample = proto_series.samples(0);
        LogOutput("Sample value: " + std::to_string(sample.value()) + " (expected: " + std::to_string(written.value) + ")");
        
        if (std::abs(sample.value() - written.value) < 0.001) {
            LogOutput("✅ VERIFICATION SUCCESS: Metric verified correctly!");
            LogOutput("   Metric: " + written.metric_name);
            LogOutput("   Value: " + std::to_string(sample.value()));
            LogOutput("   Labels: " + std::to_string(proto_labels.size()));
        } else {
            LogOutput("ERROR: Value mismatch: expected " + std::to_string(written.value) + 
                     ", got " + std::to_string(sample.value()));
        }
    } else {
        LogOutput("ERROR: No samples in response!");
    }
    
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
        
        // Initialize query stub for verification (only once)
        {
            std::lock_guard<std::mutex> lock(g_verification_state.mutex);
            if (!g_verification_state.query_stub) {
                g_verification_state.server_address = server_address_;
                g_verification_state.query_stub = TSDBService::NewStub(channel_);
            }
        }
        
        // Initialize RNG
        rng_.seed(std::random_device{}());
        series_id_counter_ = 0;
    }
    
    void TearDown(const ::benchmark::State& state) override {
        // Request verification after first benchmark completes
        if (!g_verification_state.verification_requested.exchange(true)) {
            // Run verification in a separate thread to avoid blocking
            std::thread verification_thread([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Brief delay
                RunVerification();
            });
            verification_thread.detach();
        }
        
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
        // Create metric with unique name and realistic labels
        std::string metric_name = getUniqueMetricName();
        int series_id = getUniqueSeriesId();
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
            state.SkipWithError(std::string("Export failed: ") + status.error_message());
            break;
        }
        
        // Track written metric for verification (only first one)
        if (successful_writes == 0) {
            std::lock_guard<std::mutex> lock(g_verification_state.mutex);
            WrittenMetric written;
            written.metric_name = metric_name;
            written.value = value;
            written.timestamp_ms = timestamp / 1000000;  // Convert ns to ms
            written.labels = labels;
            g_verification_state.metrics.push_back(written);
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
            std::string metric_name = getUniqueMetricName();
            int series_id = getUniqueSeriesId();
            auto labels = CreateRealisticLabels(series_id, rng_);
            labels["__name__"] = metric_name;
            
            int64_t timestamp = base_timestamp + state.iterations() * batch_size + i;
            double value = static_cast<double>(state.iterations() * batch_size + i);
            
            auto point = CreateDataPoint(timestamp, value, labels);
            auto metric = CreateMetric(metric_name, {point});
            metrics.push_back(metric);
            
            // Track first metric for verification
            if (successful_writes == 0 && i == 0) {
                std::lock_guard<std::mutex> lock(g_verification_state.mutex);
                WrittenMetric written;
                written.metric_name = metric_name;
                written.value = value;
                written.timestamp_ms = timestamp / 1000000;
                written.labels = labels;
                g_verification_state.metrics.push_back(written);
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

BENCHMARK_MAIN();

#endif // HAVE_GRPC
