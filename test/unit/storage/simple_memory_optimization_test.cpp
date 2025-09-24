#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/simple_cache_alignment.h"
#include "tsdb/storage/memory_optimization/simple_sequential_layout.h"
#include "tsdb/storage/memory_optimization/simple_access_pattern_tracker.h"
#include "tsdb/core/types.h"
#include "tsdb/core/types.h"

using namespace tsdb::storage::memory_optimization;

TEST(SimpleMemoryOptimizationTest, CacheAlignment) {
    // Test cache alignment
    char data[1000];
    void* aligned_ptr = SimpleCacheAlignment::align_to_cache_line(data);
    
    // Check that the aligned pointer is cache line aligned
    ASSERT_TRUE(SimpleCacheAlignment::is_cache_aligned(aligned_ptr));
    
    // Test cache line size
    ASSERT_EQ(SimpleCacheAlignment::get_cache_line_size(), 64);
    
    // Test size alignment
    size_t aligned_size = SimpleCacheAlignment::align_size_to_cache_line(100);
    ASSERT_EQ(aligned_size, 128); // 100 aligned to 64-byte boundary
}

TEST(SimpleMemoryOptimizationTest, SequentialLayout) {
    // Create a test TimeSeries
    tsdb::core::Labels labels;
    labels.add("__name__", "test_metric");
    labels.add("host", "server1");
    tsdb::core::TimeSeries series(labels);
    series.add_sample(tsdb::core::Sample(1000, 0.5));
    series.add_sample(tsdb::core::Sample(2000, 0.6));
    
    // Test sequential layout optimization
    SimpleSequentialLayout::optimize_time_series_layout(series);
    
    // Verify data integrity
    ASSERT_EQ(series.samples().size(), 2);
    ASSERT_EQ(series.samples()[0].timestamp(), 1000);
    ASSERT_EQ(series.samples()[0].value(), 0.5);
    ASSERT_EQ(series.samples()[1].timestamp(), 2000);
    ASSERT_EQ(series.samples()[1].value(), 0.6);
}

TEST(SimpleMemoryOptimizationTest, AccessPatternTracker) {
    SimpleAccessPatternTracker tracker;
    
    // Test single access recording
    void* ptr1 = reinterpret_cast<void*>(0x1000);
    tracker.record_access(ptr1);
    
    ASSERT_EQ(tracker.get_access_count(ptr1), 1);
    
    // Test multiple accesses
    for (int i = 0; i < 15; ++i) {
        tracker.record_access(ptr1);
    }
    
    ASSERT_EQ(tracker.get_access_count(ptr1), 16);
    
    // Test bulk access recording
    std::vector<void*> addresses = {
        reinterpret_cast<void*>(0x2000),
        reinterpret_cast<void*>(0x3000),
        reinterpret_cast<void*>(0x4000)
    };
    
    tracker.record_bulk_access(addresses);
    
    for (void* addr : addresses) {
        ASSERT_EQ(tracker.get_access_count(addr), 1);
    }
    
    // Test hot/cold address identification
    tracker.analyze_patterns();
    
    auto hot_addresses = tracker.get_hot_addresses();
    auto cold_addresses = tracker.get_cold_addresses();
    
    // ptr1 should be hot (accessed 16 times)
    ASSERT_TRUE(std::find(hot_addresses.begin(), hot_addresses.end(), ptr1) != hot_addresses.end());
    
    // Other addresses should be cold (accessed only 1 time)
    for (void* addr : addresses) {
        ASSERT_TRUE(std::find(cold_addresses.begin(), cold_addresses.end(), addr) != cold_addresses.end());
    }
}

TEST(SimpleMemoryOptimizationTest, AccessPatternStats) {
    SimpleAccessPatternTracker tracker;
    
    // Record some accesses
    void* ptr1 = reinterpret_cast<void*>(0x1000);
    void* ptr2 = reinterpret_cast<void*>(0x2000);
    
    for (int i = 0; i < 5; ++i) {
        tracker.record_access(ptr1);
    }
    
    for (int i = 0; i < 2; ++i) {
        tracker.record_access(ptr2);
    }
    
    tracker.analyze_patterns();
    
    // Test stats
    std::string stats = tracker.get_stats();
    ASSERT_FALSE(stats.empty());
    ASSERT_TRUE(stats.find("Total Accesses: 7") != std::string::npos);
    ASSERT_TRUE(stats.find("Unique Addresses: 2") != std::string::npos);
}

TEST(SimpleMemoryOptimizationTest, ClearAccessPatterns) {
    SimpleAccessPatternTracker tracker;
    
    // Record some accesses
    void* ptr1 = reinterpret_cast<void*>(0x1000);
    tracker.record_access(ptr1);
    
    ASSERT_EQ(tracker.get_access_count(ptr1), 1);
    
    // Clear and verify
    tracker.clear();
    
    ASSERT_EQ(tracker.get_access_count(ptr1), 0);
    
    std::string stats = tracker.get_stats();
    ASSERT_TRUE(stats.find("Total Accesses: 0") != std::string::npos);
    ASSERT_TRUE(stats.find("Unique Addresses: 0") != std::string::npos);
}
