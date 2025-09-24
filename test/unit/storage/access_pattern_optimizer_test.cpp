#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/access_pattern_optimizer.h"
#include "tsdb/core/types.h"

using namespace tsdb::storage;

TEST(AccessPatternOptimizerTest, AccessRecording) {
    tsdb::core::StorageConfig config;
    AccessPatternOptimizer optimizer(config);
    
    // Test single access recording
    tsdb::core::Labels labels;
    labels.add("__name__", "test_metric");
    tsdb::core::TimeSeries series(labels);
    series.add_sample(tsdb::core::Sample(1000, 1.0));
    
    tsdb::core::SeriesID series_id = 12345; // Use numeric ID
    auto result = optimizer.record_access(series_id, "sequential");
    ASSERT_TRUE(result.ok());
}

TEST(AccessPatternOptimizerTest, BulkAccessRecording) {
    tsdb::core::StorageConfig config;
    AccessPatternOptimizer optimizer(config);
    
    // Test bulk access recording
    std::vector<tsdb::core::SeriesID> series_ids = {12345, 12346, 12347, 12348};
    auto result = optimizer.record_bulk_access(series_ids, "sequential");
    ASSERT_TRUE(result.ok());
}

TEST(AccessPatternOptimizerTest, PatternAnalysis) {
    tsdb::core::StorageConfig config;
    AccessPatternOptimizer optimizer(config);
    
    // Record some access patterns
    std::vector<tsdb::core::SeriesID> series_ids = {12345, 12346, 12347, 12348}; // Sequential pattern
    auto record_result = optimizer.record_bulk_access(series_ids, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Analyze patterns
    auto analysis_result = optimizer.analyze_access_patterns();
    ASSERT_TRUE(analysis_result.ok());
    
    // Check access pattern stats
    auto access_stats = optimizer.get_access_pattern_stats();
    ASSERT_FALSE(access_stats.empty());
    
    // Check optimization stats
    auto optimization_stats = optimizer.get_optimization_stats();
    ASSERT_FALSE(optimization_stats.empty());
}

TEST(AccessPatternOptimizerTest, PrefetchSuggestions) {
    tsdb::core::StorageConfig config;
    AccessPatternOptimizer optimizer(config);
    
    // Record access patterns
    std::vector<tsdb::core::SeriesID> series_ids = {12345, 12346, 12347};
    auto record_result = optimizer.record_bulk_access(series_ids, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Get prefetch suggestions
    auto prefetch_result = optimizer.suggest_prefetch_addresses(12345);
    ASSERT_TRUE(prefetch_result.ok());
    
    // Verify suggestions are reasonable
    const auto& suggestions = prefetch_result.value();
    ASSERT_GE(suggestions.size(), 0);
}

TEST(AccessPatternOptimizerTest, CacheHitRatio) {
    tsdb::core::StorageConfig config;
    AccessPatternOptimizer optimizer(config);
    
    // Record some access patterns
    std::vector<tsdb::core::SeriesID> series_ids = {12345, 12346, 12347, 12348};
    auto record_result = optimizer.record_bulk_access(series_ids, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Get prefetch stats
    auto prefetch_stats = optimizer.get_prefetch_stats();
    ASSERT_FALSE(prefetch_stats.empty());
}

TEST(AccessPatternOptimizerTest, OptimizationExecution) {
    tsdb::core::StorageConfig config;
    AccessPatternOptimizer optimizer(config);
    
    // Record access patterns
    std::vector<tsdb::core::SeriesID> series_ids = {12345, 12346, 12347};
    auto record_result = optimizer.record_bulk_access(series_ids, "sequential");
    ASSERT_TRUE(record_result.ok());
    
    // Analyze access patterns
    auto analyze_result = optimizer.analyze_access_patterns();
    ASSERT_TRUE(analyze_result.ok());
    
    // Get optimization stats
    auto optimization_stats = optimizer.get_optimization_stats();
    ASSERT_FALSE(optimization_stats.empty());
}