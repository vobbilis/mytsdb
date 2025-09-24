#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/adaptive_memory_integration.h"
#include "tsdb/storage/memory_optimization/tiered_memory_integration.h"
#include "tsdb/storage/memory_optimization/sequential_layout_optimizer.h"
#include "tsdb/storage/memory_optimization/access_pattern_optimizer.h"
#include "tsdb/core/types.h"

TEST(MemoryOptimizationTest, AdaptiveMemoryIntegration) {
    AdaptiveMemoryIntegration integration;
    
    // Test allocation
    auto result = integration.allocate_optimized(1024);
    ASSERT_TRUE(result.ok());
    
    // Test deallocation
    auto dealloc_result = integration.deallocate_optimized(result.value());
    ASSERT_TRUE(dealloc_result.ok());
}

TEST(MemoryOptimizationTest, TieredMemoryIntegration) {
    TieredMemoryIntegration integration;
    
    // Test series promotion
    tsdb::core::TimeSeries series;
    series.add_label({"__name__", "test_metric"});
    series.add_sample(tsdb::core::Sample(1000, 1.0));
    
    auto promote_result = integration.promote_series(&series);
    ASSERT_TRUE(promote_result.ok());
    
    // Test series demotion
    auto demote_result = integration.demote_series(&series);
    ASSERT_TRUE(demote_result.ok());
}

TEST(MemoryOptimizationTest, SequentialLayoutOptimizer) {
    SequentialLayoutOptimizer optimizer;
    
    // Test TimeSeries layout optimization
    tsdb::core::TimeSeries series;
    series.add_label({"__name__", "test_metric"});
    series.add_sample(tsdb::core::Sample(1000, 1.0));
    series.add_sample(tsdb::common::Sample(2000, 2.0));
    
    auto result = optimizer.optimize_time_series_layout(series);
    ASSERT_TRUE(result.ok());
    
    // Test block layout optimization
    std::vector<tsdb::core::TimeSeries> series_vec;
    series_vec.push_back(series);
    
    auto block_result = optimizer.optimize_block_layout(series_vec);
    ASSERT_TRUE(block_result.ok());
}

TEST(MemoryOptimizationTest, AccessPatternOptimizer) {
    AccessPatternOptimizer optimizer;
    
    // Test access recording
    tsdb::core::TimeSeries series;
    series.add_label({"__name__", "test_metric"});
    
    auto record_result = optimizer.record_access(&series);
    ASSERT_TRUE(record_result.ok());
    
    // Test pattern analysis
    auto analysis_result = optimizer.analyze_access_patterns();
    ASSERT_TRUE(analysis_result.ok());
    
    // Test prefetch suggestions
    auto prefetch_result = optimizer.suggest_prefetch_addresses();
    ASSERT_TRUE(prefetch_result.ok());
}

TEST(MemoryOptimizationTest, MemoryEfficiency) {
    SequentialLayoutOptimizer optimizer;
    
    // Test memory efficiency
    tsdb::core::TimeSeries series;
    series.add_label({"__name__", "test_metric"});
    series.add_sample(tsdb::core::Sample(1000, 1.0));
    
    auto result = optimizer.optimize_time_series_layout(series);
    ASSERT_TRUE(result.ok());
    
    // Test that optimization doesn't corrupt data
    ASSERT_EQ(series.samples().size(), 1);
    ASSERT_EQ(series.samples()[0].timestamp(), 1000);
    ASSERT_EQ(series.samples()[0].value(), 1.0);
}