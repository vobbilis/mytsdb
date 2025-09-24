#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/adaptive_memory_integration_working.h"
#include "tsdb/core/types.h"
#include <chrono>
#include <thread>
#include <vector>

namespace tsdb {
namespace storage {

class AdaptiveMemoryIntegrationWorkingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize storage config
        config_.cache_size_bytes = 1024 * 1024 * 1024; // 1GB
        config_.block_size = 256 * 1024 * 1024; // 256MB
        
        // Initialize adaptive memory integration
        integration_ = std::make_unique<AdaptiveMemoryIntegration>(config_);
        
        // Initialize the integration
        auto result = integration_->initialize();
        ASSERT_TRUE(result.ok()) << "Failed to initialize adaptive memory integration: " << result.error();
    }
    
    void TearDown() override {
        integration_.reset();
    }
    
    std::unique_ptr<AdaptiveMemoryIntegration> integration_;
    core::StorageConfig config_;
};

TEST_F(AdaptiveMemoryIntegrationWorkingTest, BasicAllocation) {
    // Test basic memory allocation
    auto result = integration_->allocate_optimized(256, 32);
    ASSERT_TRUE(result.ok()) << "Allocation failed: " << result.error();
    
    void* ptr = result.value();
    ASSERT_NE(ptr, nullptr);
    
    // Test deallocation
    auto dealloc_result = integration_->deallocate_optimized(ptr);
    ASSERT_TRUE(dealloc_result.ok()) << "Deallocation failed: " << dealloc_result.error();
}

TEST_F(AdaptiveMemoryIntegrationWorkingTest, MultipleAllocations) {
    // Test multiple allocations
    std::vector<void*> ptrs;
    
    for (int i = 0; i < 10; ++i) {
        auto result = integration_->allocate_optimized(128, 16);
        ASSERT_TRUE(result.ok()) << "Allocation " << i << " failed: " << result.error();
        ptrs.push_back(result.value());
    }
    
    // Deallocate all
    for (void* ptr : ptrs) {
        auto dealloc_result = integration_->deallocate_optimized(ptr);
        ASSERT_TRUE(dealloc_result.ok()) << "Deallocation failed: " << dealloc_result.error();
    }
}

TEST_F(AdaptiveMemoryIntegrationWorkingTest, AccessPatternRecording) {
    // Test access pattern recording
    auto result = integration_->allocate_optimized(512, 32);
    ASSERT_TRUE(result.ok());
    
    void* ptr = result.value();
    
    // Record access pattern multiple times
    for (int i = 0; i < 10; ++i) {
        auto record_result = integration_->record_access_pattern(ptr);
        ASSERT_TRUE(record_result.ok()) << "Access pattern recording failed: " << record_result.error();
    }
    
    // Deallocate
    auto dealloc_result = integration_->deallocate_optimized(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

TEST_F(AdaptiveMemoryIntegrationWorkingTest, MemoryStatistics) {
    // Test memory statistics
    auto stats = integration_->get_memory_stats();
    ASSERT_FALSE(stats.empty());
    
    // Test access pattern statistics
    auto access_stats = integration_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(AdaptiveMemoryIntegrationWorkingTest, InvalidOperations) {
    // Test invalid operations
    auto result = integration_->deallocate_optimized(nullptr);
    ASSERT_FALSE(result.ok());
    
    auto record_result = integration_->record_access_pattern(nullptr);
    ASSERT_FALSE(record_result.ok());
    
    // Test deallocating invalid pointer
    void* invalid_ptr = reinterpret_cast<void*>(0x12345678);
    auto dealloc_result = integration_->deallocate_optimized(invalid_ptr);
    ASSERT_FALSE(dealloc_result.ok());
}

TEST_F(AdaptiveMemoryIntegrationWorkingTest, ConcurrentAccess) {
    // Test concurrent access
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 25; ++j) {
                auto result = integration_->allocate_optimized(128, 32);
                if (result.ok()) {
                    void* ptr = result.value();
                    
                    // Record access pattern
                    integration_->record_access_pattern(ptr);
                    
                    // Deallocate
                    auto dealloc_result = integration_->deallocate_optimized(ptr);
                    if (dealloc_result.ok()) {
                        success_count.fetch_add(1);
                    }
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all operations succeeded
    EXPECT_EQ(success_count.load(), 100); // 4 threads * 25 operations
}

TEST_F(AdaptiveMemoryIntegrationWorkingTest, AlignmentRequirements) {
    // Test different alignment requirements
    std::vector<size_t> alignments = {8, 16, 32, 64, 128};
    
    for (size_t alignment : alignments) {
        auto result = integration_->allocate_optimized(256, alignment);
        ASSERT_TRUE(result.ok()) << "Allocation failed for alignment " << alignment;
        
        void* ptr = result.value();
        ASSERT_NE(ptr, nullptr);
        
        // Verify alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        ASSERT_EQ(addr % alignment, 0) << "Pointer not aligned to " << alignment << " bytes";
        
        // Deallocate
        auto dealloc_result = integration_->deallocate_optimized(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(AdaptiveMemoryIntegrationWorkingTest, LargeAllocation) {
    // Test large allocation
    auto result = integration_->allocate_optimized(1024 * 1024, 64); // 1MB
    ASSERT_TRUE(result.ok()) << "Large allocation failed: " << result.error();
    
    void* ptr = result.value();
    ASSERT_NE(ptr, nullptr);
    
    // Deallocate
    auto dealloc_result = integration_->deallocate_optimized(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

} // namespace storage
} // namespace tsdb
