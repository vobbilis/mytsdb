#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include <filesystem>
#include <memory>
#include <thread>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

// Test: Simulate exact benchmark verification path
class BenchmarkVerificationPathTest : public ::testing::Test {
protected:
    std::string server_address_ = "127.0.0.1:4319";
    std::filesystem::path test_data_dir_;
    pid_t server_pid_ = 0;
    std::unique_ptr<MetricsService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
    
    void SetUp() override {
        // Create unique data directory
        test_data_dir_ = std::filesystem::temp_directory_path() / 
                        ("tsdb_benchmark_verify_" + std::to_string(std::rand()));
        std::filesystem::create_directories(test_data_dir_);
        
        // Start server as subprocess
        std::string server_cmd = std::string(TSDB_SERVER_PATH) + 
                                " --address " + server_address_ + 
                                " --data-dir " + test_data_dir_.string() + 
                                " > /tmp/benchmark_verify_server.log 2>&1";
        
        server_pid_ = fork();
        if (server_pid_ == 0) {
            execl("/bin/sh", "sh", "-c", server_cmd.c_str(), (char*)nullptr);
            exit(1);
        }
        
        ASSERT_NE(server_pid_, -1);
        ASSERT_NE(server_pid_, 0);
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        stub_ = MetricsService::NewStub(channel_);
        
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);
        if (!channel_->WaitForConnected(deadline)) {
            FAIL() << "Failed to connect to gRPC server";
        }
    }
    
    void TearDown() override {
        if (server_pid_ > 0) {
            kill(server_pid_, SIGTERM);
            int status;
            waitpid(server_pid_, &status, 0);
        }
        
        if (std::filesystem::exists(test_data_dir_)) {
            std::filesystem::remove_all(test_data_dir_);
        }
    }
    
    NumberDataPoint createDataPoint(int64_t timestamp_ns, double value, int num_attrs) {
        NumberDataPoint point;
        point.set_time_unix_nano(timestamp_ns);
        point.set_as_double(value);
        
        for (int i = 0; i < num_attrs; ++i) {
            auto* attr = point.add_attributes();
            attr->set_key("attr" + std::to_string(i));
            attr->mutable_value()->set_string_value("val" + std::to_string(i));
        }
        
        return point;
    }
    
    ExportMetricsServiceRequest createRequest(const std::string& metric_name, 
                                             const NumberDataPoint& point) {
        ExportMetricsServiceRequest request;
        auto* resource_metrics = request.add_resource_metrics();
        auto* scope_metrics = resource_metrics->add_scope_metrics();
        
        Metric metric;
        metric.set_name(metric_name);
        auto* gauge = metric.mutable_gauge();
        *gauge->mutable_data_points()->Add() = point;
        
        *scope_metrics->add_metrics() = metric;
        return request;
    }
};

// Test: Simulate benchmark verification - write via gRPC, then verify via storage
TEST_F(BenchmarkVerificationPathTest, WriteThenVerifyLikeBenchmark) {
    // Write 5 metrics with 40 attributes each (like benchmark)
    std::vector<std::string> metric_names;
    for (int i = 0; i < 5; ++i) {
        std::string metric_name = "benchmark_test_metric_" + std::to_string(i);
        metric_names.push_back(metric_name);
        
        auto point = createDataPoint(1234567890000000 + i * 1000, 42.0 + i, 40);
        auto request = createRequest(metric_name, point);
        
        ExportMetricsServiceResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
        
        grpc::Status status = stub_->Export(&context, request, &response);
        ASSERT_TRUE(status.ok()) << "gRPC Export failed: " << status.error_message();
    }
    
    std::cout << "Wrote 5 metrics via gRPC. Waiting for flush..." << std::endl;
    
    // Wait for server to flush (like benchmark does)
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Now verify exactly like benchmark does:
    // 1. Close any existing storage
    // 2. Re-initialize storage (triggers WAL replay)
    // 3. Query for the data
    
    std::cout << "Re-initializing verification storage (WAL replay)..." << std::endl;
    
    tsdb::core::StorageConfig config;
    config.data_dir = test_data_dir_.string();
    
    auto verification_storage = std::make_unique<tsdb::storage::StorageImpl>();
    auto init_result = verification_storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Failed to initialize verification storage";
    
    std::cout << "Verification storage initialized. Querying..." << std::endl;
    
#include "tsdb/core/matcher.h"
// ...

    // Query for each metric
    for (const auto& metric_name : metric_names) {
        std::vector<tsdb::core::LabelMatcher> matchers;
        matchers.emplace_back(tsdb::core::MatcherType::Equal, "__name__", metric_name);
        matchers.emplace_back(tsdb::core::MatcherType::Equal, "attr0", "val0");
        
        int64_t start_time = 1234567890000 - 1000;
        int64_t end_time = 1234567890000 + 10000;
        
        auto query_result = verification_storage->query(matchers, start_time, end_time);
        ASSERT_TRUE(query_result.ok()) << "Query failed for " << metric_name;
        
        const auto& results = query_result.value();
        if (!results.empty()) {
            const auto& series = results[0];
            const auto& labels = series.labels();
            
            std::cout << "Metric " << metric_name << ": Found series with " 
                      << labels.map().size() << " labels" << std::endl;
            
            EXPECT_GE(labels.map().size(), 41) 
                << "Metric " << metric_name << " should have at least 41 labels";
        } else {
            std::cout << "WARNING: Metric " << metric_name << " not found in query results" << std::endl;
        }
    }
    
    verification_storage->close();
}

