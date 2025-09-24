#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/access_pattern_optimizer.h"
#include "tsdb/storage/memory_optimization/sequential_layout_optimizer.h"
#include "tsdb/storage/memory_optimization/cache_alignment_utils.h"
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

class AccessPatternIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize storage config
        config_.max_memory_usage = 1024 * 1024 * 1024; // 1GB
        config_.cache_size = 256 * 1024 * 1024; // 256MB
        
        // Initialize all access pattern integration components
        access_optimizer_ = std::make_unique<AccessPatternOptimizer>(config_);
        layout_optimizer_ = std::make_unique<SequentialLayoutOptimizer>(config_);
        cache_utils_ = std::make_unique<CacheAlignmentUtils>(config_);
        adaptive_integration_ = std::make_unique<AdaptiveMemoryIntegration>(config_);
        tiered_integration_ = std::make_unique<TieredMemoryIntegration>(config_);
        
        // Initialize all components
        auto access_result = access_optimizer_->initialize();
        auto layout_result = layout_optimizer_->initialize();
        auto cache_result = cache_utils_->initialize();
        auto adaptive_result = adaptive_integration_->initialize();
        auto tiered_result = tiered_integration_->initialize();
        
        ASSERT_TRUE(access_result.ok()) << "Failed to initialize access optimizer: " << access_result.error();
        ASSERT_TRUE(layout_result.ok()) << "Failed to initialize layout optimizer: " << layout_result.error();
        ASSERT_TRUE(cache_result.ok()) << "Failed to initialize cache utils: " << cache_result.error();
        ASSERT_TRUE(adaptive_result.ok()) << "Failed to initialize adaptive integration: " << adaptive_result.error();
        ASSERT_TRUE(tiered_result.ok()) << "Failed to initialize tiered integration: " << tiered_result.error();
    }
    
    void TearDown() override {
        access_optimizer_.reset();
        layout_optimizer_.reset();
        cache_utils_.reset();
        adaptive_integration_.reset();
        tiered_integration_.reset();
    }
    
    std::unique_ptr<AccessPatternOptimizer> access_optimizer_;
    std::unique_ptr<SequentialLayoutOptimizer> layout_optimizer_;
    std::unique_ptr<CacheAlignmentUtils> cache_utils_;
    std::unique_ptr<AdaptiveMemoryIntegration> adaptive_integration_;
    std::unique_ptr<TieredMemoryIntegration> tiered_integration_;
    core::StorageConfig config_;
};

TEST_F(AccessPatternIntegrationTest, BasicIntegration) {
    // Test basic integration between all components
    core::SeriesID series_id("integration_series");
    
    // Add series to tiered memory
    auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::RAM);
    ASSERT_TRUE(add_result.ok());
    
    // Allocate memory using adaptive memory pool
    auto alloc_result = adaptive_integration_->allocate_optimized(1024, 64);
    ASSERT_TRUE(alloc_result.ok());
    
    void* ptr = alloc_result.value();
    
    // Record access pattern
    auto record_result = adaptive_integration_->record_access_pattern(ptr);
    ASSERT_TRUE(record_result.ok());
    
    // Record access pattern in optimizer
    auto access_record_result = access_optimizer_->record_access(series_id, "sequential");
    ASSERT_TRUE(access_record_result.ok());
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Optimize access pattern
    auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
    ASSERT_TRUE(optimize_result.ok());
    
    // Optimize memory layout
    auto layout_optimize_result = adaptive_integration_->optimize_memory_layout();
    ASSERT_TRUE(layout_optimize_result.ok());
    
    // Clean up
    auto dealloc_result = adaptive_integration_->deallocate_optimized(ptr);
    ASSERT_TRUE(dealloc_result.ok());
    
    auto remove_result = tiered_integration_->remove_series(series_id);
    ASSERT_TRUE(remove_result.ok());
}

TEST_F(AccessPatternIntegrationTest, SequentialAccessIntegration) {
    // Test sequential access pattern integration
    core::SeriesID series_id("sequential_series");
    
    // Add series to tiered memory
    auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::SSD);
    ASSERT_TRUE(add_result.ok());
    
    // Allocate aligned memory
    auto alloc_result = cache_utils_->allocate_aligned(512, 64);
    ASSERT_TRUE(alloc_result.ok());
    
    void* ptr = alloc_result.value();
    
    // Record sequential access pattern
    for (int i = 0; i < 50; ++i) {
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Optimize access pattern
    auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
    ASSERT_TRUE(optimize_result.ok());
    
    // Promote series to faster tier
    auto promote_result = tiered_integration_->promote_series(series_id);
    ASSERT_TRUE(promote_result.ok());
    
    // Prefetch data
    auto prefetch_result = cache_utils_->prefetch_data(ptr, 512);
    ASSERT_TRUE(prefetch_result.ok());
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
    
    auto remove_result = tiered_integration_->remove_series(series_id);
    ASSERT_TRUE(remove_result.ok());
}

TEST_F(AccessPatternIntegrationTest, RandomAccessIntegration) {
    // Test random access pattern integration
    core::SeriesID series_id("random_series");
    
    // Add series to tiered memory
    auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::RAM);
    ASSERT_TRUE(add_result.ok());
    
    // Allocate aligned memory
    auto alloc_result = cache_utils_->allocate_aligned(256, 64);
    ASSERT_TRUE(alloc_result.ok());
    
    void* ptr = alloc_result.value();
    
    // Record random access pattern
    for (int i = 0; i < 50; ++i) {
        auto record_result = access_optimizer_->record_access(series_id, "random");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Optimize access pattern
    auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
    ASSERT_TRUE(optimize_result.ok());
    
    // Demote series to slower tier
    auto demote_result = tiered_integration_->demote_series(series_id);
    ASSERT_TRUE(demote_result.ok());
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
    
    auto remove_result = tiered_integration_->remove_series(series_id);
    ASSERT_TRUE(remove_result.ok());
}

TEST_F(AccessPatternIntegrationTest, MixedAccessIntegration) {
    // Test mixed access pattern integration
    core::SeriesID series_id("mixed_series");
    
    // Add series to tiered memory
    auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::SSD);
    ASSERT_TRUE(add_result.ok());
    
    // Allocate aligned memory
    auto alloc_result = cache_utils_->allocate_aligned(384, 64);
    ASSERT_TRUE(alloc_result.ok());
    
    void* ptr = alloc_result.value();
    
    // Record mixed access pattern
    std::vector<std::string> access_types = {"sequential", "random", "mixed", "burst"};
    for (int i = 0; i < 100; ++i) {
        std::string access_type = access_types[i % access_types.size()];
        auto record_result = access_optimizer_->record_access(series_id, access_type);
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Optimize access pattern
    auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
    ASSERT_TRUE(optimize_result.ok());
    
    // Optimize TimeSeries layout
    core::TimeSeries time_series;
    time_series.set_series_id(series_id);
    auto layout_result = layout_optimizer_->optimize_time_series_layout(time_series);
    ASSERT_TRUE(layout_result.ok());
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
    
    auto remove_result = tiered_integration_->remove_series(series_id);
    ASSERT_TRUE(remove_result.ok());
}

TEST_F(AccessPatternIntegrationTest, BulkAccessIntegration) {
    // Test bulk access pattern integration
    std::vector<core::SeriesID> series_ids;
    
    // Create multiple series
    for (int i = 0; i < 20; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
        
        // Add series to tiered memory
        auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::SSD);
        ASSERT_TRUE(add_result.ok());
    }
    
    // Record bulk access pattern
    auto record_result = access_optimizer_->record_bulk_access(series_ids, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Optimize all access patterns
    for (const auto& series_id : series_ids) {
        auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
        ASSERT_TRUE(optimize_result.ok());
    }
    
    // Clean up
    for (const auto& series_id : series_ids) {
        auto remove_result = tiered_integration_->remove_series(series_id);
        ASSERT_TRUE(remove_result.ok());
    }
}

TEST_F(AccessPatternIntegrationTest, ConcurrentIntegration) {
    // Test concurrent access pattern integration
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 25; ++j) {
                core::SeriesID series_id("series_" + std::to_string(i) + "_" + std::to_string(j));
                
                // Add series to tiered memory
                auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::SSD);
                if (add_result.ok()) {
                    // Allocate aligned memory
                    auto alloc_result = cache_utils_->allocate_aligned(128, 64);
                    if (alloc_result.ok()) {
                        void* ptr = alloc_result.value();
                        
                        // Record access pattern
                        auto record_result = access_optimizer_->record_access(series_id, "sequential");
                        if (record_result.ok()) {
                            // Analyze access patterns
                            auto analyze_result = access_optimizer_->analyze_access_patterns();
                            if (analyze_result.ok()) {
                                // Optimize access pattern
                                auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
                                if (optimize_result.ok()) {
                                    // Clean up
                                    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
                                    auto remove_result = tiered_integration_->remove_series(series_id);
                                    if (dealloc_result.ok() && remove_result.ok()) {
                                        success_count.fetch_add(1);
                                    }
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
    EXPECT_EQ(success_count.load(), 200); // 8 threads * 25 operations
}

TEST_F(AccessPatternIntegrationTest, PerformanceBenchmark) {
    // Test performance benchmark
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform multiple integration operations
    for (int i = 0; i < 500; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        
        // Add series to tiered memory
        auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::SSD);
        ASSERT_TRUE(add_result.ok());
        
        // Allocate aligned memory
        auto alloc_result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(alloc_result.ok());
        
        void* ptr = alloc_result.value();
        
        // Record access pattern
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
        
        // Analyze access patterns
        auto analyze_result = access_optimizer_->analyze_access_patterns();
        ASSERT_TRUE(analyze_result.ok());
        
        // Optimize access pattern
        auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
        ASSERT_TRUE(optimize_result.ok());
        
        // Clean up
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
        
        auto remove_result = tiered_integration_->remove_series(series_id);
        ASSERT_TRUE(remove_result.ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance target: Should complete 500 operations in reasonable time
    EXPECT_LT(duration.count(), 200000); // 200ms for 500 operations
}

TEST_F(AccessPatternIntegrationTest, ErrorHandling) {
    // Test error handling
    core::SeriesID invalid_series("");
    
    // These operations should handle invalid input gracefully
    auto record_result = access_optimizer_->record_access(invalid_series, "sequential");
    // Should not crash, even if it fails
    
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    // Should handle gracefully
    
    auto optimize_result = access_optimizer_->optimize_access_pattern(invalid_series);
    // Should handle gracefully
}

TEST_F(AccessPatternIntegrationTest, ResourceManagement) {
    // Test resource management
    std::vector<core::SeriesID> series_ids;
    std::vector<void*> pointers;
    
    // Create multiple series and allocations
    for (int i = 0; i < 100; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
        
        // Add series to tiered memory
        auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::SSD);
        ASSERT_TRUE(add_result.ok());
        
        // Allocate aligned memory
        auto alloc_result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(alloc_result.ok());
        pointers.push_back(alloc_result.value());
        
        // Record access pattern
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze all access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Check statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    auto tiered_stats = tiered_integration_->get_tiered_stats();
    auto cache_stats = cache_utils_->get_cache_stats();
    
    ASSERT_FALSE(access_stats.empty());
    ASSERT_FALSE(tiered_stats.empty());
    ASSERT_FALSE(cache_stats.empty());
    
    // Clean up all resources
    for (void* ptr : pointers) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
    
    for (const auto& series_id : series_ids) {
        auto remove_result = tiered_integration_->remove_series(series_id);
        ASSERT_TRUE(remove_result.ok());
    }
}

TEST_F(AccessPatternIntegrationTest, ComprehensiveIntegration) {
    // Test comprehensive integration between all components
    std::vector<core::SeriesID> series_ids;
    std::vector<void*> pointers;
    
    // Create multiple series and allocations
    for (int i = 0; i < 50; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
        
        // Add series to tiered memory
        auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::SSD);
        ASSERT_TRUE(add_result.ok());
        
        // Allocate aligned memory
        auto alloc_result = cache_utils_->allocate_aligned(256, 64);
        ASSERT_TRUE(alloc_result.ok());
        pointers.push_back(alloc_result.value());
        
        // Record access pattern
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
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
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    auto layout_stats = layout_optimizer_->get_optimization_stats();
    auto cache_stats = cache_utils_->get_cache_stats();
    auto adaptive_stats = adaptive_integration_->get_memory_stats();
    auto tiered_stats = tiered_integration_->get_tiered_stats();
    
    ASSERT_FALSE(access_stats.empty());
    ASSERT_FALSE(layout_stats.empty());
    ASSERT_FALSE(cache_stats.empty());
    ASSERT_FALSE(adaptive_stats.empty());
    ASSERT_FALSE(tiered_stats.empty());
    
    // Clean up all resources
    for (void* ptr : pointers) {
        auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
        ASSERT_TRUE(dealloc_result.ok());
    }
    
    for (const auto& series_id : series_ids) {
        auto remove_result = tiered_integration_->remove_series(series_id);
        ASSERT_TRUE(remove_result.ok());
    }
}

TEST_F(AccessPatternIntegrationTest, StressTest) {
    // Test stress test
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads for stress testing
    for (int i = 0; i < 16; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 100; ++j) {
                core::SeriesID series_id("series_" + std::to_string(i) + "_" + std::to_string(j));
                
                // Add series to tiered memory
                auto add_result = tiered_integration_->add_series(series_id, core::semantic_vector::MemoryTier::SSD);
                if (add_result.ok()) {
                    // Allocate aligned memory
                    auto alloc_result = cache_utils_->allocate_aligned(128, 64);
                    if (alloc_result.ok()) {
                        void* ptr = alloc_result.value();
                        
                        // Record access pattern
                        auto record_result = access_optimizer_->record_access(series_id, "sequential");
                        if (record_result.ok()) {
                            // Analyze access patterns
                            auto analyze_result = access_optimizer_->analyze_access_patterns();
                            if (analyze_result.ok()) {
                                // Clean up
                                auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
                                auto remove_result = tiered_integration_->remove_series(series_id);
                                if (dealloc_result.ok() && remove_result.ok()) {
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

} // namespace storage
} // namespace tsdb
