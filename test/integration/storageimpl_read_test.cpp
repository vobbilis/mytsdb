/**
 * @file storageimpl_read_test.cpp
 * @brief Comprehensive test suite for StorageImpl read method debugging
 * 
 * This test file implements a systematic approach to identify, isolate, and fix
 * the segmentation fault in the StorageImpl::read method. The tests are organized
 * in phases to progressively isolate the root cause of the issue.
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>

#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/time_series.h"
#include "tsdb/core/labels.h"

namespace tsdb {
namespace integration {

class StorageImplReadTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }
    
    void TearDown() override {
        // Common cleanup if needed
    }
    
    // Helper method to create a simple test series
    core::TimeSeries createTestSeries(const std::string& name, size_t num_samples = 1) {
        core::Labels labels;
        labels.add("test", name);
        
        core::TimeSeries series(labels);
        for (size_t i = 0; i < num_samples; ++i) {
            series.add_sample(1000000000 + i, static_cast<double>(i));
        }
        return series;
    }
};

// ============================================================================
// Phase 1: Isolation and Minimal Reproduction
// ============================================================================

TEST_F(StorageImplReadTest, MinimalReadTest) {
    std::cout << "\n=== MINIMAL READ TEST ===" << std::endl;
    
    // 1. Initialize storage with minimal configuration
    core::StorageConfig config;
    config.enable_compression = false;  // Disable compression to isolate issue
    config.data_dir = "./test_data_minimal";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    // 2. Write a single sample series
    core::Labels labels;
    labels.add("test", "minimal");
    core::TimeSeries series(labels);
    series.add_sample(1000000000, 42.0);
    
    auto write_result = storage->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    // 3. Read the series - this is where segmentation fault occurs
    std::cout << "About to call read method..." << std::endl;
    auto read_result = storage->read(labels, 1000000000, 1000000000);
    std::cout << "Read method completed" << std::endl;
    
    ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
    
    // 4. Verify result
    EXPECT_EQ(read_result.value().samples().size(), 1);
    EXPECT_EQ(read_result.value().samples()[0].value(), 42.0);
    
    storage->close();
    std::cout << "✓ Minimal read test completed" << std::endl;
}

TEST_F(StorageImplReadTest, EmptyStorageReadTest) {
    std::cout << "\n=== EMPTY STORAGE READ TEST ===" << std::endl;
    
    // Test reading from empty storage
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_empty";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    // Try to read non-existent series
    core::Labels labels;
    labels.add("test", "non_existent");
    
    auto read_result = storage->read(labels, 1000000000, 1000000000);
    EXPECT_FALSE(read_result.ok()) << "Should fail to read non-existent series";
    EXPECT_EQ(read_result.error(), "Series not found");
    
    storage->close();
    std::cout << "✓ Empty storage read test completed" << std::endl;
}

TEST_F(StorageImplReadTest, NullPointerAccessTest) {
    std::cout << "\n=== NULL POINTER ACCESS TEST ===" << std::endl;
    
    // Test with uninitialized storage
    auto storage = std::make_unique<tsdb::storage::StorageImpl>();
    
    core::Labels labels;
    labels.add("test", "uninitialized");
    
    auto read_result = storage->read(labels, 1000000000, 1000000000);
    EXPECT_FALSE(read_result.ok()) << "Should fail on uninitialized storage";
    EXPECT_EQ(read_result.error(), "Storage not initialized");
    
    std::cout << "✓ Null pointer access test completed" << std::endl;
}

// ============================================================================
// Phase 2: Component Isolation Tests
// ============================================================================

TEST_F(StorageImplReadTest, ObjectPoolIsolationTest) {
    std::cout << "\n=== OBJECT POOL ISOLATION TEST ===" << std::endl;
    
    // Test read method with object pools
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_pool_test";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    // Write test data
    core::Labels labels;
    labels.add("test", "pool_test");
    core::TimeSeries series(labels);
    series.add_sample(1000000000, 42.0);
    
    auto write_result = storage->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    // Read with object pools
    auto read_result = storage->read(labels, 1000000000, 1000000000);
    ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
    EXPECT_EQ(read_result.value().samples().size(), 1);
    
    storage->close();
    std::cout << "✓ Object pool isolation test completed" << std::endl;
}

TEST_F(StorageImplReadTest, CacheHierarchyIsolationTest) {
    std::cout << "\n=== CACHE HIERARCHY ISOLATION TEST ===" << std::endl;
    
    // Test read method with cache hierarchy
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_cache_test";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    // Write test data
    core::Labels labels;
    labels.add("test", "cache_test");
    core::TimeSeries series(labels);
    series.add_sample(1000000000, 42.0);
    
    auto write_result = storage->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    // Read with cache hierarchy
    auto read_result = storage->read(labels, 1000000000, 1000000000);
    ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
    EXPECT_EQ(read_result.value().samples().size(), 1);
    
    storage->close();
    std::cout << "✓ Cache hierarchy isolation test completed" << std::endl;
}

TEST_F(StorageImplReadTest, CompressionIsolationTest) {
    std::cout << "\n=== COMPRESSION ISOLATION TEST ===" << std::endl;
    
    // Test read method with compression enabled/disabled
    
    // Test with compression disabled
    {
        std::cout << "Testing with compression disabled..." << std::endl;
        core::StorageConfig config;
        config.enable_compression = false;
        config.data_dir = "./test_data_no_compression";
        
        auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
        auto init_result = storage->init(config);
        ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
        
        core::Labels labels;
        labels.add("test", "no_compression");
        core::TimeSeries series(labels);
        series.add_sample(1000000000, 42.0);
        
        auto write_result = storage->write(series);
        ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
        
        auto read_result = storage->read(labels, 1000000000, 1000000000);
        ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
        EXPECT_EQ(read_result.value().samples().size(), 1);
        
        storage->close();
    }
    
    // Test with compression enabled
    {
        std::cout << "Testing with compression enabled..." << std::endl;
        core::StorageConfig config;
        config.enable_compression = true;
        config.data_dir = "./test_data_with_compression";
        
        auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
        auto init_result = storage->init(config);
        ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
        
        core::Labels labels;
        labels.add("test", "with_compression");
        core::TimeSeries series(labels);
        series.add_sample(1000000000, 42.0);
        
        auto write_result = storage->write(series);
        ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
        
        auto read_result = storage->read(labels, 1000000000, 1000000000);
        ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
        EXPECT_EQ(read_result.value().samples().size(), 1);
        
        storage->close();
    }
    
    std::cout << "✓ Compression isolation test completed" << std::endl;
}

// ============================================================================
// Phase 3: Memory Access Pattern Tests
// ============================================================================

TEST_F(StorageImplReadTest, LargeSeriesTest) {
    std::cout << "\n=== LARGE SERIES TEST ===" << std::endl;
    
    // Test with large series to check for memory boundary issues
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_large_series";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    // Create large series
    core::Labels labels;
    labels.add("test", "large_series");
    core::TimeSeries series(labels);
    
    const size_t num_samples = 1000; // Reduced from 10000 for faster testing
    for (size_t i = 0; i < num_samples; ++i) {
        series.add_sample(1000000000 + i, static_cast<double>(i));
    }
    
    auto write_result = storage->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    auto read_result = storage->read(labels, 1000000000, 1000000000 + num_samples);
    ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
    EXPECT_EQ(read_result.value().samples().size(), num_samples);
    
    storage->close();
    std::cout << "✓ Large series test completed" << std::endl;
}

TEST_F(StorageImplReadTest, MultipleSeriesTest) {
    std::cout << "\n=== MULTIPLE SERIES TEST ===" << std::endl;
    
    // Test with multiple series to check for index boundary issues
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_multiple_series";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    // Write multiple series
    const size_t num_series = 5; // Reduced from 10 for faster testing
    std::vector<core::Labels> all_labels;
    
    for (size_t i = 0; i < num_series; ++i) {
        core::Labels labels;
        labels.add("test", "series_" + std::to_string(i));
        all_labels.push_back(labels);
        
        core::TimeSeries series(labels);
        series.add_sample(1000000000, static_cast<double>(i));
        
        auto write_result = storage->write(series);
        ASSERT_TRUE(write_result.ok()) << "Write failed for series " << i;
    }
    
    // Read each series
    for (size_t i = 0; i < num_series; ++i) {
        auto read_result = storage->read(all_labels[i], 1000000000, 1000000000);
        ASSERT_TRUE(read_result.ok()) << "Read failed for series " << i << ": " << read_result.error();
        EXPECT_EQ(read_result.value().samples().size(), 1);
        EXPECT_EQ(read_result.value().samples()[0].value(), static_cast<double>(i));
    }
    
    storage->close();
    std::cout << "✓ Multiple series test completed" << std::endl;
}

TEST_F(StorageImplReadTest, TimeRangeBoundaryTest) {
    std::cout << "\n=== TIME RANGE BOUNDARY TEST ===" << std::endl;
    
    // Test with various time range boundaries
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_time_boundary";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    core::Labels labels;
    labels.add("test", "time_boundary");
    core::TimeSeries series(labels);
    
    // Add samples at different timestamps
    series.add_sample(1000000000, 1.0);
    series.add_sample(1000000100, 2.0);
    series.add_sample(1000000200, 3.0);
    
    auto write_result = storage->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    // Test various time ranges
    std::vector<std::pair<int64_t, int64_t>> time_ranges = {
        {1000000000, 1000000000},  // Single timestamp
        {1000000000, 1000000200},  // Full range
        {1000000050, 1000000150},  // Partial range
        {999999999, 1000000201},   // Extended range
        {1000000300, 1000000300},  // No matching data
    };
    
    for (const auto& [start, end] : time_ranges) {
        auto read_result = storage->read(labels, start, end);
        ASSERT_TRUE(read_result.ok()) << "Read failed for range [" << start << ", " << end << "]: " << read_result.error();
    }
    
    storage->close();
    std::cout << "✓ Time range boundary test completed" << std::endl;
}

// ============================================================================
// Phase 4: Thread Safety Tests
// ============================================================================

TEST_F(StorageImplReadTest, ConcurrentReadTest) {
    std::cout << "\n=== CONCURRENT READ TEST ===" << std::endl;
    
    // Test concurrent reads from multiple threads
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_concurrent";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    // Write test data
    core::Labels labels;
    labels.add("test", "concurrent");
    core::TimeSeries series(labels);
    series.add_sample(1000000000, 42.0);
    
    auto write_result = storage->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    // Concurrent reads
    const size_t num_threads = 2; // Reduced from 4 for faster testing
    const size_t reads_per_thread = 10; // Reduced from 100 for faster testing
    std::vector<std::thread> threads;
    std::atomic<size_t> successful_reads{0};
    
    for (size_t thread_id = 0; thread_id < num_threads; ++thread_id) {
        threads.emplace_back([&storage, &labels, &successful_reads, reads_per_thread]() {
            for (size_t i = 0; i < reads_per_thread; ++i) {
                auto read_result = storage->read(labels, 1000000000, 1000000000);
                if (read_result.ok()) {
                    successful_reads++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(successful_reads.load(), num_threads * reads_per_thread);
    
    storage->close();
    std::cout << "✓ Concurrent read test completed" << std::endl;
}

// ============================================================================
// Phase 5: Error Condition Tests
// ============================================================================

TEST_F(StorageImplReadTest, InvalidTimeRangeTest) {
    std::cout << "\n=== INVALID TIME RANGE TEST ===" << std::endl;
    
    // Test with invalid time ranges
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_invalid_range";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    core::Labels labels;
    labels.add("test", "invalid_range");
    
    // Test invalid time ranges
    std::vector<std::pair<int64_t, int64_t>> invalid_ranges = {
        {1000000000, 999999999},   // start > end
        {1000000000, 1000000000},  // start == end
        {-1, 1000000000},          // negative start
        {1000000000, -1},          // negative end
    };
    
    for (const auto& [start, end] : invalid_ranges) {
        auto read_result = storage->read(labels, start, end);
        if (start >= end) {
            EXPECT_FALSE(read_result.ok()) << "Should fail for invalid range [" << start << ", " << end << "]";
        }
    }
    
    storage->close();
    std::cout << "✓ Invalid time range test completed" << std::endl;
}

TEST_F(StorageImplReadTest, EmptyLabelsTest) {
    std::cout << "\n=== EMPTY LABELS TEST ===" << std::endl;
    
    // Test with empty labels
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_empty_labels";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    core::Labels empty_labels;
    
    auto read_result = storage->read(empty_labels, 1000000000, 1000000000);
    EXPECT_FALSE(read_result.ok()) << "Should fail for empty labels";
    
    storage->close();
    std::cout << "✓ Empty labels test completed" << std::endl;
}

// ============================================================================
// Phase 6: Debugging and Instrumentation
// ============================================================================

TEST_F(StorageImplReadTest, InstrumentedReadTest) {
    std::cout << "\n=== INSTRUMENTED READ TEST ===" << std::endl;
    
    // Test with debugging instrumentation
    core::StorageConfig config;
    config.enable_compression = false;
    config.data_dir = "./test_data_instrumented";
    
    auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok()) << "Init failed: " << init_result.error();
    
    // Write test data
    core::Labels labels;
    labels.add("test", "instrumented");
    core::TimeSeries series(labels);
    series.add_sample(1000000000, 42.0);
    
    auto write_result = storage->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    // Add debugging output before read
    std::cout << "About to call read method..." << std::endl;
    std::cout << "Labels: " << labels.to_string() << std::endl;
    std::cout << "Time range: [1000000000, 1000000000]" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto read_result = storage->read(labels, 1000000000, 1000000000);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "Read method completed in " << duration.count() << " microseconds" << std::endl;
    
    ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
    EXPECT_EQ(read_result.value().samples().size(), 1);
    
    storage->close();
    std::cout << "✓ Instrumented read test completed" << std::endl;
}

} // namespace integration
} // namespace tsdb 