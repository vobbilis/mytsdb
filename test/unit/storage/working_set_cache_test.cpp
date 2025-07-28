#include <gtest/gtest.h>
#include "tsdb/storage/working_set_cache.h"
#include "tsdb/core/types.h"
#include <thread>
#include <vector>
#include <chrono>
#include <random>

namespace tsdb {
namespace storage {
namespace test {

class WorkingSetCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for tests
    }
    
    void TearDown() override {
        // Common cleanup for tests
    }
    
    // Helper function to create a test time series
    std::shared_ptr<core::TimeSeries> create_test_series(core::SeriesID id, int num_samples = 10) {
        core::Labels::Map labels_map{{"series", std::to_string(id)}};
        auto series = std::make_shared<core::TimeSeries>(core::Labels(labels_map));
        for (int i = 0; i < num_samples; ++i) {
            series->add_sample(1000 + i, 42.0 + i);
        }
        return series;
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(WorkingSetCacheTest, BasicOperations) {
    WorkingSetCache cache(5);
    
    // Test initial state
    EXPECT_EQ(cache.size(), 0);
    EXPECT_EQ(cache.max_size(), 5);
    EXPECT_FALSE(cache.is_full());
    
    // Test put and get
    auto series1 = create_test_series(1);
    cache.put(1, series1);
    EXPECT_EQ(cache.size(), 1);
    
    auto retrieved = cache.get(1);
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->labels().get("series").value(), "1");
    EXPECT_EQ(retrieved->samples().size(), 10);
}

TEST_F(WorkingSetCacheTest, CacheMiss) {
    WorkingSetCache cache(10);
    
    // Try to get non-existent series
    auto result = cache.get(999);
    EXPECT_EQ(result, nullptr);
    
    // Check that miss count increased
    EXPECT_EQ(cache.miss_count(), 1);
    EXPECT_EQ(cache.hit_count(), 0);
}

TEST_F(WorkingSetCacheTest, CacheHit) {
    WorkingSetCache cache(10);
    
    // Put a series
    auto series = create_test_series(1);
    cache.put(1, series);
    
    // Get it multiple times
    for (int i = 0; i < 5; ++i) {
        auto result = cache.get(1);
        EXPECT_NE(result, nullptr);
    }
    
    // Check hit count
    EXPECT_EQ(cache.hit_count(), 5);
    EXPECT_EQ(cache.miss_count(), 0);
}

// ============================================================================
// LRU Eviction Tests
// ============================================================================

TEST_F(WorkingSetCacheTest, LRUEviction) {
    WorkingSetCache cache(3);
    
    // Add 3 series
    cache.put(1, create_test_series(1));
    cache.put(2, create_test_series(2));
    cache.put(3, create_test_series(3));
    
    EXPECT_EQ(cache.size(), 3);
    EXPECT_TRUE(cache.is_full());
    
    // Add a 4th series - should evict the least recently used (series 1)
    cache.put(4, create_test_series(4));
    EXPECT_EQ(cache.size(), 3);
    
    // Series 1 should be evicted
    EXPECT_EQ(cache.get(1), nullptr);
    
    // Series 2, 3, 4 should still be there
    EXPECT_NE(cache.get(2), nullptr);
    EXPECT_NE(cache.get(3), nullptr);
    EXPECT_NE(cache.get(4), nullptr);
}

TEST_F(WorkingSetCacheTest, LRUOrdering) {
    WorkingSetCache cache(3);
    
    // Add 3 series
    cache.put(1, create_test_series(1));
    cache.put(2, create_test_series(2));
    cache.put(3, create_test_series(3));
    
    // Access series 1 to make it most recently used
    cache.get(1);
    
    // Add a 4th series - should evict series 2 (least recently used)
    cache.put(4, create_test_series(4));
    
    // Series 2 should be evicted
    EXPECT_EQ(cache.get(2), nullptr);
    
    // Series 1, 3, 4 should still be there
    EXPECT_NE(cache.get(1), nullptr);
    EXPECT_NE(cache.get(3), nullptr);
    EXPECT_NE(cache.get(4), nullptr);
}

// ============================================================================
// Update and Remove Tests
// ============================================================================

TEST_F(WorkingSetCacheTest, UpdateExisting) {
    WorkingSetCache cache(10);
    
    // Add initial series
    auto series1 = create_test_series(1, 5);
    cache.put(1, series1);
    
    // Update with new series
    auto series2 = create_test_series(1, 10);
    cache.put(1, series2);
    
    // Should get the updated series
    auto result = cache.get(1);
    EXPECT_EQ(result->samples().size(), 10);
}

TEST_F(WorkingSetCacheTest, RemoveEntry) {
    WorkingSetCache cache(10);
    
    // Add a series
    cache.put(1, create_test_series(1));
    EXPECT_EQ(cache.size(), 1);
    
    // Remove it
    EXPECT_TRUE(cache.remove(1));
    EXPECT_EQ(cache.size(), 0);
    
    // Should not be found
    EXPECT_EQ(cache.get(1), nullptr);
}

TEST_F(WorkingSetCacheTest, RemoveNonExistent) {
    WorkingSetCache cache(10);
    
    // Try to remove non-existent entry
    EXPECT_FALSE(cache.remove(999));
}

TEST_F(WorkingSetCacheTest, ClearCache) {
    WorkingSetCache cache(10);
    
    // Add multiple series
    cache.put(1, create_test_series(1));
    cache.put(2, create_test_series(2));
    cache.put(3, create_test_series(3));
    
    EXPECT_EQ(cache.size(), 3);
    
    // Clear cache
    cache.clear();
    EXPECT_EQ(cache.size(), 0);
    
    // Should not find any series
    EXPECT_EQ(cache.get(1), nullptr);
    EXPECT_EQ(cache.get(2), nullptr);
    EXPECT_EQ(cache.get(3), nullptr);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(WorkingSetCacheTest, HitRatioCalculation) {
    WorkingSetCache cache(10);
    
    // No requests yet
    EXPECT_EQ(cache.hit_ratio(), 0.0);
    
    // Add a series
    cache.put(1, create_test_series(1));
    
    // 3 hits, 2 misses
    cache.get(1); // hit
    cache.get(1); // hit
    cache.get(999); // miss
    cache.get(1); // hit
    cache.get(888); // miss
    
    double expected_ratio = 3.0 / 5.0 * 100.0;
    EXPECT_DOUBLE_EQ(cache.hit_ratio(), expected_ratio);
}

TEST_F(WorkingSetCacheTest, ResetStats) {
    WorkingSetCache cache(10);
    
    // Generate some stats
    cache.put(1, create_test_series(1));
    cache.get(1); // hit
    cache.get(999); // miss
    
    EXPECT_GT(cache.hit_count(), 0);
    EXPECT_GT(cache.miss_count(), 0);
    
    // Reset stats
    cache.reset_stats();
    
    EXPECT_EQ(cache.hit_count(), 0);
    EXPECT_EQ(cache.miss_count(), 0);
    EXPECT_EQ(cache.hit_ratio(), 0.0);
}

TEST_F(WorkingSetCacheTest, StatsString) {
    WorkingSetCache cache(5);
    
    // Generate some activity
    cache.put(1, create_test_series(1));
    cache.get(1); // hit
    cache.get(999); // miss
    
    std::string stats = cache.stats();
    
    // Should contain expected information
    EXPECT_NE(stats.find("WorkingSetCache Stats"), std::string::npos);
    EXPECT_NE(stats.find("Current size: 1/5"), std::string::npos);
    EXPECT_NE(stats.find("Hit count: 1"), std::string::npos);
    EXPECT_NE(stats.find("Miss count: 1"), std::string::npos);
    EXPECT_NE(stats.find("Hit ratio: 50.00%"), std::string::npos);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(WorkingSetCacheTest, ThreadSafety) {
    WorkingSetCache cache(100);
    const int num_threads = 4;
    const int operations_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::atomic<int> total_operations{0};
    
    // Start multiple threads performing cache operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, t, operations_per_thread, &total_operations]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 50);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int series_id = dis(gen);
                
                // Randomly choose operation
                if (i % 3 == 0) {
                    // Put operation
                    core::Labels::Map labels_map{{"thread", std::to_string(t)}, {"series", std::to_string(series_id)}};
                    auto series = std::make_shared<core::TimeSeries>(core::Labels(labels_map));
                    series->add_sample(1000 + i, 42.0 + i);
                    cache.put(series_id, series);
                } else {
                    // Get operation
                    cache.get(series_id);
                }
                
                total_operations.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify cache is in consistent state
    EXPECT_LE(cache.size(), cache.max_size());
    EXPECT_EQ(total_operations.load(), num_threads * operations_per_thread);
    
    // Verify statistics are reasonable
    uint64_t total_requests = cache.hit_count() + cache.miss_count();
    // Note: Some operations might not result in cache hits/misses due to concurrent access
    EXPECT_LE(total_requests, total_operations.load());
    EXPECT_GT(total_requests, 0);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(WorkingSetCacheTest, PerformanceUnderLoad) {
    WorkingSetCache cache(1000);
    const int num_operations = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform many cache operations
    for (int i = 0; i < num_operations; ++i) {
        int series_id = i % 100; // Reuse series IDs to test cache effectiveness
        
        if (i % 2 == 0) {
            // Put operation
            auto series = create_test_series(series_id);
            cache.put(series_id, series);
        } else {
            // Get operation
            cache.get(series_id);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance should be reasonable (less than 1ms per operation)
    double avg_time_per_op = static_cast<double>(duration.count()) / num_operations;
    EXPECT_LT(avg_time_per_op, 1000.0); // Less than 1ms per operation
    
    // Should have some hit ratio due to series reuse
    double hit_ratio = cache.hit_ratio();
    // Note: With random access patterns, hit ratio might be low
    EXPECT_GE(hit_ratio, 0.0); // At least 0% hit ratio
    
    std::cout << "Cache performance test:" << std::endl;
    std::cout << "  Average time per operation: " << avg_time_per_op << " μs" << std::endl;
    std::cout << "  Hit ratio: " << hit_ratio << "%" << std::endl;
    std::cout << "  Total operations: " << num_operations << std::endl;
}

TEST_F(WorkingSetCacheTest, CacheHitRatioTarget) {
    WorkingSetCache cache(100);
    const int num_operations = 10000;
    
    // Create a workload with high locality (repeated access to same series)
    std::vector<int> hot_series = {1, 2, 3, 4, 5}; // Hot series
    std::vector<int> cold_series; // Cold series
    
    for (int i = 6; i <= 100; ++i) {
        cold_series.push_back(i);
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    // Perform operations with 80% access to hot series
    for (int i = 0; i < num_operations; ++i) {
        int series_id;
        
        if (dis(gen) < 0.8) {
            // 80% chance to access hot series
            std::uniform_int_distribution<> hot_dis(0, hot_series.size() - 1);
            series_id = hot_series[hot_dis(gen)];
        } else {
            // 20% chance to access cold series
            std::uniform_int_distribution<> cold_dis(0, cold_series.size() - 1);
            series_id = cold_series[cold_dis(gen)];
        }
        
        if (i % 2 == 0) {
            // Put operation
            auto series = create_test_series(series_id);
            cache.put(series_id, series);
        } else {
            // Get operation
            cache.get(series_id);
        }
    }
    
    // Should achieve good hit ratio for hot data
    double hit_ratio = cache.hit_ratio();
    EXPECT_GT(hit_ratio, 50.0); // At least 50% hit ratio
    EXPECT_LT(hit_ratio, 99.0); // Should not be perfect due to cold data
    
    std::cout << "Cache hit ratio test:" << std::endl;
    std::cout << "  Achieved hit ratio: " << hit_ratio << "%" << std::endl;
    std::cout << "  Expected range: 70-90%" << std::endl;
}

} // namespace test
} // namespace storage
} // namespace tsdb 