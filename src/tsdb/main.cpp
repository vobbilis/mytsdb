#include <memory>
#include <string>
#include <grpcpp/server_builder.h>
#include <grpcpp/server.h>
#include <spdlog/spdlog.h>
#include "tsdb/otel/bridge.h"
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include <csignal>
#include <thread>
#include <atomic>

namespace {
std::atomic<bool> g_running{true};
tsdb::TSDBServer* g_server = nullptr;

void SignalHandler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    g_running.store(false);
    if (g_server) {
        g_server->Stop();
    }
}
}  // namespace

namespace tsdb {

class TSDBServer {
public:
    explicit TSDBServer(const std::string& address) 
        : address_(address), shutdown_(false) {}

    bool Start() {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
        
        // Create storage
        storage_ = storage::CreateStorage(storage::StorageOptions{
            .data_dir = "/tmp/tsdb",
            .max_block_size = 1024 * 1024,  // 1MB
            .block_duration = std::chrono::hours(2)
        });

        // Create and register metrics service
        metrics_service_ = std::make_unique<otel::MetricsService>(storage_);
        builder.RegisterService(metrics_service_.get());

        // Start server
        server_ = builder.BuildAndStart();
        if (!server_) {
            spdlog::error("Failed to start server");
            return false;
        }

        // Start background tasks
        StartBackgroundTasks();
        return true;
    }

    void RunCompactionTask() {
        while (!shutdown_.load() && g_running.load()) {
            auto result = storage_->compact();
            if (!result.ok()) {
                spdlog::error("Compaction failed: {}", result.error().what());
            }
            std::this_thread::sleep_for(std::chrono::minutes(30));
        }
    }

    void RunFlushTask() {
        while (!shutdown_.load() && g_running.load()) {
            auto result = storage_->flush();
            if (!result.ok()) {
                spdlog::error("Flush failed: {}", result.error().what());
            }
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }

    void StartBackgroundTasks() {
        compaction_thread_ = std::thread(&TSDBServer::RunCompactionTask, this);
        flush_thread_ = std::thread(&TSDBServer::RunFlushTask, this);
    }

    void Stop() {
        if (server_) {
            shutdown_.store(true);
            server_->Shutdown();
            if (compaction_thread_.joinable()) {
                compaction_thread_.join();
            }
            if (flush_thread_.joinable()) {
                flush_thread_.join();
            }
        }
    }

    void Wait() {
        if (server_) {
            // Wait for shutdown signal in a loop
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            Stop();
        }
    }

private:
    std::string address_;
    std::unique_ptr<grpc::Server> server_;
    std::shared_ptr<storage::Storage> storage_;
    std::unique_ptr<otel::MetricsService> metrics_service_;
    std::atomic<bool> shutdown_;
    std::thread compaction_thread_;
    std::thread flush_thread_;
};

} // namespace tsdb

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Set up signal handling
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    try {
        tsdb::TSDBServer server("0.0.0.0:50051");
        g_server = &server;
        
        if (!server.Start()) {
            return 1;
        }

        server.Wait();
        g_server = nullptr;
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
} 