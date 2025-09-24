#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/access_pattern_optimizer.h"
#include "tsdb/storage/memory_optimization/sequential_layout_optimizer.h"
#include "tsdb/storage/memory_optimization/cache_alignment_utils.h"
#include "tsdb/core/types.h"
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <algorithm>

namespace tsdb {
namespace storage {

class AccessPatternOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize storage config
        config_.max_memory_usage = 1024 * 1024 * 1024; // 1GB
        config_.cache_size = 256 * 1024 * 1024; // 256MB
        
        // Initialize access pattern optimization components
        access_optimizer_ = std::make_unique<AccessPatternOptimizer>(config_);
        layout_optimizer_ = std::make_unique<SequentialLayoutOptimizer>(config_);
        cache_utils_ = std::make_unique<CacheAlignmentUtils>(config_);
        
        // Initialize all components
        auto access_result = access_optimizer_->initialize();
        auto layout_result = layout_optimizer_->initialize();
        auto cache_result = cache_utils_->initialize();
        
        ASSERT_TRUE(access_result.ok()) << "Failed to initialize access optimizer: " << access_result.error();
        ASSERT_TRUE(layout_result.ok()) << "Failed to initialize layout optimizer: " << layout_result.error();
        ASSERT_TRUE(cache_result.ok()) << "Failed to initialize cache utils: " << cache_result.error();
    }
    
    void TearDown() override {
        access_optimizer_.reset();
        layout_optimizer_.reset();
        cache_utils_.reset();
    }
    
    std::unique_ptr<AccessPatternOptimizer> access_optimizer_;
    std::unique_ptr<SequentialLayoutOptimizer> layout_optimizer_;
    std::unique_ptr<CacheAlignmentUtils> cache_utils_;
    core::StorageConfig config_;
};

TEST_F(AccessPatternOptimizationTest, BasicInitialization) {
    // Test basic initialization
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    auto layout_stats = layout_optimizer_->get_optimization_stats();
    auto cache_stats = cache_utils_->get_cache_stats();
    
    ASSERT_FALSE(access_stats.empty());
    ASSERT_FALSE(layout_stats.empty());
    ASSERT_FALSE(cache_stats.empty());
}

TEST_F(AccessPatternOptimizationTest, SequentialAccessPattern) {
    // Test sequential access pattern optimization
    core::SeriesID series_id("sequential_series");
    
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
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(AccessPatternOptimizationTest, RandomAccessPattern) {
    // Test random access pattern optimization
    core::SeriesID series_id("random_series");
    
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
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(AccessPatternOptimizationTest, MixedAccessPattern) {
    // Test mixed access pattern optimization
    core::SeriesID series_id("mixed_series");
    
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
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(AccessPatternOptimizationTest, BulkAccessPattern) {
    // Test bulk access pattern optimization
    std::vector<core::SeriesID> series_ids;
    
    // Create multiple series
    for (int i = 0; i < 20; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
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
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(AccessPatternOptimizationTest, PrefetchOptimization) {
    // Test prefetch optimization
    core::SeriesID series_id("prefetch_series");
    
    // Record access pattern
    auto record_result = access_optimizer_->record_access(series_id, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Get prefetch suggestions
    auto suggestions_result = access_optimizer_->suggest_prefetch_addresses(series_id);
    ASSERT_TRUE(suggestions_result.ok());
    
    auto suggestions = suggestions_result.value();
    ASSERT_FALSE(suggestions.empty());
    
    // Execute prefetch
    auto prefetch_result = access_optimizer_->execute_prefetch(suggestions);
    ASSERT_TRUE(prefetch_result.ok());
    
    // Verify statistics
    auto prefetch_stats = access_optimizer_->get_prefetch_stats();
    ASSERT_FALSE(prefetch_stats.empty());
}

TEST_F(AccessPatternOptimizationTest, SequentialLayoutOptimization) {
    // Test sequential layout optimization
    core::TimeSeries time_series;
    time_series.set_series_id(core::SeriesID("layout_series"));
    
    // Optimize TimeSeries layout
    auto layout_result = layout_optimizer_->optimize_time_series_layout(time_series);
    ASSERT_TRUE(layout_result.ok());
    
    // Reserve capacity
    auto reserve_result = layout_optimizer_->reserve_capacity(time_series, 1024);
    ASSERT_TRUE(reserve_result.ok());
    
    // Prefetch data
    auto prefetch_result = layout_optimizer_->prefetch_data(time_series);
    ASSERT_TRUE(prefetch_result.ok());
    
    // Verify statistics
    auto layout_stats = layout_optimizer_->get_optimization_stats();
    ASSERT_FALSE(layout_stats.empty());
}

TEST_F(AccessPatternOptimizationTest, CacheAlignmentOptimization) {
    // Test cache alignment optimization
    auto alloc_result = cache_utils_->allocate_aligned(512, 64);
    ASSERT_TRUE(alloc_result.ok());
    
    void* ptr = alloc_result.value();
    
    // Prefetch data
    auto prefetch_result = cache_utils_->prefetch_data(ptr, 512);
    ASSERT_TRUE(prefetch_result.ok());
    
    // Promote to hot data
    auto promote_result = cache_utils_->promote_hot_data(ptr);
    ASSERT_TRUE(promote_result.ok());
    
    // Optimize data layout
    std::vector<void*> data_ptrs = {ptr};
    auto optimize_result = cache_utils_->optimize_data_layout(data_ptrs);
    ASSERT_TRUE(optimize_result.ok());
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

TEST_F(AccessPatternOptimizationTest, ComprehensiveOptimization) {
    // Test comprehensive optimization
    core::SeriesID series_id("comprehensive_series");
    
    // Record access pattern
    auto record_result = access_optimizer_->record_access(series_id, "sequential");
    ASSERT_TRUE(record_result.ok());
    
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
    
    // Allocate aligned memory
    auto alloc_result = cache_utils_->allocate_aligned(256, 64);
    ASSERT_TRUE(alloc_result.ok());
    
    void* ptr = alloc_result.value();
    
    // Prefetch data
    auto prefetch_result = cache_utils_->prefetch_data(ptr, 256);
    ASSERT_TRUE(prefetch_result.ok());
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

TEST_F(AccessPatternOptimizationTest, ConcurrentOptimization) {
    // Test concurrent optimization
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 50; ++j) {
                core::SeriesID series_id("series_" + std::to_string(i) + "_" + std::to_string(j));
                
                // Record access pattern
                auto record_result = access_optimizer_->record_access(series_id, "sequential");
                if (record_result.ok()) {
                    // Analyze access patterns
                    auto analyze_result = access_optimizer_->analyze_access_patterns();
                    if (analyze_result.ok()) {
                        // Optimize access pattern
                        auto optimize_result = access_optimizer_->optimize_access_pattern(series_id);
                        if (optimize_result.ok()) {
                            success_count.fetch_add(1);
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
    EXPECT_EQ(success_count.load(), 400); // 8 threads * 50 operations
}

TEST_F(AccessPatternOptimizationTest, PerformanceBenchmark) {
    // Test performance benchmark
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform multiple optimization operations
    for (int i = 0; i < 1000; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        
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
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance target: Should complete 1000 operations in reasonable time
    EXPECT_LT(duration.count(), 100000); // 100ms for 1000 operations
}

TEST_F(AccessPatternOptimizationTest, ErrorHandling) {
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

TEST_F(AccessPatternOptimizationTest, ResourceManagement) {
    // Test resource management
    std::vector<core::SeriesID> series_ids;
    
    // Create multiple series
    for (int i = 0; i < 100; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
        
        // Record access pattern
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze all patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Check statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
    
    // Verify resources are properly managed
    ASSERT_TRUE(access_stats.find("Total Series") != std::string::npos);
    ASSERT_TRUE(access_stats.find("Sequential Series") != std::string::npos);
    ASSERT_TRUE(access_stats.find("Random Series") != std::string::npos);
}

TEST_F(AccessPatternOptimizationTest, IntegrationTest) {
    // Test integration between all components
    core::SeriesID series_id("integration_series");
    
    // Record access pattern
    auto record_result = access_optimizer_->record_access(series_id, "sequential");
    ASSERT_TRUE(record_result.ok());
    
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
    
    // Allocate aligned memory
    auto alloc_result = cache_utils_->allocate_aligned(256, 64);
    ASSERT_TRUE(alloc_result.ok());
    
    void* ptr = alloc_result.value();
    
    // Prefetch data
    auto prefetch_result = cache_utils_->prefetch_data(ptr, 256);
    ASSERT_TRUE(prefetch_result.ok());
    
    // Check statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    auto layout_stats = layout_optimizer_->get_optimization_stats();
    auto cache_stats = cache_utils_->get_cache_stats();
    
    ASSERT_FALSE(access_stats.empty());
    ASSERT_FALSE(layout_stats.empty());
    ASSERT_FALSE(cache_stats.empty());
    
    // Clean up
    auto dealloc_result = cache_utils_->deallocate_aligned(ptr);
    ASSERT_TRUE(dealloc_result.ok());
}

} // namespace storage
} // namespace tsdb
