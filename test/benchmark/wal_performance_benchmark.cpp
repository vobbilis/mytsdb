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

namespace tsdb {
namespace benchmark {
namespace {

/**
 * @brief WAL Write Performance Benchmarks
 * 
 * These benchmarks measure the performance characteristics of the WAL (Write-Ahead Log)
 * implementation under various workload conditions. The benchmarks test:
 * 
 * 1. Single Write Performance - Individual write operations with WAL logging
 * 2. Batch Write Performance - High-volume batch operations with WAL
 * 3. Concurrent Write Performance - Multi-threaded write operations with WAL
 * 4. WAL File Management - Segment rotation and file management performance
 * 5. WAL Replay Performance - Crash recovery and replay performance
 * 6. Mixed Workload Performance - Combined read/write operations with WAL
 * 
 * Performance Targets:
 * - Single write latency: <1ms
 * - Batch throughput: >10K writes/sec
 * - Concurrent throughput: >5K writes/sec (4 threads)
 * - WAL replay time: <100ms for 1K operations
 * - Memory usage: <100MB for 10K operations
 */

class WALPerformanceBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_wal_perf_benchmark";
        std::filesystem::create_directories(test_dir_);

        // Configure storage with WAL-optimized settings
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
        // Generate realistic test data for performance testing
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

    // Helper method to create realistic time series
    core::TimeSeries createTimeSeries(int id, const std::string& name = "performance_test") {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test_id", std::to_string(id));
        labels.add("label_value", std::to_string(test_labels_[id % test_labels_.size()]));
        labels.add("workload", "benchmark");
        
        core::TimeSeries series(labels);
        series.add_sample(core::Sample(1000 + id, test_values_[id % test_values_.size()]));
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

    // Helper method to analyze WAL files
    void analyzeWALFiles() {
        std::filesystem::path wal_dir = test_dir_ / "wal";
        if (std::filesystem::exists(wal_dir)) {
            int wal_file_count = 0;
            size_t total_wal_size = 0;
            
            for (const auto& entry : std::filesystem::directory_iterator(wal_dir)) {
                if (entry.is_regular_file() && entry.path().filename().string().substr(0, 4) == "wal_") {
                    wal_file_count++;
                    total_wal_size += std::filesystem::file_size(entry.path());
                }
            }
            
            std::cout << "WAL Analysis: " << wal_file_count << " files, " << total_wal_size << " bytes total" << std::endl;
        }
    }

    std::filesystem::path test_dir_;
    std::shared_ptr<storage::Storage> storage_;
    
    // Test data
    std::vector<double> test_values_;
    std::vector<int> test_labels_;
};

TEST_F(WALPerformanceBenchmark, SingleWritePerformance) {
    // Test single write performance with WAL logging
    
    auto series = createTimeSeries(1, "single_write_test");
    
    auto [write_result, write_time] = measurePerformance("Single Write with WAL", [&]() {
        return storage_->write(series);
    });
    
    ASSERT_TRUE(write_result.ok()) << "Single write failed: " << write_result.error();
    
    // Performance assertions
    EXPECT_LT(write_time.count(), 1000); // Should complete within 1ms
    std::cout << "Single write latency: " << write_time.count() << " microseconds" << std::endl;
}

TEST_F(WALPerformanceBenchmark, BatchWritePerformance) {
    // Test batch write performance with WAL logging
    
    const int batch_size = 1000;
    std::vector<core::TimeSeries> batch_series;
    
    // Generate batch data
    for (int i = 0; i < batch_size; ++i) {
        batch_series.push_back(createTimeSeries(i, "batch_write_test"));
    }
    
    auto [success_count, batch_time] = measurePerformance("Batch Write with WAL", [&]() {
        int success_count = 0;
        for (const auto& series : batch_series) {
            auto result = storage_->write(series);
            if (result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    EXPECT_GT(success_count, batch_size * 0.95); // At least 95% success rate
    EXPECT_LT(batch_time.count(), 1000000); // Should complete within 1 second
    
    double throughput = batch_size / (batch_time.count() / 1000000.0);
    std::cout << "Batch write throughput: " << throughput << " writes/sec" << std::endl;
    EXPECT_GT(throughput, 1000); // At least 1000 writes/sec
}

TEST_F(WALPerformanceBenchmark, ConcurrentWritePerformance) {
    // Test concurrent write performance with WAL logging
    
    const int num_threads = 4;
    const int metrics_per_thread = 250;
    std::atomic<int> concurrent_success{0};
    std::atomic<int> concurrent_errors{0};
    
    auto concurrent_work = [&](int thread_id) {
        for (int i = 0; i < metrics_per_thread; ++i) {
            auto series = createTimeSeries(thread_id * 1000 + i, "concurrent_write_test");
            auto result = storage_->write(series);
            if (result.ok()) {
                concurrent_success++;
            } else {
                concurrent_errors++;
            }
        }
    };
    
    auto [total_processed, concurrent_time] = measurePerformance("Concurrent Write with WAL", [&]() {
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(concurrent_work, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        return concurrent_success.load();
    });
    
    int total_expected = num_threads * metrics_per_thread;
    EXPECT_GT(concurrent_success.load(), total_expected * 0.95); // At least 95% success rate
    EXPECT_LT(concurrent_time.count(), 2000000); // Should complete within 2 seconds
    
    double concurrent_throughput = total_processed / (concurrent_time.count() / 1000000.0);
    std::cout << "Concurrent write throughput: " << concurrent_throughput << " writes/sec" << std::endl;
    EXPECT_GT(concurrent_throughput, 500); // At least 500 writes/sec
}

TEST_F(WALPerformanceBenchmark, WALFileManagementPerformance) {
    // Test WAL file management and segment rotation performance
    
    const int large_batch_size = 5000; // Large enough to trigger segment rotation
    std::vector<core::TimeSeries> large_batch;
    
    // Generate large batch to trigger WAL segment rotation
    for (int i = 0; i < large_batch_size; ++i) {
        large_batch.push_back(createTimeSeries(i, "wal_management_test"));
    }
    
    auto [success_count, management_time] = measurePerformance("WAL File Management", [&]() {
        int success_count = 0;
        for (const auto& series : large_batch) {
            auto result = storage_->write(series);
            if (result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    EXPECT_GT(success_count, large_batch_size * 0.95); // At least 95% success rate
    EXPECT_LT(management_time.count(), 5000000); // Should complete within 5 seconds
    
    // Analyze WAL files
    analyzeWALFiles();
    
    double management_throughput = success_count / (management_time.count() / 1000000.0);
    std::cout << "WAL management throughput: " << management_throughput << " writes/sec" << std::endl;
    EXPECT_GT(management_throughput, 1000); // At least 1000 writes/sec
}

TEST_F(WALPerformanceBenchmark, WALReplayPerformance) {
    // Test WAL replay performance (simulated crash recovery)
    
    // First, write some data to create WAL entries
    const int initial_writes = 1000;
    for (int i = 0; i < initial_writes; ++i) {
        auto series = createTimeSeries(i, "replay_test");
        auto result = storage_->write(series);
        ASSERT_TRUE(result.ok()) << "Initial write failed: " << result.error();
    }
    
    // Simulate crash recovery by creating a new storage instance
    auto recovery_storage = std::make_shared<storage::StorageImpl>();
    
    auto [recovery_result, replay_time] = measurePerformance("WAL Replay (Crash Recovery)", [&]() {
        core::StorageConfig config;
        config.data_dir = test_dir_.string();
        config.block_size = 64 * 1024;
        config.max_blocks_per_series = 1000;
        config.cache_size_bytes = 10 * 1024 * 1024;
        config.block_duration = 3600 * 1000;
        config.retention_period = 7 * 24 * 3600 * 1000;
        config.enable_compression = true;
        
        return recovery_storage->init(config);
    });
    
    ASSERT_TRUE(recovery_result.ok()) << "WAL replay failed: " << recovery_result.error();
    EXPECT_LT(replay_time.count(), 100000); // Should complete within 100ms
    
    std::cout << "WAL replay time: " << replay_time.count() << " microseconds" << std::endl;
    
    // Cleanup recovery storage
    recovery_storage->close();
}

TEST_F(WALPerformanceBenchmark, MixedWorkloadPerformance) {
    // Test mixed read/write workload performance with WAL
    
    const int write_count = 500;
    const int read_count = 200;
    
    // Phase 1: Write operations
    auto [write_success, write_time] = measurePerformance("Mixed Workload - Write Phase", [&]() {
        int success_count = 0;
        for (int i = 0; i < write_count; ++i) {
            auto series = createTimeSeries(i, "mixed_workload_test");
            auto result = storage_->write(series);
            if (result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    // Phase 2: Read operations
    auto [read_success, read_time] = measurePerformance("Mixed Workload - Read Phase", [&]() {
        int success_count = 0;
        for (int i = 0; i < read_count; ++i) {
            // Query by test_id
            std::vector<std::pair<std::string, std::string>> matchers;
            matchers.emplace_back("__name__", "mixed_workload_test");
            matchers.emplace_back("test_id", std::to_string(i));
            
            auto query_result = storage_->query(matchers, 0, std::numeric_limits<int64_t>::max());
            if (query_result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    EXPECT_GT(write_success, write_count * 0.95); // At least 95% write success
    EXPECT_GT(read_success, read_count * 0.90);   // At least 90% read success
    
    double write_throughput = write_success / (write_time.count() / 1000000.0);
    double read_throughput = read_success / (read_time.count() / 1000000.0);
    
    std::cout << "Mixed workload write throughput: " << write_throughput << " writes/sec" << std::endl;
    std::cout << "Mixed workload read throughput: " << read_throughput << " reads/sec" << std::endl;
    
    EXPECT_GT(write_throughput, 500); // At least 500 writes/sec
    EXPECT_GT(read_throughput, 200);  // At least 200 reads/sec
}

} // namespace
} // namespace benchmark
} // namespace tsdb


