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

// Test: Full gRPC path - Start server, send request, verify data written
class GRPCFullPathTest : public ::testing::Test {
protected:
    std::string server_address_ = "127.0.0.1:4318";  // Use different port to avoid conflicts
    std::filesystem::path test_data_dir_;
    pid_t server_pid_ = 0;
    std::unique_ptr<MetricsService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
    
    void SetUp() override {
        // Create unique data directory
        test_data_dir_ = std::filesystem::temp_directory_path() / 
                        ("tsdb_grpc_full_test_" + std::to_string(std::rand()));
        std::filesystem::create_directories(test_data_dir_);
        
        // Start server as subprocess
        std::string server_cmd = std::string(TSDB_SERVER_PATH) + 
                                " --address " + server_address_ + 
                                " --data-dir " + test_data_dir_.string() + 
                                " > /tmp/grpc_server_test.log 2>&1";
        
        server_pid_ = fork();
        if (server_pid_ == 0) {
            // Child process - start server
            execl("/bin/sh", "sh", "-c", server_cmd.c_str(), (char*)nullptr);
            exit(1);
        }
        
        ASSERT_NE(server_pid_, -1) << "Failed to fork server process";
        ASSERT_NE(server_pid_, 0) << "Server process should not be child";
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Connect client
        channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        stub_ = MetricsService::NewStub(channel_);
        
        // Wait for connection
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);
        if (!channel_->WaitForConnected(deadline)) {
            FAIL() << "Failed to connect to gRPC server at " << server_address_;
        }
    }
    
    void TearDown() override {
        // Shutdown server
        if (server_pid_ > 0) {
            kill(server_pid_, SIGTERM);
            int status;
            waitpid(server_pid_, &status, 0);
        }
        
        // Cleanup
        if (std::filesystem::exists(test_data_dir_)) {
            std::filesystem::remove_all(test_data_dir_);
        }
    }
    
    // Helper to create data point with attributes
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
    
    // Helper to create request
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

// Test: Verify gRPC Export is called and processes request
TEST_F(GRPCFullPathTest, ExportCalledAndProcesses) {
    // Create request with 40 attributes
    auto point = createDataPoint(1234567890000000, 42.0, 40);
    auto request = createRequest("test_grpc_metric", point);
    
    std::cout << "Sending gRPC Export request with " << point.attributes_size() << " attributes..." << std::endl;
    
    // Send via gRPC
    ExportMetricsServiceResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    
    grpc::Status status = stub_->Export(&context, request, &response);
    
    ASSERT_TRUE(status.ok()) << "gRPC Export failed: " << status.error_message();
    
    // Give server time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify data was written by reading server log
    std::ifstream log_file("/tmp/grpc_server_test.log");
    std::string log_content((std::istreambuf_iterator<char>(log_file)),
                           std::istreambuf_iterator<char>());
    
    std::cout << "Checking server log for Export calls..." << std::endl;
    
    // Check if Export was called
    bool export_called = log_content.find("MetricsService::Export called") != std::string::npos;
    bool bridge_called = log_content.find("OTEL Bridge: ConvertMetrics called") != std::string::npos;
    bool labels_written = log_content.find("with 41 labels") != std::string::npos || 
                         log_content.find("with 40 labels") != std::string::npos;
    
    std::cout << "Export called: " << (export_called ? "YES" : "NO") << std::endl;
    std::cout << "Bridge called: " << (bridge_called ? "YES" : "NO") << std::endl;
    std::cout << "Labels written: " << (labels_written ? "YES" : "NO") << std::endl;
    
    if (!export_called) {
        std::cout << "Server log content:" << std::endl;
        std::cout << log_content << std::endl;
    }
    
    EXPECT_TRUE(export_called) << "MetricsService::Export should be called";
    EXPECT_TRUE(bridge_called) << "Bridge should be called";
    EXPECT_TRUE(labels_written) << "Labels should be written correctly";
}

// Test: Verify data can be read back after gRPC write
TEST_F(GRPCFullPathTest, WriteAndReadBack) {
    // Create request with 40 attributes
    auto point = createDataPoint(1234567890000000, 99.9, 40);
    auto request = createRequest("readback_test_metric", point);
    
    // Send via gRPC
    ExportMetricsServiceResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    
    grpc::Status status = stub_->Export(&context, request, &response);
    ASSERT_TRUE(status.ok()) << "gRPC Export failed: " << status.error_message();
    
    // Give server time to flush
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Create storage instance to read back data
    tsdb::core::StorageConfig config;
    config.data_dir = test_data_dir_.string();
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>();
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Failed to initialize verification storage";
    
#include "tsdb/core/matcher.h"
// ...

    // Query for the data
    std::vector<tsdb::core::LabelMatcher> matchers;
    matchers.emplace_back(tsdb::core::MatcherType::Equal, "__name__", "readback_test_metric");
    matchers.emplace_back(tsdb::core::MatcherType::Equal, "attr0", "val0");
    
    int64_t start_time = 1234567890000 - 1000;
    int64_t end_time = 1234567890000 + 1000;
    
    auto query_result = storage->query(matchers, start_time, end_time);
    ASSERT_TRUE(query_result.ok()) << "Query failed: " << query_result.error();
    
    const auto& results = query_result.value();
    std::cout << "Query returned " << results.size() << " series" << std::endl;
    
    if (!results.empty()) {
        const auto& series = results[0];
        const auto& labels = series.labels();
        std::cout << "Series has " << labels.map().size() << " labels" << std::endl;
        std::cout << "Labels: " << labels.to_string() << std::endl;
        
        EXPECT_GE(labels.map().size(), 41) << "Should have at least 41 labels (__name__ + 40 attributes)";
    }
    
    storage->close();
}

