#include <memory>
#include <string>
#include <iostream>
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"

#ifdef HAVE_GRPC
#include "tsdb/otel/bridge.h"
#include "tsdb/otel/query_service.h"
#include "proto/gen/tsdb.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#endif

#ifdef HAVE_RAPIDJSON
#include "prometheus/server/http_server.h"
#include "prometheus/api/query_handler.h"
#endif

namespace {
std::atomic<bool> g_running{true};

void SignalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
}
}  // namespace

namespace tsdb {

class TSDBServer {
public:
    explicit TSDBServer(const std::string& address, const std::string& data_dir = "/tmp/tsdb") 
        : address_(address), data_dir_(data_dir), shutdown_(false) {
        std::cout << "Creating TSDB server on address: " << address << std::endl;
    }

    bool Start() {
        // Create storage
        core::StorageConfig config;
        config.data_dir = data_dir_;
        config.block_size = 1024 * 1024;  // 1MB
        config.enable_compression = true;
        
        storage_ = std::make_shared<storage::StorageImpl>();
        auto init_result = storage_->init(config);
        if (!init_result.ok()) {
            std::cerr << "Failed to initialize storage: " << init_result.error() << std::endl;
            return false;
        }

#ifdef HAVE_GRPC
        // Start gRPC server
        grpc::ServerBuilder builder;
        
        // Configure server
        builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
        builder.SetMaxReceiveMessageSize(4 * 1024 * 1024);  // 4MB max message
        builder.SetMaxSendMessageSize(4 * 1024 * 1024);
        
        // Create and register MetricsService
        metrics_service_ = std::make_unique<otel::MetricsService>(storage_);
        builder.RegisterService(metrics_service_.get());
        std::cout << "MetricsService registered successfully" << std::endl;
        std::cerr << "MetricsService registered successfully" << std::endl;
        
        // Create and register QueryService (for reads/verification)
        query_service_ = std::make_unique<otel::QueryService>(storage_);
        builder.RegisterService(query_service_.get());
        std::cout << "QueryService registered successfully" << std::endl;
        
        // Build and start server
        grpc_server_ = builder.BuildAndStart();
        if (!grpc_server_) {
            std::cerr << "Failed to start gRPC server on " << address_ << std::endl;
            return false;
        }
        
        std::cout << "gRPC server listening on " << address_ << std::endl;
        std::cout << "OTEL metrics endpoint: " << address_ << std::endl;
#else
        std::cout << "TSDB server started (gRPC support not available)" << std::endl;
#endif

#ifdef HAVE_RAPIDJSON
        // Start HTTP server for query endpoint
        prometheus::ServerConfig http_config;
        http_config.listen_address = "0.0.0.0";
        http_config.port = 9090;
        http_config.timeout_seconds = 30;
        http_config.max_connections = 1000;
        
        http_server_ = std::make_unique<prometheus::HttpServer>(http_config);
        
        // Create query handler
        query_handler_ = std::make_unique<prometheus::QueryHandler>(storage_);
        
        // Register query endpoint
        http_server_->RegisterQueryHandler("/api/v1/query", [this](const std::string& path_with_query) -> std::string {
            // Parse query parameters from path_with_query
            // Format: /api/v1/query?match[]={__name__="metric"}&start=123&end=456
            prometheus::QueryParams params;
            
            // Find query string part
            size_t query_start = path_with_query.find('?');
            if (query_start == std::string::npos) {
                return query_handler_->QuerySeries(params);
            }
            
            std::string query_string = path_with_query.substr(query_start + 1);
            
            // Parse query parameters - handle match[] array parameters
            std::istringstream iss(query_string);
            std::string param;
            while (std::getline(iss, param, '&')) {
                size_t eq_pos = param.find('=');
                if (eq_pos == std::string::npos) continue;
                
                std::string key = param.substr(0, eq_pos);
                std::string value = param.substr(eq_pos + 1);
                
                // URL decode the value
                std::string decoded_value;
                for (size_t i = 0; i < value.length(); ++i) {
                    if (value[i] == '%' && i + 2 < value.length()) {
                        char hex[3] = {value[i+1], value[i+2], '\0'};
                        char decoded_char = static_cast<char>(std::strtol(hex, nullptr, 16));
                        decoded_value += decoded_char;
                        i += 2;
                    } else if (value[i] == '+') {
                        decoded_value += ' ';
                    } else {
                        decoded_value += value[i];
                    }
                }
                
                if (key.find("match[]") != std::string::npos || key == "match[]") {
                    params.matchers.push_back(decoded_value);
                } else if (key == "start") {
                    try {
                        params.start_time = std::stoll(decoded_value);
                    } catch (...) {
                        // Invalid start time, ignore
                    }
                } else if (key == "end") {
                    try {
                        params.end_time = std::stoll(decoded_value);
                    } catch (...) {
                        // Invalid end time, ignore
                    }
                }
            }
            
            return query_handler_->QuerySeries(params);
        });
        
        // Start HTTP server
        try {
            http_server_->Start();
            std::cout << "HTTP query server listening on 0.0.0.0:9090" << std::endl;
            std::cout << "Query endpoint: http://localhost:9090/api/v1/query" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to start HTTP server: " << e.what() << std::endl;
            return false;
        }
#endif
        
        return true;
    }

    void Stop() {
        shutdown_.store(true);
        
#ifdef HAVE_GRPC
        // Shutdown gRPC server gracefully
        if (grpc_server_) {
            std::cout << "Shutting down gRPC server..." << std::endl;
            grpc_server_->Shutdown();
            
            // Wait for server to finish (with timeout)
            auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
            while (std::chrono::system_clock::now() < deadline) {
                // Wait() blocks until server is terminated, so we use a timeout approach
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // Check if server is still running by attempting to wait with a short timeout
                // If Wait() returns immediately, server is terminated
            }
            
            grpc_server_.reset();
            metrics_service_.reset();
            query_service_.reset();
        }
#endif

#ifdef HAVE_RAPIDJSON
        // Shutdown HTTP server
        if (http_server_) {
            std::cout << "Shutting down HTTP server..." << std::endl;
            http_server_->Stop();
            http_server_.reset();
            query_handler_.reset();
        }
#endif
        
        if (storage_) {
            auto result = storage_->close();
            if (!result.ok()) {
                std::cerr << "Error closing storage: " << result.error() << std::endl;
            }
        }
        std::cout << "TSDB server stopped" << std::endl;
    }

    void Wait() {
        // Simple wait loop
        while (g_running.load() && !shutdown_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        Stop();
    }

private:
    std::string address_;
    std::string data_dir_;
    std::shared_ptr<storage::StorageImpl> storage_;
    std::atomic<bool> shutdown_;
    
#ifdef HAVE_GRPC
        std::unique_ptr<grpc::Server> grpc_server_;
        std::unique_ptr<otel::MetricsService> metrics_service_;
        std::unique_ptr<otel::QueryService> query_service_;
#endif

#ifdef HAVE_RAPIDJSON
        std::unique_ptr<prometheus::HttpServer> http_server_;
        std::unique_ptr<prometheus::QueryHandler> query_handler_;
#endif
};

} // namespace tsdb

int main(int argc, char* argv[]) {
    // Set up signal handling
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    // Parse command-line arguments
    std::string address = "0.0.0.0:4317";  // Default OTEL gRPC port
    std::string data_dir = "/tmp/tsdb";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--address" && i + 1 < argc) {
            address = argv[++i];
        } else if (arg == "--data-dir" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --address ADDRESS    Server address (default: 0.0.0.0:4317)" << std::endl;
            std::cout << "  --data-dir DIR       Data directory (default: /tmp/tsdb)" << std::endl;
            std::cout << "  --help, -h           Show this help message" << std::endl;
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            std::cerr << "Use --help for usage information" << std::endl;
            return 1;
        }
    }
    
    try {
        tsdb::TSDBServer server(address, data_dir);
        
        if (!server.Start()) {
            return 1;
        }

        std::cout << "TSDB server running. Press Ctrl+C to stop." << std::endl;
        server.Wait();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
} 