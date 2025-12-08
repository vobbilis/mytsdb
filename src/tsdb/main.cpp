#include <memory>
#include <string>
#include <iostream>
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>
#include "tsdb/common/logger.h"
#include <spdlog/spdlog.h>
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/prometheus/server/http_server.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/prometheus/api/query_handler.h"
#include "tsdb/prometheus/api/labels.h"
#include "tsdb/prometheus/remote/write_handler.h"
#include "tsdb/prometheus/remote/read_handler.h"
#include "tsdb/prometheus/auth/no_auth.h"
#include "tsdb/server/self_monitor.h"

#if defined(HAVE_GRPC) && defined(ENABLE_OTEL)
#include "tsdb/otel/bridge.h"
#include "tsdb/otel/query_service.h"
#include "proto/gen/tsdb.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#endif

#include "tsdb/storage/filtering_storage.h"
#include "tsdb/storage/rule_manager.h"
#include "tsdb/storage/derived_metrics.h"
#include "tsdb/storage/write_performance_instrumentation.h"

#ifdef HAVE_ARROW_FLIGHT
#include "tsdb/arrow/flight_server.h"
#endif

// Global flag for shutdown
std::atomic<bool> g_running(true);

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
        g_running.store(false);
    }
}

namespace tsdb {

class TSDBServer {
public:
    explicit TSDBServer(const std::string& address, int http_port, int arrow_port, const std::string& data_dir = "/tmp/tsdb") 
        : address_(address), http_port_(http_port), arrow_port_(arrow_port), data_dir_(data_dir), shutdown_(false) {
        std::cout << "Creating TSDB server on address: " << address << ", HTTP port: " << http_port;
#ifdef HAVE_ARROW_FLIGHT
        std::cout << ", Arrow port: " << arrow_port;
#endif
        std::cout << std::endl;
    }

    bool Start() {
        // Create storage
        core::StorageConfig config;
        config.data_dir = data_dir_;
        config.block_size = 1024 * 1024;  // 1MB
        config.enable_compression = true;
        
        // Enable background processing with aggressive compaction
        config.background_config = core::BackgroundConfig::Default();
        config.background_config.enable_auto_compaction = true;
        config.background_config.compaction_interval = std::chrono::milliseconds(10000);  // 10 seconds
        
        // 1. Create Base Storage (StorageImpl)
        auto base_storage = std::make_shared<storage::StorageImpl>();
        
        // 2. Create Rule Manager
        rule_manager_ = std::make_shared<storage::RuleManager>();
        
        // 3. Create Filtering Storage (Decorator)
        filtering_storage_ = std::make_shared<storage::FilteringStorage>(
            std::static_pointer_cast<storage::Storage>(base_storage), 
            rule_manager_);
        
        // 4. Assign to main storage interface
        storage_ = filtering_storage_;
        
        auto init_result = storage_->init(config);
        if (!init_result.ok()) {
            std::cerr << "Failed to initialize storage: " << init_result.error() << std::endl;
            return false;
        }

        // Initialize Prometheus components
        try {
            // 1. Create TSDB Adapter
            tsdb_adapter_ = std::make_shared<prometheus::storage::TSDBAdapter>(storage_);
            
            // 2. Create Engine
            prometheus::promql::EngineOptions engine_opts;
            engine_opts.storage_adapter = tsdb_adapter_.get();
            engine_ = std::make_shared<prometheus::promql::Engine>(engine_opts);
            
            // 3. Create Query Handler
            query_handler_ = std::make_unique<prometheus::api::QueryHandler>(engine_);
            
            // 4. Create and Start HTTP Server
            prometheus::ServerConfig server_config;
            server_config.port = static_cast<uint16_t>(http_port_);
            server_config.num_threads = 16;  // Increase threads to handle concurrent load
            http_server_ = std::make_unique<prometheus::HttpServer>(server_config);
            
            // Register handlers
            
            // Register Remote Write handler
            auto authenticator = std::make_shared<prometheus::auth::NoAuthenticator>();
            auto write_handler = std::make_shared<prometheus::remote::WriteHandler>(
                std::static_pointer_cast<storage::Storage>(storage_), 
                std::static_pointer_cast<prometheus::auth::Authenticator>(authenticator));
            http_server_->RegisterHandler("/api/v1/write", 
                [write_handler](const prometheus::Request& req, std::string& res) {
                    write_handler->Handle(req, res);
                });
            
            // Register Remote Read handler
            auto read_handler = std::make_shared<prometheus::remote::ReadHandler>(
                std::static_pointer_cast<storage::Storage>(storage_), 
                std::static_pointer_cast<prometheus::auth::Authenticator>(authenticator));
            http_server_->RegisterHandler("/api/v1/read",
                [read_handler](const prometheus::Request& req, std::string& res) {
                    read_handler->Handle(req, res);
                });

            http_server_->RegisterHandler("/api/v1/query", [this](const prometheus::Request& req, std::string& res) {
                query_handler_->HandleInstantQuery(req, res);
            });
            
            http_server_->RegisterHandler("/api/v1/query_range", [this](const prometheus::Request& req, std::string& res) {
                query_handler_->HandleRangeQuery(req, res);
            });

            // 3b. Create Labels Handler
            labels_handler_ = std::make_unique<prometheus::api::LabelsHandler>(storage_);

            // Register metadata handlers
            http_server_->RegisterHandler("/api/v1/labels", [this](const prometheus::Request& req, std::string& res) {
                prometheus::api::LabelQueryParams params;
                if (!req.GetParam("start").empty()) {
                    try { params.start_time = static_cast<int64_t>(std::stod(req.GetParam("start")) * 1000); } catch (...) {}
                }
                if (!req.GetParam("end").empty()) {
                    try { params.end_time = static_cast<int64_t>(std::stod(req.GetParam("end")) * 1000); } catch (...) {}
                }
                params.matchers = req.GetMultiParam("match[]");
                
                res = labels_handler_->GetLabels(params).ToJSON();
            });

            http_server_->RegisterHandler("/api/v1/label/:name/values", [this](const prometheus::Request& req, std::string& res) {
                prometheus::api::LabelQueryParams params;
                if (!req.GetParam("start").empty()) {
                    try { params.start_time = static_cast<int64_t>(std::stod(req.GetParam("start")) * 1000); } catch (...) {}
                }
                if (!req.GetParam("end").empty()) {
                    try { params.end_time = static_cast<int64_t>(std::stod(req.GetParam("end")) * 1000); } catch (...) {}
                }
                params.matchers = req.GetMultiParam("match[]");
                
                std::string name = req.GetPathParam("name");
                res = labels_handler_->GetLabelValues(name, params).ToJSON();
            });

            http_server_->RegisterHandler("/api/v1/series", [this](const prometheus::Request& req, std::string& res) {
                prometheus::api::LabelQueryParams params;
                if (!req.GetParam("start").empty()) {
                    try { params.start_time = static_cast<int64_t>(std::stod(req.GetParam("start")) * 1000); } catch (...) {}
                }
                if (!req.GetParam("end").empty()) {
                    try { params.end_time = static_cast<int64_t>(std::stod(req.GetParam("end")) * 1000); } catch (...) {}
                }
                // Series endpoint requires matchers
                std::vector<std::string> matchers = req.GetMultiParam("match[]");
                
                res = labels_handler_->GetSeries(matchers, params).ToJSON();
            });
            
            // --- New API Endpoints for Filtering ---
            
            // POST /api/v1/config/drop-rules
            http_server_->RegisterHandler("/api/v1/config/drop-rules", [this](const prometheus::Request& req, std::string& res) {
                if (req.method != "POST") {
                    res = "{\"status\":\"error\",\"error\":\"Method not allowed\"}";
                    return;
                }
                // Simple payload: {"selector": "up{env='dev'}"}
                // For now, just assume body IS the selector for simplicity or parse JSON if we had a parser handy.
                // Let's assume the body contains the selector string directly for this MVP.
                std::string selector = req.body;
                if (selector.empty()) {
                    res = "{\"status\":\"error\",\"error\":\"Empty selector\"}";
                    return;
                }
                rule_manager_->add_drop_rule(selector);
                res = "{\"status\":\"success\"}";
            });
            
            // POST /api/v1/config/derived-metrics
            http_server_->RegisterHandler("/api/v1/config/derived-metrics", [this](const prometheus::Request& req, std::string& res) {
                if (req.method != "POST") {
                    res = "{\"status\":\"error\",\"error\":\"Method not allowed\"}";
                    return;
                }
                // Payload: name|query|interval_ms
                // Very simple parsing for MVP
                std::string body = req.body;
                size_t p1 = body.find('|');
                size_t p2 = body.find('|', p1 + 1);
                if (p1 == std::string::npos || p2 == std::string::npos) {
                    res = "{\"status\":\"error\",\"error\":\"Invalid format. Expected: name|query|interval_ms\"}";
                    return;
                }
                std::string name = body.substr(0, p1);
                std::string query = body.substr(p1 + 1, p2 - p1 - 1);
                int64_t interval = std::stoll(body.substr(p2 + 1));
                
                derived_metric_manager_->add_rule(name, query, interval);
                res = "{\"status\":\"success\"}";
            });
            
            http_server_->Start();
            std::cout << "Prometheus HTTP server listening on port " << http_port_ << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to start Prometheus components: " << e.what() << std::endl;
            return false;
        }

#if defined(HAVE_GRPC) && defined(ENABLE_OTEL)
        // Start gRPC server
        grpc::ServerBuilder builder;
        
        // Configure server
        builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
        builder.SetMaxReceiveMessageSize(4 * 1024 * 1024);  // 4MB max message
        builder.SetMaxSendMessageSize(4 * 1024 * 1024);
        
        // Create and register MetricsService
        metrics_service_ = std::make_unique<otel::MetricsService>(storage_);
        builder.RegisterService(metrics_service_.get());
        
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
        std::cout << "TSDB server started (gRPC/OTEL support not available)" << std::endl;
#endif
        
        // Start self-monitoring
        std::cout << "[Main] Initializing self-monitoring..." << std::endl;
        auto bg_processor = base_storage->GetBackgroundProcessor();
        if (bg_processor) {
            std::cout << "[Main] Background processor obtained successfully" << std::endl;
            self_monitor_ = std::make_unique<server::SelfMonitor>(storage_, bg_processor);
            self_monitor_->Start();
            std::cout << "[Main] Self-monitoring started" << std::endl;
            
            // Initialize Derived Metric Manager
            derived_metric_manager_ = std::make_unique<storage::DerivedMetricManager>(storage_, bg_processor);
            derived_metric_manager_->start();
            std::cout << "[Main] Derived Metric Manager started" << std::endl;
            
        } else {
            std::cerr << "[Main] ERROR: Failed to get background processor!" << std::endl;
        }
        
#ifdef HAVE_ARROW_FLIGHT
        // Start Arrow Flight server
        if (arrow_port_ > 0) {
            arrow_server_ = std::make_unique<arrow::MetricsFlightServer>(storage_);
            auto status = arrow_server_->Init(arrow_port_);
            if (status.ok()) {
                // Run server in background thread
                arrow_server_thread_ = std::thread([this]() {
                    arrow_server_->Serve();
                });
                std::cout << "Arrow Flight server listening on port " << arrow_port_ << std::endl;
            } else {
                std::cerr << "Failed to start Arrow Flight server: " << status.ToString() << std::endl;
            }
        }
#endif
        
        return true;
    }

    void Stop() {
        shutdown_.store(true);
        
        // Print write performance summary immediately on shutdown
        tsdb::storage::WritePerformanceInstrumentation::instance().print_summary();
        
        if (derived_metric_manager_) {
            std::cout << "Stopping derived metric manager..." << std::endl;
            derived_metric_manager_->stop();
        }
        
        if (self_monitor_) {
            std::cout << "Stopping self-monitor..." << std::endl;
            self_monitor_->Stop();
        }
        
        if (http_server_) {
            std::cout << "Stopping HTTP server..." << std::endl;
            http_server_->Stop();
        }

#if defined(HAVE_GRPC) && defined(ENABLE_OTEL)
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

#ifdef HAVE_ARROW_FLIGHT
        // Shutdown Arrow Flight server
        if (arrow_server_) {
            std::cout << "Shutting down Arrow Flight server..." << std::endl;
            arrow_server_->Shutdown();
            if (arrow_server_thread_.joinable()) {
                arrow_server_thread_.join();
            }
            arrow_server_.reset();
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
    int http_port_;
    int arrow_port_;
    std::string data_dir_;
    std::shared_ptr<storage::Storage> storage_;
    std::unique_ptr<prometheus::HttpServer> http_server_;
    std::unique_ptr<server::SelfMonitor> self_monitor_;
    std::atomic<bool> shutdown_;
    
    // Prometheus components
    std::shared_ptr<prometheus::storage::TSDBAdapter> tsdb_adapter_;
    std::shared_ptr<prometheus::promql::Engine> engine_;
    std::unique_ptr<prometheus::api::QueryHandler> query_handler_;
    std::unique_ptr<prometheus::api::LabelsHandler> labels_handler_;
    
    // New Components
    std::shared_ptr<storage::RuleManager> rule_manager_;
    std::shared_ptr<storage::FilteringStorage> filtering_storage_;
    std::unique_ptr<storage::DerivedMetricManager> derived_metric_manager_;
    
#if defined(HAVE_GRPC) && defined(ENABLE_OTEL)
        std::unique_ptr<grpc::Server> grpc_server_;
        std::unique_ptr<otel::MetricsService> metrics_service_;
        std::unique_ptr<otel::QueryService> query_service_;
#endif

#ifdef HAVE_ARROW_FLIGHT
    std::unique_ptr<arrow::MetricsFlightServer> arrow_server_;
    std::thread arrow_server_thread_;
#endif
};

} // namespace tsdb

int main(int argc, char* argv[]) {
    // Set up signal handling
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    // Parse command-line arguments
    std::string address = "0.0.0.0:4317";  // Default OTEL gRPC port
    int http_port = 9090;                  // Default Prometheus HTTP port
    int arrow_port = 8815;                 // Default Arrow Flight port
    std::string data_dir = "/tmp/tsdb";
    bool enable_write_instrumentation = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--address" && i + 1 < argc) {
            address = argv[++i];
        } else if (arg == "--http-port" && i + 1 < argc) {
            http_port = std::stoi(argv[++i]);
        } else if (arg == "--arrow-port" && i + 1 < argc) {
            arrow_port = std::stoi(argv[++i]);
        } else if (arg == "--data-dir" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            std::string level_str = argv[++i];
            if (level_str == "debug") tsdb::common::Logger::SetLevel(spdlog::level::debug);
            else if (level_str == "info") tsdb::common::Logger::SetLevel(spdlog::level::info);
            else if (level_str == "warn") tsdb::common::Logger::SetLevel(spdlog::level::warn);
            else if (level_str == "error") tsdb::common::Logger::SetLevel(spdlog::level::err);
            else if (level_str == "off") tsdb::common::Logger::SetLevel(spdlog::level::off);
            else std::cerr << "Unknown log level: " << level_str << ". Using default (info)." << std::endl;
        } else if (arg == "--enable-write-instrumentation") {
            enable_write_instrumentation = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --address ADDRESS    gRPC Server address (default: 0.0.0.0:4317)" << std::endl;
            std::cout << "  --http-port PORT     HTTP Server port (default: 9090)" << std::endl;
            std::cout << "  --arrow-port PORT    Arrow Flight port (default: 8815, 0 to disable)" << std::endl;
            std::cout << "  --data-dir DIR       Data directory (default: /tmp/tsdb)" << std::endl;
            std::cout << "  --log-level LEVEL    Log level (debug, info, warn, error, off)" << std::endl;
            std::cout << "  --enable-write-instrumentation Enable detailed write performance metrics" << std::endl;
            std::cout << "  --help, -h           Show this help message" << std::endl;
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            std::cerr << "Use --help for usage information" << std::endl;
            return 1;
        }
    }
    
    try {
        if (enable_write_instrumentation) {
            tsdb::storage::StorageImpl::enable_write_instrumentation(true);
            std::cout << "Write performance instrumentation enabled" << std::endl;
        }

        tsdb::TSDBServer server(address, http_port, arrow_port, data_dir);
        
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
