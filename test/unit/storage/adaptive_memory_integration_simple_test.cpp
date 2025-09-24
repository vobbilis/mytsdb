#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/adaptive_memory_integration_working.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

// Simple test without complex setup
TEST(AdaptiveMemoryIntegrationSimpleTest, BasicAllocation) {
    // Create config
    core::StorageConfig config;
    config.cache_size_bytes = 1024 * 1024 * 1024; // 1GB
    config.block_size = 256 * 1024 * 1024; // 256MB
    
    // Create integration
    AdaptiveMemoryIntegration integration(config);
    
    // Initialize
    auto init_result = integration.initialize();
    if (!init_result.ok()) {
        FAIL() << "Initialization failed: " << init_result.error();
    }
    
    // Test allocation
    auto result = integration.allocate_optimized(256, 32);
    if (!result.ok()) {
        FAIL() << "Allocation failed: " << result.error();
    }
    
    void* ptr = result.value();
    if (ptr == nullptr) {
        FAIL() << "Allocated pointer is null";
    }
    
    // Test deallocation
    auto dealloc_result = integration.deallocate_optimized(ptr);
    if (!dealloc_result.ok()) {
        FAIL() << "Deallocation failed: " << dealloc_result.error();
    }
}

TEST(AdaptiveMemoryIntegrationSimpleTest, MultipleAllocations) {
    // Create config
    core::StorageConfig config;
    config.cache_size_bytes = 1024 * 1024 * 1024; // 1GB
    config.block_size = 256 * 1024 * 1024; // 256MB
    
    // Create integration
    AdaptiveMemoryIntegration integration(config);
    
    // Initialize
    auto init_result = integration.initialize();
    if (!init_result.ok()) {
        FAIL() << "Initialization failed: " << init_result.error();
    }
    
    // Test multiple allocations
    std::vector<void*> ptrs;
    
    for (int i = 0; i < 5; ++i) {
        auto result = integration.allocate_optimized(128, 16);
        if (!result.ok()) {
            FAIL() << "Allocation " << i << " failed: " << result.error();
        }
        ptrs.push_back(result.value());
    }
    
    // Deallocate all
    for (void* ptr : ptrs) {
        auto dealloc_result = integration.deallocate_optimized(ptr);
        if (!dealloc_result.ok()) {
            FAIL() << "Deallocation failed: " << dealloc_result.error();
        }
    }
}

} // namespace storage
} // namespace tsdb
