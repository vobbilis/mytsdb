#ifdef HAVE_GRPC

#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include <filesystem>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <atomic>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

namespace tsdb {
namespace otel_grpc {
namespace e2e {

class GRPCServerE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for server data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_grpc_e2e_test";
        std::filesystem::create_directories(test_dir_);
        
        // Find server executable
        server_exe_ = findServerExecutable();
        ASSERT_FALSE(server_exe_.empty()) << "Server executable not found";
        
        // Use a unique port for each test
        port_ = 43170 + (getpid() % 1000);  // Use PID to avoid conflicts
        server_address_ = "localhost:" + std::to_string(port_);
        
        server_pid_ = -1;
    }
    
    void TearDown() override {
        stopServer();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::string findServerExecutable() {
        // Try common build locations
        std::vector<std::string> paths = {
            "build/src/tsdb/tsdb_server",
            "../build/src/tsdb/tsdb_server",
            "../../build/src/tsdb/tsdb_server",
            "./tsdb_server"
        };
        
        for (const auto& path : paths) {
            if (std::filesystem::exists(path)) {
                return std::filesystem::absolute(path).string();
            }
        }
        return "";
    }
    
    bool startServer() {
        if (server_pid_ > 0) {
            return true;  // Already running
        }
        
#if defined(__unix__) || defined(__APPLE__)
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: start server
            std::string data_dir = test_dir_.string();
            std::string address = "0.0.0.0:" + std::to_string(port_);
            
            // Redirect output to log file
            std::string log_file = (test_dir_ / "server.log").string();
            int fd = open(log_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            
            execl(server_exe_.c_str(), 
                  server_exe_.c_str(),
                  "--address", address.c_str(),
                  "--data-dir", data_dir.c_str(),
                  nullptr);
            
            // If execl fails
            exit(1);
        } else if (pid > 0) {
            server_pid_ = pid;
            
            // Wait for server to be ready (max 10 seconds)
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (std::chrono::steady_clock::now() < deadline) {
                // Try to connect to server
                auto channel = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
                auto stub = MetricsService::NewStub(channel);
                
                // Try a simple call to check if server is ready
                grpc::ClientContext context;
                context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(1));
                
                ExportMetricsServiceRequest request;
                ExportMetricsServiceResponse response;
                grpc::Status status = stub->Export(&context, request, &response);
                
                // If we get any response (even an error), server is up
                if (status.error_code() != grpc::StatusCode::UNAVAILABLE) {
                    return true;
                }
                
                // Check if process is still running
                if (waitpid(server_pid_, nullptr, WNOHANG) != 0) {
                    // Process died
                    server_pid_ = -1;
                    
                    // Print server log
                    std::string log_file = (test_dir_ / "server.log").string();
                    std::ifstream log_stream(log_file);
                    if (log_stream) {
                        std::cout << "Server failed to start. Log content:" << std::endl;
                        std::cout << log_stream.rdbuf() << std::endl;
                    } else {
                        std::cout << "Server failed to start. Could not open log file: " << log_file << std::endl;
                    }
                    
                    return false;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Timeout - server didn't start
            std::cout << "Server startup timed out after 10 seconds" << std::endl;
            stopServer();
            return false;
        } else {
            std::cout << "Fork failed: " << strerror(errno) << std::endl;
            return false;  // Fork failed
        }
#else
        // On non-Unix systems, assume server is already running
        // or skip these tests
        std::cout << "Not on Unix, skipping server start" << std::endl;
        return false;
#endif
    }
    
    void stopServer() {
#if defined(__unix__) || defined(__APPLE__)
        if (server_pid_ > 0) {
            std::cout << "Stopping server with PID " << server_pid_ << std::endl;
            kill(server_pid_, SIGTERM);
            
            // Wait for process to exit (max 5 seconds)
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < deadline) {
                int status;
                pid_t result = waitpid(server_pid_, &status, WNOHANG);
                if (result == server_pid_) {
                    std::cout << "Server exited with status " << status << std::endl;
                    server_pid_ = -1;
                    return;
                } else if (result == -1) {
                    std::cout << "waitpid failed: " << strerror(errno) << std::endl;
                    // If ECHILD, process is already gone
                    if (errno == ECHILD) {
                        server_pid_ = -1;
                        return;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Force kill if still running
            if (server_pid_ > 0) {
                std::cout << "Server did not exit, sending SIGKILL" << std::endl;
                kill(server_pid_, SIGKILL);
                waitpid(server_pid_, nullptr, 0);
                server_pid_ = -1;
            }
        }
#endif
    }
    
    std::shared_ptr<MetricsService::Stub> createStub() {
        auto channel = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        return MetricsService::NewStub(channel);
    }
    
    ExportMetricsServiceRequest createTestRequest(const std::string& metric_name, double value) {
        ExportMetricsServiceRequest request;
        
        auto* resource_metrics = request.add_resource_metrics();
        auto* scope_metrics = resource_metrics->add_scope_metrics();
        auto* metric = scope_metrics->add_metrics();
        
        metric->set_name(metric_name);
        metric->set_description("Test metric");
        metric->set_unit("1");
        
        auto* gauge = metric->mutable_gauge();
        auto* data_point = gauge->add_data_points();
        
        int64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        data_point->set_time_unix_nano(timestamp_ns);
        data_point->set_as_double(value);
        
        // Add some attributes
        auto* attr = data_point->add_attributes();
        attr->set_key("test");
        attr->mutable_value()->set_string_value("e2e_test");
        
        return request;
    }
    
    std::filesystem::path test_dir_;
    std::string server_exe_;
    std::string server_address_;
    int port_;
    pid_t server_pid_;
};

TEST_F(GRPCServerE2ETest, ServerStartsSuccessfully) {
    ASSERT_TRUE(startServer()) << "Failed to start gRPC server";
    
    // Server should be running
    ASSERT_GT(server_pid_, 0);
    
    // Try to create a stub and connect
    auto stub = createStub();
    ASSERT_NE(stub, nullptr);
}

TEST_F(GRPCServerE2ETest, ExportSingleMetric) {
    ASSERT_TRUE(startServer()) << "Failed to start gRPC server";
    
    auto stub = createStub();
    ASSERT_NE(stub, nullptr);
    
    // Create and send a test metric
    auto request = createTestRequest("test_metric", 42.0);
    
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    
    ExportMetricsServiceResponse response;
    grpc::Status status = stub->Export(&context, request, &response);
    
    ASSERT_TRUE(status.ok()) << "Export failed: " << status.error_message();
}

TEST_F(GRPCServerE2ETest, ExportMultipleMetrics) {
    ASSERT_TRUE(startServer()) << "Failed to start gRPC server";
    
    auto stub = createStub();
    ASSERT_NE(stub, nullptr);
    
    // Create request with multiple metrics
    ExportMetricsServiceRequest request;
    auto* resource_metrics = request.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    
    int64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    for (int i = 0; i < 5; ++i) {
        auto* metric = scope_metrics->add_metrics();
        metric->set_name("test_metric_" + std::to_string(i));
        metric->set_description("Test metric " + std::to_string(i));
        metric->set_unit("1");
        
        auto* gauge = metric->mutable_gauge();
        auto* data_point = gauge->add_data_points();
        data_point->set_time_unix_nano(timestamp_ns);
        data_point->set_as_double(static_cast<double>(i * 10));
    }
    
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    
    ExportMetricsServiceResponse response;
    grpc::Status status = stub->Export(&context, request, &response);
    
    ASSERT_TRUE(status.ok()) << "Export failed: " << status.error_message();
}

TEST_F(GRPCServerE2ETest, ServerShutdownGracefully) {
    ASSERT_TRUE(startServer()) << "Failed to start gRPC server";
    
    // Verify server is running
    auto stub = createStub();
    ASSERT_NE(stub, nullptr);
    
    // Stop server
    stopServer();
    
    // Wait a bit for shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify server is no longer accepting connections
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(1));
    
    ExportMetricsServiceRequest request = createTestRequest("test", 1.0);
    ExportMetricsServiceResponse response;
    grpc::Status status = stub->Export(&context, request, &response);
    
    // Should fail with UNAVAILABLE
    ASSERT_EQ(status.error_code(), grpc::StatusCode::UNAVAILABLE);
}

TEST_F(GRPCServerE2ETest, ConcurrentExports) {
    ASSERT_TRUE(startServer()) << "Failed to start gRPC server";
    
    const int num_threads = 4;
    const int metrics_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            auto stub = createStub();
            for (int i = 0; i < metrics_per_thread; ++i) {
                auto request = createTestRequest(
                    "concurrent_metric_" + std::to_string(t) + "_" + std::to_string(i),
                    static_cast<double>(t * 100 + i)
                );
                
                grpc::ClientContext context;
                context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
                
                ExportMetricsServiceResponse response;
                grpc::Status status = stub->Export(&context, request, &response);
                
                if (status.ok()) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(success_count.load(), num_threads * metrics_per_thread);
    EXPECT_EQ(failure_count.load(), 0);
}

} // namespace e2e
} // namespace otel_grpc
} // namespace tsdb

#else  // HAVE_GRPC

// Dummy test when gRPC is not available
#include <gtest/gtest.h>

TEST(GRPCServerE2ETest, DISABLED_GRPCNotAvailable) {
    GTEST_SKIP() << "gRPC support not available";
}

#endif  // HAVE_GRPC

