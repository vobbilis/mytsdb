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

// Debug test: Write a single metric with 40 labels and verify it can be read back
class VerificationDebugTest : public ::testing::Test {
protected:
    std::string server_address_ = "127.0.0.1:4320";
    std::filesystem::path test_data_dir_;
    pid_t server_pid_ = 0;
    std::unique_ptr<MetricsService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
    
    void SetUp() override {
        // Create unique data directory
        test_data_dir_ = std::filesystem::temp_directory_path() / 
                        ("tsdb_verify_debug_" + std::to_string(std::rand()));
        std::filesystem::create_directories(test_data_dir_);
        
        // Start server as subprocess
        std::string server_cmd = std::string(TSDB_SERVER_PATH) + 
                                " --address " + server_address_ + 
                                " --data-dir " + test_data_dir_.string() + 
                                " > /tmp/verify_debug_server.log 2>&1";
        
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

// Test: Write one metric with 40 labels and verify it can be queried
TEST_F(VerificationDebugTest, WriteAndQuerySingleMetric) {
    std::string metric_name = "test_verify_metric";
    int64_t timestamp = 1234567890000000;
    double value = 42.0;
    
    // Create data point with 40 attributes
    auto point = createDataPoint(timestamp, value, 40);
    std::cout << "Created data point with " << point.attributes_size() << " attributes" << std::endl;
    
    // Create and send request
    auto request = createRequest(metric_name, point);
    
    std::cout << "Sending gRPC Export request..." << std::endl;
    ExportMetricsServiceResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    
    grpc::Status status = stub_->Export(&context, request, &response);
    ASSERT_TRUE(status.ok()) << "gRPC Export failed: " << status.error_message();
    std::cout << "Export succeeded" << std::endl;
    
    // Wait for server to flush
    std::cout << "Waiting 3 seconds for server to flush writes..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Check server log
    std::ifstream log_file("/tmp/verify_debug_server.log");
    std::string log_content((std::istreambuf_iterator<char>(log_file)),
                           std::istreambuf_iterator<char>());
    
    std::cout << "\n=== SERVER LOG (last 50 lines) ===" << std::endl;
    std::vector<std::string> lines;
    std::istringstream iss(log_content);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    for (size_t i = lines.size() > 50 ? lines.size() - 50 : 0; i < lines.size(); ++i) {
        std::cout << lines[i] << std::endl;
    }
    std::cout << "===================================\n" << std::endl;
    
    // Create storage instance to read back data
    std::cout << "Initializing verification storage..." << std::endl;
    tsdb::core::StorageConfig config;
    config.data_dir = test_data_dir_.string();
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>();
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Failed to initialize verification storage";
    std::cout << "Verification storage initialized" << std::endl;
    
    // Try different query strategies
    std::cout << "\n=== Testing Query Strategies ===" << std::endl;
    
    // Strategy 1: Query by __name__ only
    std::cout << "\n1. Query by __name__ only:" << std::endl;
    std::vector<tsdb::core::LabelMatcher> matchers1;
    matchers1.emplace_back(tsdb::core::MatcherType::Equal, "__name__", metric_name);
    
    int64_t start_time = timestamp / 1000000 - 1000;
    int64_t end_time = timestamp / 1000000 + 1000;
    
    auto query_result1 = storage->query(matchers1, start_time, end_time);
    if (query_result1.ok()) {
        const auto& results1 = query_result1.value();
        std::cout << "  Found " << results1.size() << " series" << std::endl;
        for (size_t i = 0; i < results1.size() && i < 3; ++i) {
            const auto& series = results1[i];
            const auto& labels = series.labels();
            std::cout << "  Series " << i << ": " << labels.map().size() << " labels" << std::endl;
            std::cout << "  Labels: " << labels.to_string() << std::endl;
        }
    } else {
        std::cout << "  Query failed: " << query_result1.error() << std::endl;
    }
    
    // Strategy 2: Query by __name__ + first attribute
    std::cout << "\n2. Query by __name__ + attr0:" << std::endl;
    std::vector<tsdb::core::LabelMatcher> matchers2;
    matchers2.emplace_back(tsdb::core::MatcherType::Equal, "__name__", metric_name);
    matchers2.emplace_back(tsdb::core::MatcherType::Equal, "attr0", "val0");
    
    auto query_result2 = storage->query(matchers2, start_time, end_time);
    if (query_result2.ok()) {
        const auto& results2 = query_result2.value();
        std::cout << "  Found " << results2.size() << " series" << std::endl;
    } else {
        std::cout << "  Query failed: " << query_result2.error() << std::endl;
    }
    
    // Strategy 3: List all series in index
    std::cout << "\n3. Checking index directly (if possible)..." << std::endl;
    // Note: We can't directly access the index, but we can try broader queries
    
    // Strategy 4: Query with all 40 attributes (should be unique)
    std::cout << "\n4. Query by __name__ + multiple attributes:" << std::endl;
    std::vector<tsdb::core::LabelMatcher> matchers4;
    matchers4.emplace_back(tsdb::core::MatcherType::Equal, "__name__", metric_name);
    matchers4.emplace_back(tsdb::core::MatcherType::Equal, "attr0", "val0");
    matchers4.emplace_back(tsdb::core::MatcherType::Equal, "attr1", "val1");
    matchers4.emplace_back(tsdb::core::MatcherType::Equal, "attr2", "val2");
    
    auto query_result4 = storage->query(matchers4, start_time, end_time);
    if (query_result4.ok()) {
        const auto& results4 = query_result4.value();
        std::cout << "  Found " << results4.size() << " series" << std::endl;
        if (!results4.empty()) {
            const auto& series = results4[0];
            const auto& labels = series.labels();
            std::cout << "  Series has " << labels.map().size() << " labels" << std::endl;
            std::cout << "  Value: " << (series.samples().empty() ? "N/A" : std::to_string(series.samples()[0].value())) << std::endl;
        }
    } else {
        std::cout << "  Query failed: " << query_result4.error() << std::endl;
    }
    
    storage->close();
}

