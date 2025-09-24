#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/adaptive_memory_integration.h"
#include "tsdb/core/types.h"
#include <chrono>
#include <thread>
#include <vector>

namespace tsdb {
namespace storage {

class AdaptiveMemoryIntegrationTest : public ::testing::Test {
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

TEST_F(AdaptiveMemoryIntegrationTest, BasicAllocation) {
    // Test basic memory allocation with smaller size and different alignment
    auto result = integration_->allocate_optimized(256, 16);
    ASSERT_TRUE(result.ok()) << "Allocation failed: " << result.error();
    
    void* ptr = result.value();
    ASSERT_NE(ptr, nullptr);
    
    // Test deallocation
    auto dealloc_result = integration_->deallocate_optimized(ptr);
    ASSERT_TRUE(dealloc_result.ok()) << "Deallocation failed: " << dealloc_result.error();
}

TEST_F(AdaptiveMemoryIntegrationTest, AccessPatternRecording) {
    // Test access pattern recording
    auto result = integration_->allocate_optimized(512, 32);
    ASSERT_TRUE(result.ok());
    
    void* ptr = result.value();
    
    // Record multiple accesses
    for (int i = 0; i < 20; ++i) {
        auto record_result = integration_->record_access_pattern(ptr);
        ASSERT_TRUE(record_result.ok()) << "Access pattern recording failed: " << record_result.error();
    }
    
    // Clean up
    auto dealloc_result = integration_->deallocate_optimized(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

TEST_F(AdaptiveMemoryIntegrationTest, MemoryLayoutOptimization) {
    // Test memory layout optimization
    std::vector<void*> pointers;
    
    // Allocate multiple blocks
    for (int i = 0; i < 10; ++i) {
        auto result = integration_->allocate_optimized(256, 32);
        ASSERT_TRUE(result.ok());
        
        void* ptr = result.value();
        pointers.push_back(ptr);
        
        // Record access patterns
        for (int j = 0; j < 5; ++j) {
            auto record_result = integration_->record_access_pattern(ptr);
            ASSERT_TRUE(record_result.ok());
        }
    }
    
    // Optimize memory layout
    auto optimize_result = integration_->optimize_memory_layout();
    ASSERT_TRUE(optimize_result.ok()) << "Memory layout optimization failed: " << optimize_result.error();
    
    // Clean up
    for (void* ptr : pointers) {
        auto dealloc_result = integration_->deallocate_optimized(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(AdaptiveMemoryIntegrationTest, HotDataPromotion) {
    // Test hot data promotion
    core::SeriesID series_id = 12345;
    
    auto promote_result = integration_->promote_hot_data(series_id);
    ASSERT_TRUE(promote_result.ok()) << "Hot data promotion failed: " << promote_result.error();
}

TEST_F(AdaptiveMemoryIntegrationTest, ColdDataDemotion) {
    // Test cold data demotion
    core::SeriesID series_id = 12345;
    
    auto demote_result = integration_->demote_cold_data(series_id);
    ASSERT_TRUE(demote_result.ok()) << "Cold data demotion failed: " << demote_result.error();
}

TEST_F(AdaptiveMemoryIntegrationTest, MemoryStatistics) {
    // Test memory statistics
    auto stats = integration_->get_memory_stats();
    ASSERT_FALSE(stats.empty());
    
    // Test access pattern statistics
    auto access_stats = integration_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
    
    // Test tiered memory statistics
    auto tiered_stats = integration_->get_tiered_memory_stats();
    ASSERT_FALSE(tiered_stats.empty());
}

TEST_F(AdaptiveMemoryIntegrationTest, ConcurrentAccess) {
    // Test concurrent access to adaptive memory integration
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 50; ++j) {
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
    EXPECT_EQ(success_count.load(), 200); // 4 threads * 50 operations
}

TEST_F(AdaptiveMemoryIntegrationTest, AlignmentRequirements) {
    // Test different alignment requirements
    std::vector<size_t> alignments = {8, 16, 32, 64, 128};
    
    for (size_t alignment : alignments) {
        auto result = integration_->allocate_optimized(256, alignment);
        ASSERT_TRUE(result.ok()) << "Allocation with alignment " << alignment << " failed: " << result.error();
        
        void* ptr = result.value();
        ASSERT_NE(ptr, nullptr);
        
        // Verify alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        ASSERT_EQ(addr % alignment, 0) << "Pointer not properly aligned to " << alignment;
        
        // Clean up
        auto dealloc_result = integration_->deallocate_optimized(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(AdaptiveMemoryIntegrationTest, LargeAllocation) {
    // Test large allocation
    size_t large_size = 1024 * 1024; // 1MB
    
    auto result = integration_->allocate_optimized(large_size, 64);
    ASSERT_TRUE(result.ok()) << "Large allocation failed: " << result.error();
    
    void* ptr = result.value();
    ASSERT_NE(ptr, nullptr);
    
    // Clean up
    auto dealloc_result = integration_->deallocate_optimized(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

TEST_F(AdaptiveMemoryIntegrationTest, InvalidOperations) {
    // Test invalid operations
    auto result = integration_->allocate_optimized(0, 64);
    ASSERT_FALSE(result.ok()) << "Zero-size allocation should fail";
    
    auto dealloc_result = integration_->deallocate_optimized(nullptr);
    ASSERT_FALSE(dealloc_result.ok()) << "Null pointer deallocation should fail";
    
    auto record_result = integration_->record_access_pattern(nullptr);
    ASSERT_FALSE(record_result.ok()) << "Null pointer access pattern recording should fail";
}

} // namespace storage
} // namespace tsdb
