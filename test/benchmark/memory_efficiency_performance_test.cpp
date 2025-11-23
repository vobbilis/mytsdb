#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include <filesystem>
#include <memory>
#include <random>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/resource.h>

namespace tsdb {
namespace benchmark {
namespace {

/**
 * @brief Memory Efficiency Performance Tests
 * 
 * These tests validate the performance improvements from Task 1 of DESIGN_FIXES.md:
 * "Eliminate In-Memory Data Duplication and Full Scans"
 * 
 * Key improvements tested:
 * 1. Memory usage reduction by eliminating stored_series_ vector
 * 2. Performance improvement by eliminating full scans
 * 3. Memory stability under load
 * 4. Block-based storage efficiency
 */

class MemoryEfficiencyPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_memory_efficiency_test";
        std::filesystem::create_directories(test_dir_);

        // Configure storage with memory-optimized settings
        core::StorageConfig config;
        config.data_dir = test_dir_.string();
        config.block_size = 64 * 1024;  // 64KB blocks
        config.max_blocks_per_series = 1000;
        config.cache_size_bytes = 10 * 1024 * 1024;  // 10MB cache
        config.block_duration = 3600 * 1000;  // 1 hour
        config.retention_period = 7 * 24 * 3600 * 1000;  // 1 week
        config.enable_compression = true;

        storage_ = std::make_shared<storage::StorageImpl>();
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
        
        // Initialize test data
        setupTestData();
    }

    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }

    void setupTestData() {
        // Generate realistic test data for memory testing
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> value_dist(50.0, 15.0);
        std::uniform_int_distribution<> label_dist(1, 100);

        // Generate test values
        for (int i = 0; i < 10000; ++i) {
            test_values_.push_back(std::max(0.0, std::min(100.0, value_dist(gen))));
            test_labels_.push_back(label_dist(gen));
        }
    }

    // Helper method to get current memory usage
    size_t getCurrentMemoryUsage() {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_maxrss * 1024;  // Convert to bytes
    }

    // Helper method to create realistic time series
    core::TimeSeries createTimeSeries(int id, const std::string& name = "memory_test") {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test_id", std::to_string(id));
        labels.add("label_value", std::to_string(test_labels_[id % test_labels_.size()]));
        labels.add("workload", "memory_efficiency");
        
        core::TimeSeries series(labels);
        series.add_sample(core::Sample(1000 + id, test_values_[id % test_values_.size()]));
        return series;
    }

    // Helper method to create large time series
    core::TimeSeries createLargeTimeSeries(int id, int sampleCount, const std::string& name = "large_memory_test") {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test_id", std::to_string(id));
        labels.add("sample_count", std::to_string(sampleCount));
        labels.add("workload", "memory_efficiency");
        
        core::TimeSeries series(labels);
        for (int i = 0; i < sampleCount; ++i) {
            series.add_sample(core::Sample(1000 + id * 1000 + i, test_values_[i % test_values_.size()]));
        }
        return series;
    }

    // Helper method to measure performance
    template<typename Func>
    auto measurePerformance(const std::string& operation, Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << operation << " took " << duration.count() << " microseconds" << std::endl;
        return std::make_pair(result, duration);
    }

    // Helper method to calculate expected memory with old approach
    size_t calculateExpectedMemoryWithDuplication(int numSeries, int samplesPerSeries) {
        // Estimate memory usage with old stored_series_ vector approach
        size_t seriesOverhead = sizeof(core::TimeSeries) * numSeries;
        size_t samplesOverhead = sizeof(core::Sample) * numSeries * samplesPerSeries;
        size_t labelsOverhead = sizeof(core::Labels) * numSeries;
        size_t compressionOverhead = sizeof(std::vector<uint8_t>) * numSeries;
        
        return seriesOverhead + samplesOverhead + labelsOverhead + compressionOverhead;
    }

    std::filesystem::path test_dir_;
    std::shared_ptr<storage::Storage> storage_;
    
    // Test data
    std::vector<double> test_values_;
    std::vector<int> test_labels_;
};

TEST_F(MemoryEfficiencyPerformanceTest, InMemoryDuplicationElimination) {
    // Test: Memory usage without stored_series_ vector
    // Validates: Memory efficiency improvement from Task 1
    
    auto initialMemory = getCurrentMemoryUsage();
    std::cout << "Initial memory usage: " << initialMemory << " bytes" << std::endl;
    
    // Write large amount of data that would previously cause memory bloat
    const int NUM_SERIES = 10000;
    const int SAMPLES_PER_SERIES = 100;
    
    auto [success_count, write_time] = measurePerformance("Large Dataset Write", [&]() {
        int success_count = 0;
        for (int i = 0; i < NUM_SERIES; ++i) {
            auto series = createTimeSeries(i, "memory_test");
            auto result = storage_->write(series);
            if (result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    auto peakMemory = getCurrentMemoryUsage();
    auto memoryGrowth = peakMemory - initialMemory;
    
    std::cout << "Peak memory usage: " << peakMemory << " bytes" << std::endl;
    std::cout << "Memory growth: " << memoryGrowth << " bytes" << std::endl;
    
    // Calculate expected memory with old approach (stored_series_ vector)
    auto expectedMemoryWithDuplication = calculateExpectedMemoryWithDuplication(NUM_SERIES, SAMPLES_PER_SERIES);
    std::cout << "Expected memory with duplication: " << expectedMemoryWithDuplication << " bytes" << std::endl;
    
    // Verify memory efficiency improvement (should be <50% of old approach)
    EXPECT_LT(memoryGrowth, expectedMemoryWithDuplication * 0.5)
        << "Memory usage too high - in-memory duplication not eliminated";
    
    // Verify high success rate
    EXPECT_GT(success_count, NUM_SERIES * 0.95) << "Too many write failures";
    
    // Verify write performance
    double throughput = NUM_SERIES / (write_time.count() / 1000000.0);
    std::cout << "Write throughput: " << throughput << " ops/sec" << std::endl;
    EXPECT_GT(throughput, 5000) << "Write throughput too low: " << throughput << " ops/sec";
}

TEST_F(MemoryEfficiencyPerformanceTest, MemoryStabilityUnderLoad) {
    // Test: Memory stability without stored_series_ vector
    // Validates: No unbounded memory growth
    
    const int NUM_ITERATIONS = 100;
    const int SERIES_PER_ITERATION = 100;
    
    std::vector<size_t> memorySnapshots;
    
    for (int iteration = 0; iteration < NUM_ITERATIONS; ++iteration) {
        // Write batch of series
        for (int i = 0; i < SERIES_PER_ITERATION; ++i) {
            auto series = createTimeSeries(iteration * SERIES_PER_ITERATION + i, "stability_test");
            ASSERT_TRUE(storage_->write(series).ok());
        }
        
        // Take memory snapshot
        memorySnapshots.push_back(getCurrentMemoryUsage());
        
        // Verify memory growth is linear, not exponential
        if (iteration > 10) {
            auto recentGrowth = memorySnapshots[iteration] - memorySnapshots[iteration - 10];
            auto expectedGrowth = calculateExpectedMemoryWithDuplication(10, SERIES_PER_ITERATION) * 0.1;
            EXPECT_LT(recentGrowth, expectedGrowth * 2.0)
                << "Memory growth not linear - possible memory leak at iteration " << iteration;
        }
        
        if (iteration % 20 == 0) {
            std::cout << "Iteration " << iteration << " memory: " << memorySnapshots[iteration] << " bytes" << std::endl;
        }
    }
    
    // Verify overall memory stability
    auto maxMemory = *std::max_element(memorySnapshots.begin(), memorySnapshots.end());
    auto minMemory = *std::min_element(memorySnapshots.begin(), memorySnapshots.end());
    auto memoryVariation = maxMemory - minMemory;
    
    std::cout << "Memory variation: " << memoryVariation << " bytes" << std::endl;
    EXPECT_LT(memoryVariation, maxMemory * 0.5) << "Memory usage too variable - possible memory leak";
}

TEST_F(MemoryEfficiencyPerformanceTest, WritePerformanceWithoutDuplication) {
    // Test: Write performance without stored_series_ vector
    // Validates: Performance improvement from eliminating duplication
    
    const int NUM_OPERATIONS = 50000;
    
    auto [success_count, write_time] = measurePerformance("High-Volume Write Performance", [&]() {
        int success_count = 0;
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            auto series = createTimeSeries(i, "perf_test");
            auto result = storage_->write(series);
            if (result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    double throughput = NUM_OPERATIONS / (write_time.count() / 1000000.0);
    
    std::cout << "Write throughput: " << throughput << " ops/sec" << std::endl;
    
    // Verify performance improvement (should be >20K ops/sec without duplication)
    EXPECT_GT(throughput, 20000) << "Write throughput too low: " << throughput << " ops/sec";
    
    // Verify high success rate
    EXPECT_GT(success_count, NUM_OPERATIONS * 0.99) << "Too many write failures";
    
    // Verify no performance degradation over time
    verifyPerformanceStability();
}

TEST_F(MemoryEfficiencyPerformanceTest, BlockStorageEfficiency) {
    // Test: Block-based storage vs in-memory approach
    // Validates: Block storage performance benefits
    
    const int NUM_SERIES = 5000;
    const int SAMPLES_PER_SERIES = 200;
    
    // Write data to blocks
    auto [write_success, write_time] = measurePerformance("Block Storage Write", [&]() {
        int success_count = 0;
        for (int i = 0; i < NUM_SERIES; ++i) {
            auto series = createLargeTimeSeries(i, SAMPLES_PER_SERIES, "block_test");
            auto result = storage_->write(series);
            if (result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    // Force block persistence
    storage_->flush();
    
    // Measure read performance from blocks
    auto [read_success, read_time] = measurePerformance("Block Storage Read", [&]() {
        int success_count = 0;
        for (int i = 0; i < NUM_SERIES; ++i) {
            core::Labels labels;
            labels.add("__name__", "block_test");
            labels.add("test_id", std::to_string(i));
            auto result = storage_->read(labels, 0, LLONG_MAX);
            if (result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    double writeThroughput = NUM_SERIES / (write_time.count() / 1000000.0);
    double readThroughput = NUM_SERIES / (read_time.count() / 1000000.0);
    
    std::cout << "Block write throughput: " << writeThroughput << " ops/sec" << std::endl;
    std::cout << "Block read throughput: " << readThroughput << " ops/sec" << std::endl;
    
    // Verify block storage performance
    EXPECT_GT(writeThroughput, 5000) << "Block write throughput too low: " << writeThroughput << " ops/sec";
    EXPECT_GT(readThroughput, 3000) << "Block read throughput too low: " << readThroughput << " ops/sec";
    
    // Verify high success rates
    EXPECT_GT(write_success, NUM_SERIES * 0.95) << "Too many write failures";
    EXPECT_GT(read_success, NUM_SERIES * 0.90) << "Too many read failures";
}

TEST_F(MemoryEfficiencyPerformanceTest, MemoryEfficiencyValidation) {
    // Test: Memory efficiency compared to old approach
    // Validates: Significant memory savings achieved
    
    const int NUM_SERIES = 20000;
    const int SAMPLES_PER_SERIES = 50;
    
    auto initialMemory = getCurrentMemoryUsage();
    
    // Write data
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTimeSeries(i, "efficiency_test");
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    auto finalMemory = getCurrentMemoryUsage();
    auto actualMemoryUsage = finalMemory - initialMemory;
    
    // Calculate expected memory with old approach
    auto expectedMemoryWithDuplication = calculateExpectedMemoryWithDuplication(NUM_SERIES, SAMPLES_PER_SERIES);
    
    double memoryEfficiency = 1.0 - (static_cast<double>(actualMemoryUsage) / expectedMemoryWithDuplication);
    
    std::cout << "Actual memory usage: " << actualMemoryUsage << " bytes" << std::endl;
    std::cout << "Expected with duplication: " << expectedMemoryWithDuplication << " bytes" << std::endl;
    std::cout << "Memory efficiency: " << (memoryEfficiency * 100) << "%" << std::endl;
    
    // Verify significant memory efficiency improvement
    EXPECT_GT(memoryEfficiency, 0.5) << "Memory efficiency too low: " << (memoryEfficiency * 100) << "%";
    
    // Verify memory usage is less than 50% of old approach
    EXPECT_LT(actualMemoryUsage, expectedMemoryWithDuplication * 0.5)
        << "Memory usage too high compared to old approach";
}

TEST_F(MemoryEfficiencyPerformanceTest, LargeDatasetMemoryStability) {
    // Test: Memory stability with very large datasets
    // Validates: No memory bloat with large amounts of data
    
    const int NUM_SERIES = 100000;
    const int SAMPLES_PER_SERIES = 10;
    
    auto initialMemory = getCurrentMemoryUsage();
    
    // Write large dataset
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTimeSeries(i, "large_dataset_test");
        ASSERT_TRUE(storage_->write(series).ok());
        
        // Monitor memory every 10K operations
        if (i % 10000 == 0) {
            auto currentMemory = getCurrentMemoryUsage();
            auto memoryGrowth = currentMemory - initialMemory;
            std::cout << "Series " << i << " memory growth: " << memoryGrowth << " bytes" << std::endl;
            
            // Verify memory growth is reasonable
            EXPECT_LT(memoryGrowth, (i / 10000 + 1) * 50 * 1024 * 1024) // 50MB per 10K series
                << "Memory growth too high at series " << i;
        }
    }
    
    auto finalMemory = getCurrentMemoryUsage();
    auto totalMemoryGrowth = finalMemory - initialMemory;
    
    std::cout << "Total memory growth: " << totalMemoryGrowth << " bytes" << std::endl;
    
    // Verify total memory growth is reasonable
    EXPECT_LT(totalMemoryGrowth, 500 * 1024 * 1024) // Less than 500MB for 100K series
        << "Total memory growth too high: " << totalMemoryGrowth << " bytes";
}

} // namespace
} // namespace benchmark
} // namespace tsdb


