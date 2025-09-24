#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/cache_alignment_utils.h"
#include "tsdb/storage/memory_optimization/access_pattern_optimizer.h"
#include "tsdb/core/types.h"
#include <chrono>
#include <thread>
#include <vector>

namespace tsdb {
namespace storage {

class CacheOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize storage config
        config_.max_memory_usage = 1024 * 1024 * 1024; // 1GB
        config_.cache_size = 256 * 1024 * 1024; // 256MB
        
        // Initialize cache optimization components
        cache_utils_ = std::make_unique<CacheAlignmentUtils>(config_);
        access_optimizer_ = std::make_unique<AccessPatternOptimizer>(config_);
        
        // Initialize all components
        auto cache_result = cache_utils_->initialize();
        auto access_result = access_optimizer_->initialize();
        
        ASSERT_TRUE(cache_result.ok()) << "Failed to initialize cache utils: " << cache_result.error();
        ASSERT_TRUE(access_result.ok()) << "Failed to initialize access optimizer: " << access_result.error();
    }
    
    void TearDown() override {
        cache_utils_.reset();
        access_optimizer_.reset();
    }
    
    std::unique_ptr<CacheAlignmentUtils> cache_utils_;
    std::unique_ptr<AccessPatternOptimizer> access_optimizer_;
    core::StorageConfig config_;
};

TEST_F(CacheOptimizationTest, BasicInitialization) {
    // Test basic initialization
    auto cache_stats = cache_utils_->get_cache_stats();
    ASSERT_FALSE(cache_stats.empty());
    
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(CacheOptimizationTest, CacheAlignmentOptimization) {
    // Test cache alignment optimization
    auto result = cache_utils_->allocate_aligned(1024, 64);
    ASSERT_TRUE(result.ok()) << "Aligned allocation failed: " << result.error();
    
    void* ptr = result.value();
    ASSERT_NE(ptr, nullptr);
    
    // Verify alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % 64, 0) << "Pointer not properly aligned to 64-byte boundary";
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

TEST_F(CacheOptimizationTest, AccessPatternOptimization) {
    // Test access pattern optimization
    core::SeriesID series_id("test_series");
    
    // Record access pattern
    auto record_result = access_optimizer_->record_access(series_id, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Optimize access pattern
    auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
    ASSERT_TRUE(optimize_result.ok());
}

TEST_F(CacheOptimizationTest, DataLayoutOptimization) {
    // Test data layout optimization
    std::vector<void*> data_ptrs;
    
    // Allocate multiple aligned blocks
    for (int i = 0; i < 10; ++i) {
        auto result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(result.ok());
        data_ptrs.push_back(result.value());
    }
    
    // Optimize data layout
    auto optimize_result = cache_utils_->optimize_data_layout(data_ptrs);
    ASSERT_TRUE(optimize_result.ok()) << "Data layout optimization failed: " << optimize_result.error();
    
    // Clean up
    for (void* ptr : data_ptrs) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(CacheOptimizationTest, PrefetchOptimization) {
    // Test prefetch optimization
    auto result = cache_utils_->allocate_aligned(512, 64);
    ASSERT_TRUE(result.ok());
    
    void* ptr = result.value();
    
    // Prefetch data
    auto prefetch_result = cache_utils_->prefetch_data(ptr, 512);
    ASSERT_TRUE(prefetch_result.ok()) << "Data prefetch failed: " << prefetch_result.error();
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

TEST_F(CacheOptimizationTest, HotColdDataManagement) {
    // Test hot/cold data management
    auto result = cache_utils_->allocate_aligned(256, 64);
    ASSERT_TRUE(result.ok());
    
    void* ptr = result.value();
    
    // Promote to hot data
    auto promote_result = cache_utils_->promote_hot_data(ptr);
    ASSERT_TRUE(promote_result.ok()) << "Hot data promotion failed: " << promote_result.error();
    
    // Demote to cold data
    auto demote_result = cache_utils_->demote_cold_data(ptr);
    ASSERT_TRUE(demote_result.ok()) << "Cold data demotion failed: " << demote_result.error();
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

TEST_F(CacheOptimizationTest, Statistics) {
    // Test statistics
    auto cache_stats = cache_utils_->get_cache_stats();
    ASSERT_FALSE(cache_stats.empty());
    
    auto memory_stats = cache_utils_->get_memory_stats();
    ASSERT_FALSE(memory_stats.empty());
    
    auto prefetch_stats = cache_utils_->get_prefetch_stats();
    ASSERT_FALSE(prefetch_stats.empty());
    
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
    
    auto optimization_stats = access_optimizer_->get_optimization_stats();
    ASSERT_FALSE(optimization_stats.empty());
}

TEST_F(CacheOptimizationTest, ConcurrentOperations) {
    // Test concurrent operations
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 25; ++j) {
                // Allocate aligned memory
                auto alloc_result = cache_utils_->allocate_aligned(128, 64);
                if (alloc_result.ok()) {
                    void* ptr = alloc_result.value();
                    
                    // Prefetch data
                    auto prefetch_result = cache_utils_->prefetch_data(ptr, 128);
                    if (prefetch_result.ok()) {
                        // Promote to hot data
                        auto promote_result = cache_utils_->promote_hot_data(ptr);
                        if (promote_result.ok()) {
                            // Deallocate
                            auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
                            if (dealloc_result.ok()) {
                                success_count.fetch_add(1);
                            }
                        }
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

TEST_F(CacheOptimizationTest, MultipleSeries) {
    // Test multiple series optimization
    std::vector<core::SeriesID> series_ids;
    
    // Create multiple series
    for (int i = 0; i < 20; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
        
        // Record access pattern
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze all patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(CacheOptimizationTest, DifferentAlignments) {
    // Test different alignment requirements
    std::vector<size_t> alignments = {8, 16, 32, 64, 128};
    
    for (size_t alignment : alignments) {
        auto result = cache_utils_->allocate_aligned(256, alignment);
        ASSERT_TRUE(result.ok()) << "Allocation with alignment " << alignment << " failed: " << result.error();
        
        void* ptr = result.value();
        ASSERT_NE(ptr, nullptr);
        
        // Verify alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(addr % alignment, 0) << "Pointer not properly aligned to " << alignment;
        
        // Clean up
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(CacheOptimizationTest, PerformanceBenchmark) {
    // Test performance benchmark
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform multiple operations
    for (int i = 0; i < 1000; ++i) {
        // Allocate aligned memory
        auto alloc_result = cache_utils_->allocate_aligned(128, 64);
        ASSERT_TRUE(alloc_result.ok());
        
        void* ptr = alloc_result.value();
        
        // Prefetch data
        auto prefetch_result = cache_utils_->prefetch_data(ptr, 128);
        ASSERT_TRUE(prefetch_result.ok());
        
        // Deallocate
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance target: Should complete 1000 operations in reasonable time
    EXPECT_LT(duration.count(), 50000); // 50ms for 1000 operations
}

TEST_F(CacheOptimizationTest, ErrorHandling) {
    // Test error handling
    auto prefetch_result = cache_utils_->prefetch_data(nullptr, 128);
    ASSERT_FALSE(prefetch_result.ok()) << "Prefetching null pointer should fail";
    
    auto promote_result = cache_utils_->promote_hot_data(nullptr);
    ASSERT_FALSE(promote_result.ok()) << "Promoting null pointer should fail";
    
    auto demote_result = cache_utils_->demote_cold_data(nullptr);
    ASSERT_FALSE(demote_result.ok()) << "Demoting null pointer should fail";
    
    auto dealloc_result = cache_utils_->deallocate_aligned(nullptr);
    ASSERT_FALSE(dealloc_result.ok()) << "Deallocating null pointer should fail";
}

TEST_F(CacheOptimizationTest, ResourceManagement) {
    // Test resource management
    std::vector<void*> pointers;
    
    // Allocate multiple aligned blocks
    for (int i = 0; i < 50; ++i) {
        auto result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(result.ok());
        pointers.push_back(result.value());
    }
    
    // Check memory statistics
    auto memory_stats = cache_utils_->get_memory_stats();
    ASSERT_FALSE(memory_stats.empty());
    
    // Clean up all resources
    for (void* ptr : pointers) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(CacheOptimizationTest, IntegrationTest) {
    // Test integration between cache utils and access optimizer
    core::SeriesID series_id("test_series");
    
    // Allocate aligned memory
    auto alloc_result = cache_utils_->allocate_aligned(512, 64);
    ASSERT_TRUE(alloc_result.ok());
    
    void* ptr = alloc_result.value();
    
    // Record access pattern
    auto record_result = access_optimizer_->record_access(series_id, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Prefetch data
    auto prefetch_result = cache_utils_->prefetch_data(ptr, 512);
    ASSERT_TRUE(prefetch_result.ok());
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Optimize access pattern
    auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
    ASSERT_TRUE(optimize_result.ok());
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

} // namespace storage
} // namespace tsdb
