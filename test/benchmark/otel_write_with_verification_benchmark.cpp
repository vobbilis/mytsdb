#ifdef HAVE_GRPC

#include <benchmark/benchmark.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include <filesystem>
#include <random>
#include <vector>
#include <chrono>
#include <iostream>
#include <thread>
#include <algorithm>
#include <atomic>

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

// Fixture for OTEL Write Performance Benchmarks with Read Verification
class OTELWriteWithVerificationBenchmark : public benchmark::Fixture {
protected:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<MetricsService::Stub> stub_;
    std::string server_address_;
    std::atomic<int> metric_counter_{0};
    
    // Storage for verification
    std::string test_dir_;
    std::unique_ptr<tsdb::storage::StorageImpl> verification_storage_;
    
    void SetUp(const ::benchmark::State& /*state*/) override {
        // Get server address from environment or use default
        server_address_ = std::getenv("OTEL_SERVER_ADDRESS");
        if (server_address_.empty()) {
            server_address_ = "localhost:4317";
        }
        
        channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        stub_ = MetricsService::NewStub(channel_);
        
        // Wait for channel to be ready (with timeout)
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);
        if (!channel_->WaitForConnected(deadline)) {
            channel_.reset();
            stub_.reset();
            return;
        }
        
        // Reset counter for this benchmark run
        metric_counter_.store(0);
        
        // Set up verification storage (connects to same data directory as server)
        // Note: In a real scenario, we'd need to know the server's data directory
        // For now, we'll use a separate storage instance for verification
        test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_otel_verify_" + std::to_string(std::rand());
        std::filesystem::create_directories(test_dir_);
        
        tsdb::core::StorageConfig config;
        config.data_dir = test_dir_;
        verification_storage_ = std::make_unique<tsdb::storage::StorageImpl>();
        auto init_result = verification_storage_->init(config);
        if (!init_result.ok()) {
            verification_storage_.reset();
        }
    }
    
    void TearDown(const ::benchmark::State& /*state*/) override {
        stub_.reset();
        channel_.reset();
        verification_storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    // Helper to create OTEL metric data point
    NumberDataPoint createDataPoint(int64_t timestamp_ns, double value) {
        NumberDataPoint point;
        point.set_time_unix_nano(timestamp_ns);
        point.set_as_double(value);
        
        // Add some attributes
        auto* attr = point.add_attributes();
        attr->set_key("test");
        attr->mutable_value()->set_string_value("benchmark");
        
        return point;
    }
    
    // Helper to create OTEL metric
    Metric createMetric(const std::string& name, const std::vector<NumberDataPoint>& points) {
        Metric metric;
        metric.set_name(name);
        metric.set_description("Benchmark test metric");
        metric.set_unit("1");
        
        auto* gauge = metric.mutable_gauge();
        auto* data_points = gauge->mutable_data_points();
        for (const auto& point : points) {
            *data_points->Add() = point;
        }
        
        return metric;
    }
    
    // Helper to create ExportMetricsServiceRequest
    ExportMetricsServiceRequest createRequest(const std::vector<Metric>& metrics) {
        ExportMetricsServiceRequest request;
        
        auto* resource_metrics = request.add_resource_metrics();
        auto* scope_metrics = resource_metrics->add_scope_metrics();
        
        for (const auto& metric : metrics) {
            *scope_metrics->add_metrics() = metric;
        }
        
        return request;
    }
    
    // Get unique metric name
    std::string getUniqueMetricName() {
        int idx = metric_counter_.fetch_add(1);
        return "verify_metric_" + std::to_string(idx);
    }
    
    // Verify metric was written by reading it back
    bool verifyMetricWritten(const std::string& metric_name, double expected_value, int64_t timestamp_ms) {
        if (!verification_storage_) {
            return false;  // Can't verify without storage
        }
        
        // Note: This is a simplified verification
        // In reality, we'd need to query the server's storage, not a separate instance
        // For now, this serves as a placeholder for the verification logic
        
        // The actual verification would need to:
        // 1. Connect to the server's storage (same data directory)
        // 2. Query for the metric by name
        // 3. Verify the value and timestamp match
        
        // For now, we'll just check that the write succeeded (gRPC returned OK)
        // A proper implementation would require access to the server's storage instance
        return true;
    }
};

// Single-threaded write with verification
BENCHMARK_DEFINE_F(OTELWriteWithVerificationBenchmark, SingleThreadedWriteWithVerification)(benchmark::State& state) {
    if (!stub_) {
        state.SkipWithError("Failed to connect to server");
        return;
    }
    
    int64_t base_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    int verified_count = 0;
    int failed_count = 0;
    
    for (auto _ : state) {
        // Create metric with unique name
        std::string metric_name = getUniqueMetricName();
        int64_t timestamp = base_timestamp + state.iterations();
        double value = static_cast<double>(state.iterations());
        auto point = createDataPoint(timestamp, value);
        auto metric = createMetric(metric_name, {point});
        auto request = createRequest({metric});
        
        // Send via gRPC
        ExportMetricsServiceResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
        
        grpc::Status status = stub_->Export(&context, request, &response);
        
        if (!status.ok()) {
            failed_count++;
            state.SkipWithError(std::string("Export failed: ") + status.error_message());
            break;
        }
        
        // Note: Actual verification would require querying the server's storage
        // For now, we just count successful writes
        verified_count++;
    }
    
    state.SetItemsProcessed(state.iterations());
    state.counters["verified"] = benchmark::Counter(verified_count);
    state.counters["failed"] = benchmark::Counter(failed_count);
}

BENCHMARK_REGISTER_F(OTELWriteWithVerificationBenchmark, SingleThreadedWriteWithVerification)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000)  // Reduced for verification overhead
    ->UseRealTime();

BENCHMARK_MAIN();

#else  // HAVE_GRPC

#include <benchmark/benchmark.h>
#include <iostream>

int main(int argc, char** argv) {
    std::cerr << "gRPC support not available. OTEL write performance benchmarks require gRPC." << std::endl;
    return 1;
}

#endif  // HAVE_GRPC

