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
 * @brief Concurrency Performance Tests
 * 
 * These tests validate the performance improvements from Task 4 of DESIGN_FIXES.md:
 * "Refine Locking with Concurrent Data Structures"
 * 
 * Key improvements tested:
 * 1. Concurrent write performance with tbb::concurrent_hash_map
 * 2. Mixed concurrent read/write performance
 * 3. Lock contention elimination
 * 4. Fine-grained locking performance
 * 5. Scalability with thread count
 */

class ConcurrencyPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_concurrency_test";
        std::filesystem::create_directories(test_dir_);

        // Configure storage with concurrency-optimized settings
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
        // Generate realistic test data for concurrency testing
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> value_dist(50.0, 15.0);
        std::uniform_int_distribution<> label_dist(1, 1000);

        // Generate test values and labels
        for (int i = 0; i < 10000; ++i) {
            test_values_.push_back(std::max(0.0, std::min(100.0, value_dist(gen))));
            test_labels_.push_back(label_dist(gen));
        }
    }

    // Helper method to create time series
    core::TimeSeries createTimeSeries(int id, const std::string& name = "concurrency_test") {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test_id", std::to_string(id));
        labels.add("label_value", std::to_string(test_labels_[id % test_labels_.size()]));
        labels.add("workload", "concurrency");
        
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

    // Helper method to verify throughput scaling
    void verifyThroughputScaling(const std::vector<double>& throughputs, const std::vector<int>& threadCounts) {
        // Verify that throughput generally increases with thread count
        for (size_t i = 1; i < throughputs.size(); ++i) {
            if (threadCounts[i] <= 8) { // Only check up to 8 threads for linear scaling
                EXPECT_GT(throughputs[i], throughputs[i-1] * 0.8) 
                    << "Throughput not scaling well with " << threadCounts[i] << " threads";
            }
        }
    }

    std::filesystem::path test_dir_;
    std::shared_ptr<storage::Storage> storage_;
    
    // Test data
    std::vector<double> test_values_;
    std::vector<int> test_labels_;
};

TEST_F(ConcurrencyPerformanceTest, ConcurrentWritePerformance) {
    // Test: Concurrent write performance with tbb::concurrent_hash_map
    // Validates: Performance improvement from Task 4
    
    const int NUM_THREADS = 8;
    const int OPERATIONS_PER_THREAD = 10000;
    const int TOTAL_OPERATIONS = NUM_THREADS * OPERATIONS_PER_THREAD;
    
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};
    
    auto [total_processed, concurrent_time] = measurePerformance("Concurrent Write Performance", [&]() {
        std::vector<std::thread> threads;
        for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
            threads.emplace_back([&, threadId]() {
                for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                    auto series = createTimeSeries(threadId * OPERATIONS_PER_THREAD + i, "concurrent_test");
                    auto result = storage_->write(series);
                    if (result.ok()) {
                        successCount++;
                    } else {
                        errorCount++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        return successCount.load();
    });
    
    double throughput = TOTAL_OPERATIONS / (concurrent_time.count() / 1000000.0);
    
    std::cout << "Concurrent write throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << "Success count: " << successCount.load() << std::endl;
    std::cout << "Error count: " << errorCount.load() << std::endl;
    
    // Verify concurrent performance (should be >50K ops/sec with 8 threads)
    EXPECT_GT(throughput, 50000) << "Concurrent write throughput too low: " << throughput << " ops/sec";
    
    // Verify high success rate
    EXPECT_GT(successCount.load(), TOTAL_OPERATIONS * 0.99) << "Too many write failures";
    EXPECT_EQ(errorCount.load(), 0) << "Write errors occurred";
}

TEST_F(ConcurrencyPerformanceTest, ConcurrentReadWritePerformance) {
    // Test: Mixed concurrent read/write performance
    // Validates: Concurrent hash map handles mixed workloads efficiently
    
    const int NUM_WRITER_THREADS = 4;
    const int NUM_READER_THREADS = 8;
    const int OPERATIONS_PER_THREAD = 5000;
    
    std::atomic<int> writeCount{0};
    std::atomic<int> readCount{0};
    std::atomic<int> errorCount{0};
    std::atomic<bool> stopTest{false};
    
    // Writer threads
    std::vector<std::thread> writers;
    for (int i = 0; i < NUM_WRITER_THREADS; ++i) {
        writers.emplace_back([&, i]() {
            while (!stopTest.load()) {
                auto series = createTimeSeries(writeCount.load(), "mixed_test");
                auto result = storage_->write(series);
                if (result.ok()) {
                    writeCount++;
                } else {
                    errorCount++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READER_THREADS; ++i) {
        readers.emplace_back([&, i]() {
            while (!stopTest.load()) {
                core::Labels labels;
                labels.add("__name__", "mixed_test");
                labels.add("test_id", std::to_string(readCount.load() % 1000));
                
                auto result = storage_->read(labels, 0, LLONG_MAX);
                if (result.ok() || result.error().what() == std::string("Series not found")) {
                    readCount++;
                } else {
                    errorCount++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Run test for 30 seconds
    std::this_thread::sleep_for(std::chrono::seconds(30));
    stopTest = true;
    
    for (auto& writer : writers) writer.join();
    for (auto& reader : readers) reader.join();
    
    std::cout << "Mixed workload results:" << std::endl;
    std::cout << "  Writes: " << writeCount.load() << std::endl;
    std::cout << "  Reads: " << readCount.load() << std::endl;
    std::cout << "  Errors: " << errorCount.load() << std::endl;
    
    // Verify performance and stability
    EXPECT_GT(writeCount.load(), NUM_WRITER_THREADS * 1000) << "Too few writes completed";
    EXPECT_GT(readCount.load(), NUM_READER_THREADS * 1000) << "Too few reads completed";
    EXPECT_EQ(errorCount.load(), 0) << "Errors occurred during concurrent operations";
}

TEST_F(ConcurrencyPerformanceTest, LockContentionElimination) {
    // Test: Lock contention is eliminated with concurrent data structures
    // Validates: No performance degradation under high concurrency
    
    const std::vector<int> THREAD_COUNTS = {1, 2, 4, 8, 16, 32};
    std::vector<double> throughputs;
    
    for (int numThreads : THREAD_COUNTS) {
        const int OPERATIONS_PER_THREAD = 5000;
        const int TOTAL_OPERATIONS = numThreads * OPERATIONS_PER_THREAD;
        
        std::atomic<int> successCount{0};
        std::atomic<int> errorCount{0};
        
        auto [total_processed, thread_time] = measurePerformance("Lock Contention Test with " + std::to_string(numThreads) + " threads", [&]() {
            std::vector<std::thread> threads;
            for (int threadId = 0; threadId < numThreads; ++threadId) {
                threads.emplace_back([&, threadId]() {
                    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                        auto series = createTimeSeries(threadId * OPERATIONS_PER_THREAD + i, "contention_test");
                        auto result = storage_->write(series);
                        if (result.ok()) {
                            successCount++;
                        } else {
                            errorCount++;
                        }
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            return successCount.load();
        });
        
        double throughput = TOTAL_OPERATIONS / (thread_time.count() / 1000000.0);
        throughputs.push_back(throughput);
        
        std::cout << "Threads: " << numThreads << ", Throughput: " << throughput << " ops/sec" << std::endl;
        
        // Verify success rate
        EXPECT_GT(successCount.load(), TOTAL_OPERATIONS * 0.99) << "Too many failures with " << numThreads << " threads";
    }
    
    // Verify throughput scales with thread count (should be roughly linear up to 8 threads)
    verifyThroughputScaling(throughputs, THREAD_COUNTS);
}

TEST_F(ConcurrencyPerformanceTest, FineGrainedLockingPerformance) {
    // Test: Fine-grained locking performance
    // Validates: Concurrent hash map provides fine-grained locking
    
    const int NUM_THREADS = 16;
    const int OPERATIONS_PER_THREAD = 10000;
    
    // Create series with different patterns to test fine-grained locking
    std::vector<std::string> seriesPatterns = {
        "pattern_a", "pattern_b", "pattern_c", "pattern_d", "pattern_e"
    };
    
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};
    
    auto [total_processed, fine_grained_time] = measurePerformance("Fine-Grained Locking Performance", [&]() {
        std::vector<std::thread> threads;
        for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
            threads.emplace_back([&, threadId]() {
                for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                    // Use different patterns to test fine-grained locking
                    std::string pattern = seriesPatterns[threadId % seriesPatterns.size()];
                    auto series = createTimeSeries(threadId * OPERATIONS_PER_THREAD + i, pattern);
                    
                    auto result = storage_->write(series);
                    if (result.ok()) {
                        successCount++;
                    } else {
                        errorCount++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        return successCount.load();
    });
    
    double throughput = (NUM_THREADS * OPERATIONS_PER_THREAD) / (fine_grained_time.count() / 1000000.0);
    
    std::cout << "Fine-grained locking throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << "Success count: " << successCount.load() << std::endl;
    std::cout << "Error count: " << errorCount.load() << std::endl;
    
    // Verify high throughput with fine-grained locking
    EXPECT_GT(throughput, 80000) << "Fine-grained locking throughput too low: " << throughput << " ops/sec";
    
    // Verify high success rate
    EXPECT_GT(successCount.load(), NUM_THREADS * OPERATIONS_PER_THREAD * 0.99) << "Too many failures";
    EXPECT_EQ(errorCount.load(), 0) << "Errors occurred";
}

TEST_F(ConcurrencyPerformanceTest, HighConcurrencyStressTest) {
    // Test: High concurrency stress test
    // Validates: System remains stable under extreme concurrency
    
    const int NUM_THREADS = 32;
    const int OPERATIONS_PER_THREAD = 2000;
    const int TOTAL_OPERATIONS = NUM_THREADS * OPERATIONS_PER_THREAD;
    
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};
    std::atomic<int> timeoutCount{0};
    
    auto [total_processed, stress_time] = measurePerformance("High Concurrency Stress Test", [&]() {
        std::vector<std::thread> threads;
        for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
            threads.emplace_back([&, threadId]() {
                for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                    try {
                        auto series = createTimeSeries(threadId * OPERATIONS_PER_THREAD + i, "stress_test");
                        auto result = storage_->write(series);
                        if (result.ok()) {
                            successCount++;
                        } else {
                            errorCount++;
                        }
                    } catch (const std::exception& e) {
                        timeoutCount++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        return successCount.load();
    });
    
    double throughput = TOTAL_OPERATIONS / (stress_time.count() / 1000000.0);
    
    std::cout << "High concurrency stress test results:" << std::endl;
    std::cout << "  Throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << "  Success count: " << successCount.load() << std::endl;
    std::cout << "  Error count: " << errorCount.load() << std::endl;
    std::cout << "  Timeout count: " << timeoutCount.load() << std::endl;
    
    // Verify system remains stable under high concurrency
    EXPECT_GT(throughput, 30000) << "High concurrency throughput too low: " << throughput << " ops/sec";
    EXPECT_GT(successCount.load(), TOTAL_OPERATIONS * 0.95) << "Too many failures under high concurrency";
    EXPECT_LT(timeoutCount.load(), TOTAL_OPERATIONS * 0.01) << "Too many timeouts under high concurrency";
}

TEST_F(ConcurrencyPerformanceTest, ConcurrentQueryPerformance) {
    // Test: Concurrent query performance
    // Validates: Concurrent reads don't interfere with each other
    
    // Setup: Write some data first
    const int NUM_SERIES = 1000;
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTimeSeries(i, "query_test");
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    const int NUM_THREADS = 8;
    const int QUERIES_PER_THREAD = 1000;
    const int TOTAL_QUERIES = NUM_THREADS * QUERIES_PER_THREAD;
    
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};
    
    auto [total_processed, query_time] = measurePerformance("Concurrent Query Performance", [&]() {
        std::vector<std::thread> threads;
        for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
            threads.emplace_back([&, threadId]() {
                for (int i = 0; i < QUERIES_PER_THREAD; ++i) {
                    core::Labels labels;
                    labels.add("__name__", "query_test");
                    labels.add("test_id", std::to_string(i % NUM_SERIES));
                    
                    auto result = storage_->read(labels, 0, LLONG_MAX);
                    if (result.ok()) {
                        successCount++;
                    } else if (result.error().what() == std::string("Series not found")) {
                        successCount++; // Not found is acceptable
                    } else {
                        errorCount++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        return successCount.load();
    });
    
    double throughput = TOTAL_QUERIES / (query_time.count() / 1000000.0);
    
    std::cout << "Concurrent query throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << "Success count: " << successCount.load() << std::endl;
    std::cout << "Error count: " << errorCount.load() << std::endl;
    
    // Verify concurrent query performance
    EXPECT_GT(throughput, 10000) << "Concurrent query throughput too low: " << throughput << " ops/sec";
    EXPECT_GT(successCount.load(), TOTAL_QUERIES * 0.99) << "Too many query failures";
    EXPECT_EQ(errorCount.load(), 0) << "Query errors occurred";
}

TEST_F(ConcurrencyPerformanceTest, MixedWorkloadConcurrency) {
    // Test: Mixed workload concurrency
    // Validates: System handles mixed read/write workloads efficiently
    
    const int NUM_THREADS = 12;
    const int OPERATIONS_PER_THREAD = 2000;
    
    std::atomic<int> writeCount{0};
    std::atomic<int> readCount{0};
    std::atomic<int> errorCount{0};
    std::atomic<bool> stopTest{false};
    
    // Mixed workload threads (50% write, 50% read)
    std::vector<std::thread> threads;
    for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
        threads.emplace_back([&, threadId]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                if (i % 2 == 0) {
                    // Write operation
                    auto series = createTimeSeries(threadId * OPERATIONS_PER_THREAD + i, "mixed_workload");
                    auto result = storage_->write(series);
                    if (result.ok()) {
                        writeCount++;
                    } else {
                        errorCount++;
                    }
                } else {
                    // Read operation
                    core::Labels labels;
                    labels.add("__name__", "mixed_workload");
                    labels.add("test_id", std::to_string((threadId * OPERATIONS_PER_THREAD + i) % 1000));
                    
                    auto result = storage_->read(labels, 0, LLONG_MAX);
                    if (result.ok() || result.error().what() == std::string("Series not found")) {
                        readCount++;
                    } else {
                        errorCount++;
                    }
                }
            }
        });
    }
    
    // Run test
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Mixed workload results:" << std::endl;
    std::cout << "  Writes: " << writeCount.load() << std::endl;
    std::cout << "  Reads: " << readCount.load() << std::endl;
    std::cout << "  Errors: " << errorCount.load() << std::endl;
    
    // Verify mixed workload performance
    EXPECT_GT(writeCount.load(), NUM_THREADS * OPERATIONS_PER_THREAD * 0.4) << "Too few writes completed";
    EXPECT_GT(readCount.load(), NUM_THREADS * OPERATIONS_PER_THREAD * 0.4) << "Too few reads completed";
    EXPECT_EQ(errorCount.load(), 0) << "Errors occurred during mixed workload";
}

} // namespace
} // namespace benchmark
} // namespace tsdb


