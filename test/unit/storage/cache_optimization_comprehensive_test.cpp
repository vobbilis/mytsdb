#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/cache_alignment_utils.h"
#include "tsdb/storage/memory_optimization/access_pattern_optimizer.h"
#include "tsdb/storage/memory_optimization/sequential_layout_optimizer.h"
#include "tsdb/storage/memory_optimization/adaptive_memory_integration.h"
#include "tsdb/storage/memory_optimization/tiered_memory_integration.h"
#include "tsdb/core/types.h"
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <algorithm>

namespace tsdb {
namespace storage {

class CacheOptimizationComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize storage config
        config_.max_memory_usage = 1024 * 1024 * 1024; // 1GB
        config_.cache_size = 256 * 1024 * 1024; // 256MB
        
        // Initialize all cache optimization components
        cache_utils_ = std::make_unique<CacheAlignmentUtils>(config_);
        access_optimizer_ = std::make_unique<AccessPatternOptimizer>(config_);
        layout_optimizer_ = std::make_unique<SequentialLayoutOptimizer>(config_);
        adaptive_integration_ = std::make_unique<AdaptiveMemoryIntegration>(config_);
        tiered_integration_ = std::make_unique<TieredMemoryIntegration>(config_);
        
        // Initialize all components
        auto cache_result = cache_utils_->initialize();
        auto access_result = access_optimizer_->initialize();
        auto layout_result = layout_optimizer_->initialize();
        auto adaptive_result = adaptive_integration_->initialize();
        auto tiered_result = tiered_integration_->initialize();
        
        ASSERT_TRUE(cache_result.ok()) << "Failed to initialize cache utils: " << cache_result.error();
        ASSERT_TRUE(access_result.ok()) << "Failed to initialize access optimizer: " << access_result.error();
        ASSERT_TRUE(layout_result.ok()) << "Failed to initialize layout optimizer: " << layout_result.error();
        ASSERT_TRUE(adaptive_result.ok()) << "Failed to initialize adaptive integration: " << adaptive_result.error();
        ASSERT_TRUE(tiered_result.ok()) << "Failed to initialize tiered integration: " << tiered_result.error();
    }
    
    void TearDown() override {
        cache_utils_.reset();
        access_optimizer_.reset();
        layout_optimizer_.reset();
        adaptive_integration_.reset();
        tiered_integration_.reset();
    }
    
    std::unique_ptr<CacheAlignmentUtils> cache_utils_;
    std::unique_ptr<AccessPatternOptimizer> access_optimizer_;
    std::unique_ptr<SequentialLayoutOptimizer> layout_optimizer_;
    std::unique_ptr<AdaptiveMemoryIntegration> adaptive_integration_;
    std::unique_ptr<TieredMemoryIntegration> tiered_integration_;
    core::StorageConfig config_;
};

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveInitialization) {
    // Test comprehensive initialization
    auto cache_stats = cache_utils_->get_cache_stats();
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    auto layout_stats = layout_optimizer_->get_optimization_stats();
    auto adaptive_stats = adaptive_integration_->get_memory_stats();
    auto tiered_stats = tiered_integration_->get_tiered_stats();
    
    ASSERT_FALSE(cache_stats.empty());
    ASSERT_FALSE(access_stats.empty());
    ASSERT_FALSE(layout_stats.empty());
    ASSERT_FALSE(adaptive_stats.empty());
    ASSERT_FALSE(tiered_stats.empty());
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveCacheAlignment) {
    // Test comprehensive cache alignment
    std::vector<size_t> alignments = {8, 16, 32, 64, 128, 256, 512};
    std::vector<size_t> sizes = {64, 128, 256, 512, 1024, 2048, 4096};
    
    for (size_t alignment : alignments) {
        for (size_t size : sizes) {
            auto result = cache_utils_->allocate_aligned(size, alignment);
            ASSERT_TRUE(result.ok()) << "Allocation failed for size " << size << " alignment " << alignment;
            
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
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveAccessPatternAnalysis) {
    // Test comprehensive access pattern analysis
    std::vector<core::SeriesID> series_ids;
    
    // Create multiple series with different access patterns
    for (int i = 0; i < 100; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
        
        // Record different access types
        std::string access_type = (i % 4 == 0) ? "sequential" : 
                                  (i % 4 == 1) ? "random" : 
                                  (i % 4 == 2) ? "mixed" : "burst";
        auto record_result = access_optimizer_->record_access(series_id, access_type);
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze all patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveDataLayoutOptimization) {
    // Test comprehensive data layout optimization
    std::vector<void*> data_ptrs;
    
    // Allocate multiple aligned blocks with different sizes
    for (int i = 0; i < 50; ++i) {
        size_t size = 64 + (i * 16); // Varying sizes
        auto result = cache_utils_->allocate_aligned(size, 64);
        ASSERT_TRUE(result.ok());
        data_ptrs.push_back(result.value());
    }
    
    // Optimize data layout
    auto optimize_result = cache_utils_->optimize_data_layout(data_ptrs);
    ASSERT_TRUE(optimize_result.ok());
    
    // Clean up
    for (void* ptr : data_ptrs) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensivePrefetchOptimization) {
    // Test comprehensive prefetch optimization
    std::vector<void*> prefetch_ptrs;
    
    // Allocate multiple blocks for prefetching
    for (int i = 0; i < 20; ++i) {
        auto result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(result.ok());
        prefetch_ptrs.push_back(result.value());
    }
    
    // Prefetch all blocks
    for (void* ptr : prefetch_ptrs) {
        auto prefetch_result = cache_utils_->prefetch_data(ptr, 256);
        ASSERT_TRUE(prefetch_result.ok());
    }
    
    // Clean up
    for (void* ptr : prefetch_ptrs) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveHotColdDataManagement) {
    // Test comprehensive hot/cold data management
    std::vector<void*> hot_ptrs, cold_ptrs;
    
    // Allocate hot data blocks
    for (int i = 0; i < 20; ++i) {
        auto result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(result.ok());
        hot_ptrs.push_back(result.value());
        
        // Promote to hot data
        auto promote_result = cache_utils_->promote_hot_data(result.value());
        ASSERT_TRUE(promote_result.ok());
    }
    
    // Allocate cold data blocks
    for (int i = 0; i < 20; ++i) {
        auto result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(result.ok());
        cold_ptrs.push_back(result.value());
        
        // Demote to cold data
        auto demote_result = cache_utils_->demote_cold_data(result.value());
        ASSERT_TRUE(demote_result.ok());
    }
    
    // Clean up
    for (void* ptr : hot_ptrs) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
    
    for (void* ptr : cold_ptrs) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveSequentialLayoutOptimization) {
    // Test comprehensive sequential layout optimization
    std::vector<core::TimeSeries> time_series;
    
    // Create multiple TimeSeries
    for (int i = 0; i < 50; ++i) {
        core::TimeSeries series;
        series.set_series_id(core::SeriesID("series_" + std::to_string(i)));
        time_series.push_back(series);
        
        // Optimize layout
        auto optimize_result = layout_optimizer_->optimize_time_series_layout(series);
        ASSERT_TRUE(optimize_result.ok());
    }
    
    // Verify statistics
    auto layout_stats = layout_optimizer_->get_optimization_stats();
    ASSERT_FALSE(layout_stats.empty());
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveConcurrentOperations) {
    // Test comprehensive concurrent operations
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads
    for (int i = 0; i < 16; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 100; ++j) {
                // Allocate aligned memory
                auto alloc_result = cache_utils_->allocate_aligned(128, 64);
                if (alloc_result.ok()) {
                    void* ptr = alloc_result.value();
                    
                    // Prefetch data
                    auto prefetch_result = cache_utils_->prefetch_data(ptr, 128);
                    if (prefetch_result.ok()) {
                        // Record access pattern
                        core::SeriesID series_id("series_" + std::to_string(i) + "_" + std::to_string(j));
                        auto record_result = access_optimizer_->record_access(series_id, "sequential");
                        if (record_result.ok()) {
                            // Optimize access pattern
                            auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
                            if (optimize_result.ok()) {
                                // Deallocate
                                auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
                                if (dealloc_result.ok()) {
                                    success_count.fetch_add(1);
                                }
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
    EXPECT_EQ(success_count.load(), 1600); // 16 threads * 100 operations
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensivePerformanceBenchmark) {
    // Test comprehensive performance benchmark
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform multiple complex operations
    for (int i = 0; i < 1000; ++i) {
        // Allocate aligned memory
        auto alloc_result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(alloc_result.ok());
        
        void* ptr = alloc_result.value();
        
        // Prefetch data
        auto prefetch_result = cache_utils_->prefetch_data(ptr, 256);
        ASSERT_TRUE(prefetch_result.ok());
        
        // Record access pattern
        core::SeriesID series_id("series_" + std::to_string(i));
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
        
        // Analyze access patterns
        auto analyze_result = access_optimizer_->analyze_access_patterns();
        ASSERT_TRUE(analyze_result.ok());
        
        // Optimize access pattern
        auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
        ASSERT_TRUE(optimize_result.ok());
        
        // Deallocate
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance target: Should complete 1000 operations in reasonable time
    EXPECT_LT(duration.count(), 200000); // 200ms for 1000 operations
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveErrorHandling) {
    // Test comprehensive error handling scenarios
    std::vector<void*> invalid_ptrs = {nullptr, reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x1000)};
    
    for (void* ptr : invalid_ptrs) {
        // Test prefetch with invalid pointers
        auto prefetch_result = cache_utils_->prefetch_data(ptr, 128);
        if (ptr == nullptr) {
            ASSERT_FALSE(prefetch_result.ok()) << "Prefetching null pointer should fail";
        }
        
        // Test promotion with invalid pointers
        auto promote_result = cache_utils_->promote_hot_data(ptr);
        if (ptr == nullptr) {
            ASSERT_FALSE(promote_result.ok()) << "Promoting null pointer should fail";
        }
        
        // Test demotion with invalid pointers
        auto demote_result = cache_utils_->demote_cold_data(ptr);
        if (ptr == nullptr) {
            ASSERT_FALSE(demote_result.ok()) << "Demoting null pointer should fail";
        }
    }
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveResourceManagement) {
    // Test comprehensive resource management
    std::vector<void*> pointers;
    std::vector<core::SeriesID> series_ids;
    
    // Allocate multiple aligned blocks
    for (int i = 0; i < 200; ++i) {
        auto result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(result.ok());
        pointers.push_back(result.value());
        
        // Create series
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
        
        // Record access pattern
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Check memory statistics
    auto memory_stats = cache_utils_->get_memory_stats();
    ASSERT_FALSE(memory_stats.empty());
    
    // Check access pattern statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
    
    // Clean up all resources
    for (void* ptr : pointers) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveIntegrationTest) {
    // Test comprehensive integration between all components
    std::vector<core::SeriesID> series_ids;
    std::vector<void*> pointers;
    
    // Create multiple series and allocations
    for (int i = 0; i < 50; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
        
        // Allocate aligned memory
        auto alloc_result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(alloc_result.ok());
        pointers.push_back(alloc_result.value());
        
        // Record access pattern
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
        
        // Prefetch data
        auto prefetch_result = cache_utils_->prefetch_data(alloc_result.value(), 256);
        ASSERT_TRUE(prefetch_result.ok());
    }
    
    // Analyze all access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Optimize all access patterns
    for (const auto& series_id : series_ids) {
        auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
        ASSERT_TRUE(optimize_result.ok());
    }
    
    // Check statistics
    auto cache_stats = cache_utils_->get_cache_stats();
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    auto layout_stats = layout_optimizer_->get_optimization_stats();
    auto adaptive_stats = adaptive_integration_->get_memory_stats();
    auto tiered_stats = tiered_integration_->get_tiered_stats();
    
    ASSERT_FALSE(cache_stats.empty());
    ASSERT_FALSE(access_stats.empty());
    ASSERT_FALSE(layout_stats.empty());
    ASSERT_FALSE(adaptive_stats.empty());
    ASSERT_FALSE(tiered_stats.empty());
    
    // Clean up all resources
    for (void* ptr : pointers) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
}

TEST_F(CacheOptimizationComprehensiveTest, ComprehensiveStressTest) {
    // Test comprehensive stress test
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads for stress testing
    for (int i = 0; i < 32; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 200; ++j) {
                // Allocate aligned memory
                auto alloc_result = cache_utils_->allocate_aligned(128, 64);
                if (alloc_result.ok()) {
                    void* ptr = alloc_result.value();
                    
                    // Prefetch data
                    auto prefetch_result = cache_utils_->prefetch_data(ptr, 128);
                    if (prefetch_result.ok()) {
                        // Record access pattern
                        core::SeriesID series_id("series_" + std::to_string(i) + "_" + std::to_string(j));
                        auto record_result = access_optimizer_->record_access(series_id, "sequential");
                        if (record_result.ok()) {
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
    EXPECT_EQ(success_count.load(), 6400); // 32 threads * 200 operations
}

} // namespace storage
} // namespace tsdb
