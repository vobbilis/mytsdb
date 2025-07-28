#include <gtest/gtest.h>
#include "tsdb/storage/atomic_metrics.h"
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <future>

using namespace tsdb::storage::internal;

class AtomicMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        metrics_ = std::make_unique<AtomicMetrics>();
    }
    
    void TearDown() override {
        metrics_.reset();
    }
    
    std::unique_ptr<AtomicMetrics> metrics_;
};

TEST_F(AtomicMetricsTest, BasicWriteReadTracking) {
    // Test basic write and read tracking
    metrics_->recordWrite(1024, 1000);
    metrics_->recordWrite(2048, 2000);
    metrics_->recordRead(512, 500);
    metrics_->recordRead(1024, 1500);
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.write_count, 2);
    EXPECT_EQ(snapshot.bytes_written, 3072);  // 1024 + 2048
    EXPECT_EQ(snapshot.total_write_time, 3000);  // 1000 + 2000
    
    EXPECT_EQ(snapshot.read_count, 2);
    EXPECT_EQ(snapshot.bytes_read, 1536);  // 512 + 1024
    EXPECT_EQ(snapshot.total_read_time, 2000);  // 500 + 1500
}

TEST_F(AtomicMetricsTest, CacheHitMissTracking) {
    // Test cache hit/miss tracking
    metrics_->recordCacheHit();
    metrics_->recordCacheHit();
    metrics_->recordCacheMiss();
    metrics_->recordCacheHit();
    metrics_->recordCacheMiss();
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.cache_hits, 3);
    EXPECT_EQ(snapshot.cache_misses, 2);
    EXPECT_DOUBLE_EQ(snapshot.cache_hit_ratio, 0.6);  // 3/(3+2) = 0.6
}

TEST_F(AtomicMetricsTest, CompressionTracking) {
    // Test compression tracking
    metrics_->recordCompression(1000, 500, 1000);   // 2:1 compression
    metrics_->recordCompression(2000, 1000, 2000);  // 2:1 compression
    metrics_->recordDecompression(500, 1000, 500);
    metrics_->recordDecompression(1000, 2000, 1000);
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.compression_count, 2);
    EXPECT_EQ(snapshot.decompression_count, 2);
    EXPECT_EQ(snapshot.bytes_compressed, 3000);      // 1000 + 2000
    EXPECT_EQ(snapshot.bytes_decompressed, 3000);    // 1000 + 2000
    EXPECT_EQ(snapshot.total_compression_time, 3000);
    EXPECT_EQ(snapshot.total_decompression_time, 1500);
    EXPECT_DOUBLE_EQ(snapshot.average_compression_ratio, 1.0);  // 3000/3000
}

TEST_F(AtomicMetricsTest, MemoryTracking) {
    // Test memory allocation/deallocation tracking
    metrics_->recordAllocation(1024);
    metrics_->recordAllocation(2048);
    metrics_->recordDeallocation(512);
    metrics_->recordAllocation(4096);
    metrics_->recordDeallocation(1024);
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.allocation_count, 3);
    EXPECT_EQ(snapshot.deallocation_count, 2);
    EXPECT_EQ(snapshot.bytes_allocated, 7168);       // 1024 + 2048 + 4096
    EXPECT_EQ(snapshot.bytes_deallocated, 1536);     // 512 + 1024
    EXPECT_EQ(snapshot.net_memory_usage, 5632);      // 7168 - 1536
}

TEST_F(AtomicMetricsTest, PerformanceCalculations) {
    // Test performance calculations
    metrics_->recordWrite(1000, 1000);   // 1MB in 1μs
    metrics_->recordWrite(2000, 2000);   // 2MB in 2μs
    metrics_->recordRead(500, 500);      // 0.5MB in 0.5μs
    metrics_->recordRead(1500, 1500);    // 1.5MB in 1.5μs
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_DOUBLE_EQ(snapshot.average_write_latency_ns, 1500.0);  // (1000+2000)/2
    EXPECT_DOUBLE_EQ(snapshot.average_read_latency_ns, 1000.0);   // (500+1500)/2
    
    // Throughput calculations (simplified for test)
    // Write: 3000 bytes / 3000 ns = 1 byte/ns = 1000 MB/s
    // Read: 2000 bytes / 2000 ns = 1 byte/ns = 1000 MB/s
    EXPECT_GT(snapshot.write_throughput_mbps, 0.0);
    EXPECT_GT(snapshot.read_throughput_mbps, 0.0);
}

TEST_F(AtomicMetricsTest, ConfigurationControl) {
    // Test configuration control
    AtomicMetricsConfig config;
    config.enable_tracking = false;
    config.enable_timing = false;
    config.enable_cache_metrics = false;
    config.enable_compression_metrics = false;
    
    AtomicMetrics disabled_metrics(config);
    
    // These should be no-ops
    disabled_metrics.recordWrite(1000, 1000);
    disabled_metrics.recordRead(1000, 1000);
    disabled_metrics.recordCacheHit();
    disabled_metrics.recordCacheMiss();
    disabled_metrics.recordCompression(1000, 500, 1000);
    
    auto snapshot = disabled_metrics.getSnapshot();
    
    EXPECT_EQ(snapshot.write_count, 0);
    EXPECT_EQ(snapshot.read_count, 0);
    EXPECT_EQ(snapshot.cache_hits, 0);
    EXPECT_EQ(snapshot.cache_misses, 0);
    EXPECT_EQ(snapshot.compression_count, 0);
}

TEST_F(AtomicMetricsTest, ResetFunctionality) {
    // Test reset functionality
    metrics_->recordWrite(1000, 1000);
    metrics_->recordRead(1000, 1000);
    metrics_->recordCacheHit();
    metrics_->recordCompression(1000, 500, 1000);
    metrics_->recordAllocation(1000);
    
    auto snapshot_before = metrics_->getSnapshot();
    EXPECT_GT(snapshot_before.write_count, 0);
    EXPECT_GT(snapshot_before.read_count, 0);
    EXPECT_GT(snapshot_before.cache_hits, 0);
    EXPECT_GT(snapshot_before.compression_count, 0);
    EXPECT_GT(snapshot_before.allocation_count, 0);
    
    metrics_->reset();
    
    auto snapshot_after = metrics_->getSnapshot();
    EXPECT_EQ(snapshot_after.write_count, 0);
    EXPECT_EQ(snapshot_after.read_count, 0);
    EXPECT_EQ(snapshot_after.cache_hits, 0);
    EXPECT_EQ(snapshot_after.cache_misses, 0);
    EXPECT_EQ(snapshot_after.compression_count, 0);
    EXPECT_EQ(snapshot_after.decompression_count, 0);
    EXPECT_EQ(snapshot_after.allocation_count, 0);
    EXPECT_EQ(snapshot_after.deallocation_count, 0);
    EXPECT_EQ(snapshot_after.bytes_written, 0);
    EXPECT_EQ(snapshot_after.bytes_read, 0);
    EXPECT_EQ(snapshot_after.bytes_compressed, 0);
    EXPECT_EQ(snapshot_after.bytes_decompressed, 0);
    EXPECT_EQ(snapshot_after.bytes_allocated, 0);
    EXPECT_EQ(snapshot_after.bytes_deallocated, 0);
    EXPECT_EQ(snapshot_after.total_write_time, 0);
    EXPECT_EQ(snapshot_after.total_read_time, 0);
    EXPECT_EQ(snapshot_after.total_compression_time, 0);
    EXPECT_EQ(snapshot_after.total_decompression_time, 0);
}

TEST_F(AtomicMetricsTest, ThreadSafety) {
    // Test thread safety with multiple threads
    const int num_threads = 4;
    const int operations_per_thread = 1000;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, operations_per_thread, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                metrics_->recordWrite(100 + j, 100 + j);
                metrics_->recordRead(50 + j, 50 + j);
                if (j % 2 == 0) {
                    metrics_->recordCacheHit();
                } else {
                    metrics_->recordCacheMiss();
                }
                metrics_->recordCompression(200 + j, 100 + j, 200 + j);
                metrics_->recordAllocation(100 + j);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.write_count, num_threads * operations_per_thread);
    EXPECT_EQ(snapshot.read_count, num_threads * operations_per_thread);
    EXPECT_EQ(snapshot.cache_hits, num_threads * operations_per_thread / 2);
    EXPECT_EQ(snapshot.cache_misses, num_threads * operations_per_thread / 2);
    EXPECT_EQ(snapshot.compression_count, num_threads * operations_per_thread);
    EXPECT_EQ(snapshot.allocation_count, num_threads * operations_per_thread);
}

TEST_F(AtomicMetricsTest, ScopedTimer) {
    // Test ScopedTimer functionality
    {
        ScopedTimer timer(*metrics_, "write");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        timer.stop(1024);  // 1024 bytes written
    }
    
    {
        ScopedTimer timer(*metrics_, "read");
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        timer.stop(512);   // 512 bytes read
    }
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.write_count, 1);
    EXPECT_EQ(snapshot.bytes_written, 1024);
    EXPECT_GT(snapshot.total_write_time, 0);
    
    EXPECT_EQ(snapshot.read_count, 1);
    EXPECT_EQ(snapshot.bytes_read, 512);
    EXPECT_GT(snapshot.total_read_time, 0);
}

TEST_F(AtomicMetricsTest, GlobalMetrics) {
    // Test GlobalMetrics functionality
    AtomicMetricsConfig config;
    config.enable_tracking = true;
    config.enable_timing = true;
    
    GlobalMetrics::initialize(config);
    
    GlobalMetrics::getInstance().recordWrite(1000, 1000);
    GlobalMetrics::getInstance().recordRead(1000, 1000);
    GlobalMetrics::getInstance().recordCacheHit();
    
    auto snapshot = GlobalMetrics::getSnapshot();
    EXPECT_EQ(snapshot.write_count, 1);
    EXPECT_EQ(snapshot.read_count, 1);
    EXPECT_EQ(snapshot.cache_hits, 1);
    
    std::string formatted = GlobalMetrics::getFormattedMetrics();
    EXPECT_FALSE(formatted.empty());
    EXPECT_NE(formatted.find("TSDB Storage Metrics"), std::string::npos);
    
    std::string json = GlobalMetrics::getJsonMetrics();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"operations\""), std::string::npos);
    
    GlobalMetrics::reset();
    
    auto snapshot_after_reset = GlobalMetrics::getSnapshot();
    EXPECT_EQ(snapshot_after_reset.write_count, 0);
    EXPECT_EQ(snapshot_after_reset.read_count, 0);
    EXPECT_EQ(snapshot_after_reset.cache_hits, 0);
}

TEST_F(AtomicMetricsTest, FormattedOutput) {
    // Test formatted output
    metrics_->recordWrite(1024, 1000);
    metrics_->recordRead(512, 500);
    metrics_->recordCacheHit();
    metrics_->recordCacheMiss();
    metrics_->recordCompression(1000, 500, 1000);
    metrics_->recordAllocation(1024);
    
    std::string formatted = metrics_->getFormattedMetrics();
    EXPECT_FALSE(formatted.empty());
    EXPECT_NE(formatted.find("TSDB Storage Metrics"), std::string::npos);
    EXPECT_NE(formatted.find("Operations:"), std::string::npos);
    EXPECT_NE(formatted.find("Compression:"), std::string::npos);
    EXPECT_NE(formatted.find("Memory:"), std::string::npos);
    
    std::string json = metrics_->getJsonMetrics();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"operations\""), std::string::npos);
    EXPECT_NE(json.find("\"data_volumes\""), std::string::npos);
    EXPECT_NE(json.find("\"compression\""), std::string::npos);
    EXPECT_NE(json.find("\"memory\""), std::string::npos);
}

TEST_F(AtomicMetricsTest, EdgeCases) {
    // Test edge cases
    metrics_->recordWrite(0, 0);
    metrics_->recordRead(0, 0);
    metrics_->recordCompression(0, 0, 0);
    metrics_->recordDecompression(0, 0, 0);
    metrics_->recordAllocation(0);
    metrics_->recordDeallocation(0);
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.write_count, 1);
    EXPECT_EQ(snapshot.read_count, 1);
    EXPECT_EQ(snapshot.compression_count, 1);
    EXPECT_EQ(snapshot.decompression_count, 1);
    EXPECT_EQ(snapshot.allocation_count, 1);
    EXPECT_EQ(snapshot.deallocation_count, 1);
    
    // Test with very large values
    metrics_->recordWrite(UINT64_MAX, UINT64_MAX);
    metrics_->recordRead(UINT64_MAX, UINT64_MAX);
    
    auto snapshot_large = metrics_->getSnapshot();
    EXPECT_EQ(snapshot_large.write_count, 2);  // 1 + 1
    EXPECT_EQ(snapshot_large.read_count, 2);   // 1 + 1
}

TEST_F(AtomicMetricsTest, MemoryOrdering) {
    // Test different memory ordering configurations
    AtomicMetricsConfig relaxed_config;
    relaxed_config.use_relaxed_ordering = true;
    
    AtomicMetricsConfig strict_config;
    strict_config.use_relaxed_ordering = false;
    
    AtomicMetrics relaxed_metrics(relaxed_config);
    AtomicMetrics strict_metrics(strict_config);
    
    // Both should work correctly
    relaxed_metrics.recordWrite(1000, 1000);
    strict_metrics.recordWrite(1000, 1000);
    
    auto relaxed_snapshot = relaxed_metrics.getSnapshot();
    auto strict_snapshot = strict_metrics.getSnapshot();
    
    EXPECT_EQ(relaxed_snapshot.write_count, 1);
    EXPECT_EQ(strict_snapshot.write_count, 1);
    EXPECT_EQ(relaxed_snapshot.bytes_written, 1000);
    EXPECT_EQ(strict_snapshot.bytes_written, 1000);
}

TEST_F(AtomicMetricsTest, PerformanceBenchmark) {
    // Performance benchmark test
    const int num_operations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        metrics_->recordWrite(100 + i, 100 + i);
        metrics_->recordRead(50 + i, 50 + i);
        if (i % 2 == 0) {
            metrics_->recordCacheHit();
        } else {
            metrics_->recordCacheMiss();
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.write_count, num_operations);
    EXPECT_EQ(snapshot.read_count, num_operations);
    EXPECT_EQ(snapshot.cache_hits, num_operations / 2);
    EXPECT_EQ(snapshot.cache_misses, num_operations / 2);
    
    // Performance should be reasonable (less than 1μs per operation)
    double avg_time_per_op = static_cast<double>(duration.count()) / (num_operations * 4);
    EXPECT_LT(avg_time_per_op, 1.0);  // Less than 1μs per operation
}

TEST_F(AtomicMetricsTest, ConcurrentAccess) {
    // Test concurrent access with async operations
    const int num_operations = 10000;
    
    auto write_future = std::async(std::launch::async, [this, num_operations]() {
        for (int i = 0; i < num_operations; ++i) {
            metrics_->recordWrite(100 + i, 100 + i);
        }
    });
    
    auto read_future = std::async(std::launch::async, [this, num_operations]() {
        for (int i = 0; i < num_operations; ++i) {
            metrics_->recordRead(50 + i, 50 + i);
        }
    });
    
    auto cache_future = std::async(std::launch::async, [this, num_operations]() {
        for (int i = 0; i < num_operations; ++i) {
            if (i % 2 == 0) {
                metrics_->recordCacheHit();
            } else {
                metrics_->recordCacheMiss();
            }
        }
    });
    
    write_future.wait();
    read_future.wait();
    cache_future.wait();
    
    auto snapshot = metrics_->getSnapshot();
    
    EXPECT_EQ(snapshot.write_count, num_operations);
    EXPECT_EQ(snapshot.read_count, num_operations);
    EXPECT_EQ(snapshot.cache_hits, num_operations / 2);
    EXPECT_EQ(snapshot.cache_misses, num_operations / 2);
}

TEST_F(AtomicMetricsTest, ConfigurationUpdate) {
    // Test configuration update
    AtomicMetricsConfig initial_config;
    initial_config.enable_tracking = true;
    initial_config.enable_timing = true;
    initial_config.enable_cache_metrics = true;
    initial_config.enable_compression_metrics = true;
    
    AtomicMetrics metrics(initial_config);
    
    // Record some operations
    metrics.recordWrite(1000, 1000);
    metrics.recordCacheHit();
    metrics.recordCompression(1000, 500, 1000);
    
    // Update configuration to disable some features
    AtomicMetricsConfig new_config;
    new_config.enable_tracking = true;
    new_config.enable_timing = false;
    new_config.enable_cache_metrics = false;
    new_config.enable_compression_metrics = false;
    
    metrics.updateConfig(new_config);
    
    // These should still work
    metrics.recordWrite(1000, 1000);
    metrics.recordRead(1000, 1000);
    
    // These should be no-ops
    metrics.recordCacheHit();
    metrics.recordCacheMiss();
    metrics.recordCompression(1000, 500, 1000);
    
    auto snapshot = metrics.getSnapshot();
    
    EXPECT_EQ(snapshot.write_count, 2);  // 1 + 1
    EXPECT_EQ(snapshot.read_count, 1);
    EXPECT_EQ(snapshot.cache_hits, 1);   // Only the first one
    EXPECT_EQ(snapshot.cache_misses, 0);
    EXPECT_EQ(snapshot.compression_count, 1);  // Only the first one
}

TEST_F(AtomicMetricsTest, ZeroOverheadWhenDisabled) {
    // Test that metrics have zero overhead when disabled
    AtomicMetricsConfig disabled_config;
    disabled_config.enable_tracking = false;
    
    AtomicMetrics disabled_metrics(disabled_config);
    
    const int num_operations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        disabled_metrics.recordWrite(100 + i, 100 + i);
        disabled_metrics.recordRead(50 + i, 50 + i);
        disabled_metrics.recordCacheHit();
        disabled_metrics.recordCacheMiss();
        disabled_metrics.recordCompression(1000 + i, 500 + i, 1000 + i);
        disabled_metrics.recordAllocation(100 + i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    auto snapshot = disabled_metrics.getSnapshot();
    
    // All metrics should be zero
    EXPECT_EQ(snapshot.write_count, 0);
    EXPECT_EQ(snapshot.read_count, 0);
    EXPECT_EQ(snapshot.cache_hits, 0);
    EXPECT_EQ(snapshot.cache_misses, 0);
    EXPECT_EQ(snapshot.compression_count, 0);
    EXPECT_EQ(snapshot.allocation_count, 0);
    
    // Performance should be very fast (near-zero overhead)
    double avg_time_per_op = static_cast<double>(duration.count()) / (num_operations * 6);
    EXPECT_LT(avg_time_per_op, 10.0);  // Less than 10ns per operation
} 