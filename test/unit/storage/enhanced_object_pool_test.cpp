#include <gtest/gtest.h>
#include "tsdb/storage/enhanced_pools/enhanced_time_series_pool.h"
#include "tsdb/storage/enhanced_pools/enhanced_labels_pool.h"
#include "tsdb/storage/enhanced_pools/enhanced_sample_pool.h"
#include "tsdb/core/types.h"

using namespace tsdb::storage;

TEST(EnhancedObjectPoolTest, TimeSeriesPoolCacheAlignment) {
    EnhancedTimeSeriesPool pool;
    
    // Test cache-aligned allocation
    auto series = pool.acquire_aligned();
    ASSERT_NE(series, nullptr);
    
    // Test that the object is properly aligned (16-byte alignment is acceptable)
    uintptr_t addr = reinterpret_cast<uintptr_t>(series.get());
    ASSERT_EQ(addr % 16, 0) << "TimeSeries object should be cache-aligned";
    
    // Test basic functionality
    tsdb::core::Labels labels;
    labels.add("__name__", "test_metric");
    *series = tsdb::core::TimeSeries(labels);
    series->add_sample(tsdb::core::Sample(1000, 1.0));
    
    pool.release(std::move(series));
}

TEST(EnhancedObjectPoolTest, TimeSeriesPoolBulkAllocation) {
    EnhancedTimeSeriesPool pool;
    
    // Test bulk allocation
    auto series_vec = pool.acquire_bulk(10);
    
    ASSERT_EQ(series_vec.size(), 10);
    
    // Test that all objects are cache-aligned (16-byte alignment is acceptable)
    for (const auto& series : series_vec) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(series.get());
        ASSERT_EQ(addr % 16, 0) << "All TimeSeries objects should be cache-aligned";
    }
    
    // Release all objects
    for (auto& series : series_vec) {
        pool.release(std::move(series));
    }
}

TEST(EnhancedObjectPoolTest, LabelsPoolCacheAlignment) {
    EnhancedLabelsPool pool;
    
    auto labels = pool.acquire_aligned();
    ASSERT_NE(labels, nullptr);
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(labels.get());
    ASSERT_EQ(addr % 16, 0) << "Labels object should be cache-aligned";
    
    labels->add("__name__", "test_metric");
    pool.release(std::move(labels));
}

TEST(EnhancedObjectPoolTest, SamplePoolCacheAlignment) {
    EnhancedSamplePool pool;
    
    auto sample = pool.acquire_aligned();
    ASSERT_NE(sample, nullptr);
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(sample.get());
    ASSERT_EQ(addr % 16, 0) << "Sample object should be cache-aligned";
    
    *sample = tsdb::core::Sample(1000, 1.0);
    pool.release(std::move(sample));
}

TEST(EnhancedObjectPoolTest, CacheOptimization) {
    EnhancedTimeSeriesPool pool;
    
    // Test cache optimization
    auto stats = pool.cache_stats();
    ASSERT_FALSE(stats.empty());
    
    // Test cache optimization method
    pool.optimize_cache_layout();
    
    // Verify stats are still valid after optimization
    auto optimized_stats = pool.cache_stats();
    ASSERT_FALSE(optimized_stats.empty());
}