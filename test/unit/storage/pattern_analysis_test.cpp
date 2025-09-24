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

class PatternAnalysisTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize storage config
        config_.max_memory_usage = 1024 * 1024 * 1024; // 1GB
        config_.cache_size = 256 * 1024 * 1024; // 256MB
        
        // Initialize pattern analysis components
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

TEST_F(PatternAnalysisTest, BasicPatternAnalysis) {
    // Test basic pattern analysis
    core::SeriesID series_id("test_series");
    
    // Record access pattern
    auto record_result = access_optimizer_->record_access(series_id, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(PatternAnalysisTest, SequentialPatternAnalysis) {
    // Test sequential pattern analysis
    core::SeriesID series_id("sequential_series");
    
    // Record sequential access pattern
    for (int i = 0; i < 100; ++i) {
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
    
    // Verify sequential pattern is detected
    ASSERT_TRUE(access_stats.find("Sequential Series") != std::string::npos);
}

TEST_F(PatternAnalysisTest, RandomPatternAnalysis) {
    // Test random pattern analysis
    core::SeriesID series_id("random_series");
    
    // Record random access pattern
    for (int i = 0; i < 100; ++i) {
        auto record_result = access_optimizer_->record_access(series_id, "random");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
    
    // Verify random pattern is detected
    ASSERT_TRUE(access_stats.find("Random Series") != std::string::npos);
}

TEST_F(PatternAnalysisTest, MixedPatternAnalysis) {
    // Test mixed pattern analysis
    core::SeriesID series_id("mixed_series");
    
    // Record mixed access pattern
    std::vector<std::string> access_types = {"sequential", "random", "mixed", "burst"};
    for (int i = 0; i < 200; ++i) {
        std::string access_type = access_types[i % access_types.size()];
        auto record_result = access_optimizer_->record_access(series_id, access_type);
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(PatternAnalysisTest, BulkPatternAnalysis) {
    // Test bulk pattern analysis
    std::vector<core::SeriesID> series_ids;
    
    // Create multiple series
    for (int i = 0; i < 50; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        series_ids.push_back(series_id);
    }
    
    // Record bulk access pattern
    auto record_result = access_optimizer_->record_bulk_access(series_ids, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(PatternAnalysisTest, TemporalPatternAnalysis) {
    // Test temporal pattern analysis
    core::SeriesID series_id("temporal_series");
    
    // Record access pattern over time
    for (int i = 0; i < 100; ++i) {
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
        
        // Simulate time passage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(PatternAnalysisTest, SpatialPatternAnalysis) {
    // Test spatial pattern analysis
    core::SeriesID series_id("spatial_series");
    
    // Record spatial access pattern
    for (int i = 0; i < 100; ++i) {
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(PatternAnalysisTest, BurstPatternAnalysis) {
    // Test burst pattern analysis
    core::SeriesID series_id("burst_series");
    
    // Record burst access pattern
    for (int i = 0; i < 100; ++i) {
        auto record_result = access_optimizer_->record_access(series_id, "burst");
        ASSERT_TRUE(record_result.ok());
    }
    
    // Analyze access patterns
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Verify statistics
    auto access_stats = access_optimizer_->get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
}

TEST_F(PatternAnalysisTest, ConcurrentPatternAnalysis) {
    // Test concurrent pattern analysis
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 25; ++j) {
                core::SeriesID series_id("series_" + std::to_string(i) + "_" + std::to_string(j));
                
                // Record access pattern
                auto record_result = access_optimizer_->record_access(series_id, "sequential");
                if (record_result.ok()) {
                    // Analyze access patterns
                    auto analyze_result = access_optimizer_->analyze_access_patterns();
                    if (analyze_result.ok()) {
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
    EXPECT_EQ(success_count.load(), 200); // 8 threads * 25 operations
}

TEST_F(PatternAnalysisTest, PerformanceBenchmark) {
    // Test performance benchmark
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform multiple pattern analysis operations
    for (int i = 0; i < 1000; ++i) {
        core::SeriesID series_id("series_" + std::to_string(i));
        
        // Record access pattern
        auto record_result = access_optimizer_->record_access(series_id, "sequential");
        ASSERT_TRUE(record_result.ok());
        
        // Analyze access patterns
        auto analyze_result = access_optimizer_->analyze_access_patterns();
        ASSERT_TRUE(analyze_result.ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance target: Should complete 1000 operations in reasonable time
    EXPECT_LT(duration.count(), 100000); // 100ms for 1000 operations
}

TEST_F(PatternAnalysisTest, ErrorHandling) {
    // Test error handling
    core::SeriesID invalid_series("");
    
    // These operations should handle invalid input gracefully
    auto record_result = access_optimizer_->record_access(invalid_series, "sequential");
    // Should not crash, even if it fails
    
    auto analyze_result = access_optimizer_->analyze_access_patterns();
    // Should handle gracefully
}

TEST_F(PatternAnalysisTest, ResourceManagement) {
    // Test resource management
    std::vector<core::SeriesID> series_ids;
    
    // Create multiple series
    for (int i = 0; i < 200; ++i) {
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

TEST_F(PatternAnalysisTest, IntegrationTest) {
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

TEST_F(PatternAnalysisTest, StressTest) {
    // Test stress test
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Create multiple threads for stress testing
    for (int i = 0; i < 16; ++i) {
        threads.emplace_back([this, &success_count, i]() {
            for (int j = 0; j < 100; ++j) {
                core::SeriesID series_id("series_" + std::to_string(i) + "_" + std::to_string(j));
                
                // Record access pattern
                auto record_result = access_optimizer_->record_access(series_id, "sequential");
                if (record_result.ok()) {
                    // Analyze access patterns
                    auto analyze_result = access_optimizer_->analyze_access_patterns();
                    if (analyze_result.ok()) {
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
    EXPECT_EQ(success_count.load(), 1600); // 16 threads * 100 operations
}

} // namespace storage
} // namespace tsdb
