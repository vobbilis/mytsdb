#include <memory>
#include <string>
#include <iostream>
#include <csignal>
#include <thread>
#include <atomic>
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"

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
    explicit TSDBServer(const std::string& address) 
        : address_(address), shutdown_(false) {
        std::cout << "Creating TSDB server on address: " << address << std::endl;
    }

    bool Start() {
        // Create storage
        core::StorageConfig config;
        config.data_dir = "/tmp/tsdb";
        config.block_size = 1024 * 1024;  // 1MB
        config.enable_compression = true;
        
        storage_ = std::make_shared<storage::StorageImpl>();
        auto init_result = storage_->init(config);
        if (!init_result.ok()) {
            std::cerr << "Failed to initialize storage: " << init_result.error() << std::endl;
            return false;
        }

        std::cout << "TSDB server started successfully (without gRPC support)" << std::endl;
        return true;
    }

    void Stop() {
        shutdown_.store(true);
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
    std::shared_ptr<storage::StorageImpl> storage_;
    std::atomic<bool> shutdown_;
};

} // namespace tsdb

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Set up signal handling
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    try {
        tsdb::TSDBServer server("0.0.0.0:50051");
        
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