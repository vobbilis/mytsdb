#include <iostream>
#include "tsdb/storage/memory_optimization/adaptive_memory_integration.h"
#include "tsdb/core/config.h"

int main() {
    std::cout << "Creating config..." << std::endl;
    tsdb::core::StorageConfig config;
    config.cache_size_bytes = 1024 * 1024 * 1024;
    config.block_size = 256 * 1024 * 1024;
    
    std::cout << "Creating integration..." << std::endl;
    tsdb::storage::AdaptiveMemoryIntegration integration(config);
    
    std::cout << "Initializing..." << std::endl;
    auto init_result = integration.initialize();
    if (!init_result.ok()) {
        std::cout << "Init failed: " << init_result.error() << std::endl;
        return 1;
    }
    
    std::cout << "Allocating..." << std::endl;
    auto result = integration.allocate_optimized(256, 32);
    if (!result.ok()) {
        std::cout << "Allocation failed: " << result.error() << std::endl;
        return 1;
    }
    
    std::cout << "Allocation successful!" << std::endl;
    return 0;
}
