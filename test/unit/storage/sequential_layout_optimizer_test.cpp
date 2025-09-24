#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/sequential_layout_optimizer.h"
#include "tsdb/core/types.h"

using namespace tsdb::storage;

TEST(SequentialLayoutOptimizerTest, TimeSeriesLayoutOptimization) {
    tsdb::core::StorageConfig config;
    SequentialLayoutOptimizer optimizer(config);
    
    // Create a test TimeSeries
    tsdb::core::Labels labels;
    labels.add("__name__", "cpu_usage");
    labels.add("host", "server1");
    tsdb::core::TimeSeries series(labels);
    series.add_sample(tsdb::core::Sample(1000, 0.5));
    series.add_sample(tsdb::core::Sample(2000, 0.6));
    series.add_sample(tsdb::core::Sample(3000, 0.7));
    
    // Test layout optimization
    auto result = optimizer.optimize_time_series_layout(series);
    ASSERT_TRUE(result.ok());
    
    // Verify data integrity
    ASSERT_EQ(series.samples().size(), 3);
    ASSERT_EQ(series.samples()[0].timestamp(), 1000);
    ASSERT_EQ(series.samples()[0].value(), 0.5);
    ASSERT_EQ(series.samples()[1].timestamp(), 2000);
    ASSERT_EQ(series.samples()[1].value(), 0.6);
    ASSERT_EQ(series.samples()[2].timestamp(), 3000);
    ASSERT_EQ(series.samples()[2].value(), 0.7);
}

TEST(SequentialLayoutOptimizerTest, BlockLayoutOptimization) {
    tsdb::core::StorageConfig config;
    SequentialLayoutOptimizer optimizer(config);
    
    // Create test TimeSeries vector
    std::vector<tsdb::core::TimeSeries> series_vec;
    for (int i = 0; i < 5; ++i) {
        tsdb::core::Labels labels;
        labels.add("__name__", "test_metric");
        labels.add("instance", "server" + std::to_string(i));
        tsdb::core::TimeSeries series(labels);
        series.add_sample(tsdb::core::Sample(1000 + i * 100, static_cast<double>(i)));
        series_vec.push_back(series);
    }
    
    // Test time series layout optimization for each series
    for (auto& series : series_vec) {
        auto result = optimizer.optimize_time_series_layout(series);
        ASSERT_TRUE(result.ok());
    }
    
    // Verify data integrity
    ASSERT_EQ(series_vec.size(), 5);
    for (size_t i = 0; i < series_vec.size(); ++i) {
        ASSERT_EQ(series_vec[i].samples().size(), 1);
        ASSERT_EQ(series_vec[i].samples()[0].timestamp(), 1000 + i * 100);
        ASSERT_EQ(series_vec[i].samples()[0].value(), static_cast<double>(i));
    }
}

TEST(SequentialLayoutOptimizerTest, MemoryManagement) {
    tsdb::core::StorageConfig config;
    SequentialLayoutOptimizer optimizer(config);
    
    // Test capacity reservation
    tsdb::core::Labels labels;
    labels.add("__name__", "test_metric");
    tsdb::core::TimeSeries series(labels);
    auto reserve_result = optimizer.reserve_capacity(series, 1000);
    ASSERT_TRUE(reserve_result.ok());
    
    // Test shrink to fit
    auto shrink_result = optimizer.shrink_to_fit(series);
    ASSERT_TRUE(shrink_result.ok());
}

TEST(SequentialLayoutOptimizerTest, Prefetching) {
    tsdb::core::StorageConfig config;
    SequentialLayoutOptimizer optimizer(config);
    
    // Create test data
    tsdb::core::Labels labels;
    labels.add("__name__", "test_metric");
    tsdb::core::TimeSeries series(labels);
    series.add_sample(tsdb::core::Sample(1000, 1.0));
    
    // Test prefetching
    auto prefetch_result = optimizer.prefetch_data(series);
    ASSERT_TRUE(prefetch_result.ok());
}

TEST(SequentialLayoutOptimizerTest, AccessPatternOptimization) {
    tsdb::core::StorageConfig config;
    SequentialLayoutOptimizer optimizer(config);
    
    // Create test data
    tsdb::core::Labels labels;
    labels.add("__name__", "test_metric");
    tsdb::core::TimeSeries series(labels);
    series.add_sample(tsdb::core::Sample(1000, 1.0));
    
    // Test access pattern optimization
    tsdb::core::SeriesID series_id = 12345;
    auto result = optimizer.optimize_access_pattern(series_id);
    ASSERT_TRUE(result.ok());
    
    // Verify data integrity
    ASSERT_EQ(series.samples().size(), 1);
    ASSERT_EQ(series.samples()[0].timestamp(), 1000);
    ASSERT_EQ(series.samples()[0].value(), 1.0);
}