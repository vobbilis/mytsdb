#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "tsdb/storage/background_processor.h"

using namespace tsdb::storage;

int main() {
    std::cout << "Creating BackgroundProcessor..." << std::endl;
    
    BackgroundProcessorConfig config;
    config.num_workers = 2;
    config.max_queue_size = 100;
    config.task_timeout = std::chrono::milliseconds(1000);
    config.shutdown_timeout = std::chrono::milliseconds(2000);
    config.worker_wait_timeout = std::chrono::milliseconds(50);
    
    auto processor = std::make_unique<BackgroundProcessor>(config);
    std::cout << "BackgroundProcessor created" << std::endl;
    
    std::cout << "Calling initialize()..." << std::endl;
    auto result = processor->initialize();
    
    if (result.ok()) {
        std::cout << "Initialize succeeded!" << std::endl;
        std::cout << "Is healthy: " << processor->isHealthy() << std::endl;
        std::cout << "Queue size: " << processor->getQueueSize() << std::endl;
        
        std::cout << "Calling shutdown()..." << std::endl;
        processor->shutdown();
        std::cout << "Shutdown completed" << std::endl;
    } else {
        std::cout << "Initialize failed: " << result.error().what() << std::endl;
    }
    
    return 0;
}
