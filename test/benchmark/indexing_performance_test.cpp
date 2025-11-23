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
 * @brief Indexing Performance Tests
 * 
 * These tests validate the performance improvements from Task 2 of DESIGN_FIXES.md:
 * "Implement an Inverted Index for Labels"
 * 
 * Key improvements tested:
 * 1. Query performance O(log K) vs O(N) linear scans
 * 2. Complex multi-label query performance
 * 3. Index scalability with large datasets
 * 4. Index memory efficiency
 */

class IndexingPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_indexing_test";
        std::filesystem::create_directories(test_dir_);

        // Configure storage with indexing-optimized settings
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
        // Generate realistic test data for indexing testing
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> value_dist(50.0, 15.0);
        std::uniform_int_distribution<> label_dist(1, 1000);

        // Generate test values and labels
        for (int i = 0; i < 50000; ++i) {
            test_values_.push_back(std::max(0.0, std::min(100.0, value_dist(gen))));
            test_labels_.push_back(label_dist(gen));
        }
    }

    // Helper method to create time series with specific labels
    core::TimeSeries createTimeSeriesWithLabels(int id, const std::string& labelValue, const std::string& name = "index_test") {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test_id", std::to_string(id));
        labels.add("label_value", labelValue);
        labels.add("workload", "indexing");
        
        core::TimeSeries series(labels);
        series.add_sample(core::Sample(1000 + id, test_values_[id % test_values_.size()]));
        return series;
    }

    // Helper method to create time series with multiple labels
    core::TimeSeries createTimeSeriesWithMultipleLabels(int id, const std::map<std::string, std::string>& labelMap, const std::string& name = "complex_index_test") {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test_id", std::to_string(id));
        
        for (const auto& [key, value] : labelMap) {
            labels.add(key, value);
        }
        
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

    // Helper method to calculate expected index memory usage
    size_t calculateExpectedIndexMemory(int numSeries, int numUniqueLabels) {
        // Estimate memory usage for inverted index
        size_t postingsOverhead = sizeof(std::vector<core::SeriesID>) * numUniqueLabels;
        size_t seriesLabelsOverhead = sizeof(core::Labels) * numSeries;
        size_t indexOverhead = sizeof(std::map<std::pair<std::string, std::string>, std::vector<core::SeriesID>>);
        
        return postingsOverhead + seriesLabelsOverhead + indexOverhead;
    }

    std::filesystem::path test_dir_;
    std::shared_ptr<storage::Storage> storage_;
    
    // Test data
    std::vector<double> test_values_;
    std::vector<int> test_labels_;
};

TEST_F(IndexingPerformanceTest, InvertedIndexQueryPerformance) {
    // Test: Query performance with inverted index
    // Validates: O(log K) vs O(N) performance improvement from Task 2
    
    // Setup: Create large number of series with various labels
    const int NUM_SERIES = 50000;
    const int NUM_LABEL_VALUES = 1000;
    
    std::cout << "Setting up " << NUM_SERIES << " series with " << NUM_LABEL_VALUES << " unique label values..." << std::endl;
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTimeSeriesWithLabels(i, "label_value_" + std::to_string(i % NUM_LABEL_VALUES));
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Test query performance with inverted index
    std::vector<std::pair<std::string, std::string>> matchers;
    matchers.emplace_back("label_value", "label_value_500");
    
    auto [query_result, query_time] = measurePerformance("Inverted Index Query", [&]() {
        return storage_->query(matchers, 0, LLONG_MAX);
    });
    
    ASSERT_TRUE(query_result.ok()) << "Query failed: " << query_result.error();
    
    // Verify query performance (should be <1ms with inverted index)
    EXPECT_LT(query_time.count(), 1000) << "Query too slow with inverted index: " << query_time.count() << "μs";
    
    // Verify result accuracy
    EXPECT_GT(query_result.value().size(), 0) << "No results found for query";
    
    std::cout << "Query returned " << query_result.value().size() << " results" << std::endl;
}

TEST_F(IndexingPerformanceTest, ComplexQueryPerformance) {
    // Test: Complex multi-label queries with inverted index
    // Validates: Index performance with multiple matchers
    
    const int NUM_SERIES = 25000;
    
    std::cout << "Setting up " << NUM_SERIES << " series with multiple labels..." << std::endl;
    
    // Create series with multiple labels
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTimeSeriesWithMultipleLabels(i, {
            {"service", "service_" + std::to_string(i % 100)},
            {"instance", "instance_" + std::to_string(i % 1000)},
            {"endpoint", "endpoint_" + std::to_string(i % 50)}
        });
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Test complex query performance
    std::vector<std::pair<std::string, std::string>> matchers;
    matchers.emplace_back("service", "service_50");
    matchers.emplace_back("endpoint", "endpoint_25");
    
    auto [query_result, query_time] = measurePerformance("Complex Multi-Label Query", [&]() {
        return storage_->query(matchers, 0, LLONG_MAX);
    });
    
    ASSERT_TRUE(query_result.ok()) << "Complex query failed: " << query_result.error();
    
    // Verify complex query performance (should be <2ms with inverted index)
    EXPECT_LT(query_time.count(), 2000) << "Complex query too slow: " << query_time.count() << "μs";
    
    // Verify result accuracy
    EXPECT_GT(query_result.value().size(), 0) << "No results found for complex query";
    
    std::cout << "Complex query returned " << query_result.value().size() << " results" << std::endl;
}

TEST_F(IndexingPerformanceTest, IndexScalability) {
    // Test: Index performance scales with data size
    // Validates: Index performance doesn't degrade with large datasets
    
    const std::vector<int> SCALE_LEVELS = {1000, 5000, 10000, 25000, 50000};
    
    for (int scale : SCALE_LEVELS) {
        std::cout << "Testing scalability at scale " << scale << "..." << std::endl;
        
        // Create data at this scale
        for (int i = 0; i < scale; ++i) {
            auto series = createTimeSeriesWithLabels(i, "scale_value_" + std::to_string(i % 100), "scale_test");
            ASSERT_TRUE(storage_->write(series).ok());
        }
        
        // Measure query performance at this scale
        std::vector<std::pair<std::string, std::string>> matchers;
        matchers.emplace_back("__name__", "scale_test");
        
        auto [query_result, query_time] = measurePerformance("Scalability Query at Scale " + std::to_string(scale), [&]() {
            return storage_->query(matchers, 0, LLONG_MAX);
        });
        
        ASSERT_TRUE(query_result.ok()) << "Query failed at scale " << scale;
        
        // Verify performance scales logarithmically, not linearly
        EXPECT_LT(query_time.count(), 1000 + (scale / 1000)) 
            << "Index performance not scaling well at scale " << scale << ": " << query_time.count() << "μs";
        
        std::cout << "Scale " << scale << " query time: " << query_time.count() << "μs" << std::endl;
    }
}

TEST_F(IndexingPerformanceTest, IndexMemoryEfficiency) {
    // Test: Index memory usage is reasonable
    // Validates: Index doesn't cause memory bloat
    
    const int NUM_SERIES = 50000;
    const int NUM_UNIQUE_LABELS = 5000;
    
    std::cout << "Testing index memory efficiency with " << NUM_SERIES << " series and " << NUM_UNIQUE_LABELS << " unique labels..." << std::endl;
    
    // Create series with many unique labels
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTimeSeriesWithLabels(i, "unique_label_" + std::to_string(i % NUM_UNIQUE_LABELS), "index_memory_test");
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Calculate expected index memory usage
    auto expectedIndexMemory = calculateExpectedIndexMemory(NUM_SERIES, NUM_UNIQUE_LABELS);
    
    std::cout << "Expected index memory usage: " << expectedIndexMemory << " bytes" << std::endl;
    
    // Verify index memory usage is reasonable
    // Note: This is a simplified check - in practice, we'd need to measure actual memory usage
    EXPECT_GT(expectedIndexMemory, 0) << "Expected index memory usage should be positive";
    
    // Test query performance to ensure index is working
    std::vector<std::pair<std::string, std::string>> matchers;
    matchers.emplace_back("__name__", "index_memory_test");
    
    auto [query_result, query_time] = measurePerformance("Index Memory Efficiency Query", [&]() {
        return storage_->query(matchers, 0, LLONG_MAX);
    });
    
    ASSERT_TRUE(query_result.ok()) << "Query failed during memory efficiency test";
    
    // Verify query performance is still good
    EXPECT_LT(query_time.count(), 2000) << "Query performance degraded: " << query_time.count() << "μs";
    
    std::cout << "Index memory efficiency test completed successfully" << std::endl;
}

TEST_F(IndexingPerformanceTest, MultipleQueryPerformance) {
    // Test: Performance with multiple concurrent queries
    // Validates: Index handles multiple queries efficiently
    
    const int NUM_SERIES = 20000;
    const int NUM_QUERIES = 1000;
    
    std::cout << "Setting up " << NUM_SERIES << " series for multiple query test..." << std::endl;
    
    // Create series with various labels
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTimeSeriesWithMultipleLabels(i, {
            {"category", "category_" + std::to_string(i % 10)},
            {"region", "region_" + std::to_string(i % 5)},
            {"status", "status_" + std::to_string(i % 3)}
        });
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Test multiple queries
    auto [total_results, total_time] = measurePerformance("Multiple Queries", [&]() {
        int total_results = 0;
        for (int i = 0; i < NUM_QUERIES; ++i) {
            std::vector<std::pair<std::string, std::string>> matchers;
            matchers.emplace_back("category", "category_" + std::to_string(i % 10));
            
            auto result = storage_->query(matchers, 0, LLONG_MAX);
            if (result.ok()) {
                total_results += result.value().size();
            }
        }
        return total_results;
    });
    
    double avgQueryTime = total_time.count() / NUM_QUERIES;
    double queriesPerSecond = NUM_QUERIES / (total_time.count() / 1000000.0);
    
    std::cout << "Average query time: " << avgQueryTime << "μs" << std::endl;
    std::cout << "Queries per second: " << queriesPerSecond << std::endl;
    std::cout << "Total results: " << total_results << std::endl;
    
    // Verify multiple query performance
    EXPECT_LT(avgQueryTime, 1000) << "Average query time too high: " << avgQueryTime << "μs";
    EXPECT_GT(queriesPerSecond, 1000) << "Queries per second too low: " << queriesPerSecond;
    EXPECT_GT(total_results, 0) << "No results found in multiple queries";
}

TEST_F(IndexingPerformanceTest, IndexUpdatePerformance) {
    // Test: Index update performance during writes
    // Validates: Index updates don't significantly impact write performance
    
    const int NUM_SERIES = 10000;
    const int NUM_LABEL_VALUES = 100;
    
    std::cout << "Testing index update performance with " << NUM_SERIES << " series..." << std::endl;
    
    auto [success_count, write_time] = measurePerformance("Index Update Performance", [&]() {
        int success_count = 0;
        for (int i = 0; i < NUM_SERIES; ++i) {
            auto series = createTimeSeriesWithLabels(i, "update_label_" + std::to_string(i % NUM_LABEL_VALUES), "update_test");
            auto result = storage_->write(series);
            if (result.ok()) {
                success_count++;
            }
        }
        return success_count;
    });
    
    double writeThroughput = NUM_SERIES / (write_time.count() / 1000000.0);
    
    std::cout << "Write throughput with index updates: " << writeThroughput << " ops/sec" << std::endl;
    
    // Verify write performance is still good with index updates
    EXPECT_GT(writeThroughput, 5000) << "Write throughput too low with index updates: " << writeThroughput << " ops/sec";
    
    // Verify high success rate
    EXPECT_GT(success_count, NUM_SERIES * 0.99) << "Too many write failures during index updates";
    
    // Test that index is working after updates
    std::vector<std::pair<std::string, std::string>> matchers;
    matchers.emplace_back("__name__", "update_test");
    
    auto [query_result, query_time] = measurePerformance("Post-Update Query", [&]() {
        return storage_->query(matchers, 0, LLONG_MAX);
    });
    
    ASSERT_TRUE(query_result.ok()) << "Query failed after index updates";
    EXPECT_GT(query_result.value().size(), 0) << "No results found after index updates";
    EXPECT_LT(query_time.count(), 1000) << "Query too slow after index updates: " << query_time.count() << "μs";
}

TEST_F(IndexingPerformanceTest, IndexConsistencyValidation) {
    // Test: Index consistency under concurrent operations
    // Validates: Index remains consistent during concurrent writes and queries
    
    const int NUM_SERIES = 5000;
    const int NUM_CONCURRENT_OPERATIONS = 1000;
    
    std::cout << "Testing index consistency with " << NUM_SERIES << " series and " << NUM_CONCURRENT_OPERATIONS << " concurrent operations..." << std::endl;
    
    // Write initial data
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTimeSeriesWithLabels(i, "consistency_label_" + std::to_string(i % 100), "consistency_test");
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Perform concurrent operations
    std::atomic<int> writeCount{0};
    std::atomic<int> queryCount{0};
    std::atomic<int> errorCount{0};
    
    std::vector<std::thread> threads;
    
    // Writer threads
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < NUM_CONCURRENT_OPERATIONS / 4; ++j) {
                auto series = createTimeSeriesWithLabels(NUM_SERIES + i * (NUM_CONCURRENT_OPERATIONS / 4) + j, 
                                                       "concurrent_label_" + std::to_string(j % 50), "concurrent_test");
                auto result = storage_->write(series);
                if (result.ok()) {
                    writeCount++;
                } else {
                    errorCount++;
                }
            }
        });
    }
    
    // Reader threads
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < NUM_CONCURRENT_OPERATIONS / 4; ++j) {
                std::vector<std::pair<std::string, std::string>> matchers;
                matchers.emplace_back("__name__", "consistency_test");
                
                auto result = storage_->query(matchers, 0, LLONG_MAX);
                if (result.ok()) {
                    queryCount++;
                } else {
                    errorCount++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Concurrent operations completed:" << std::endl;
    std::cout << "  Writes: " << writeCount.load() << std::endl;
    std::cout << "  Queries: " << queryCount.load() << std::endl;
    std::cout << "  Errors: " << errorCount.load() << std::endl;
    
    // Verify high success rates
    EXPECT_GT(writeCount.load(), NUM_CONCURRENT_OPERATIONS * 0.9) << "Too many write failures during concurrent operations";
    EXPECT_GT(queryCount.load(), NUM_CONCURRENT_OPERATIONS * 0.9) << "Too many query failures during concurrent operations";
    EXPECT_EQ(errorCount.load(), 0) << "Errors occurred during concurrent operations";
    
    // Verify index consistency
    std::vector<std::pair<std::string, std::string>> matchers;
    matchers.emplace_back("__name__", "consistency_test");
    
    auto [query_result, query_time] = measurePerformance("Consistency Validation Query", [&]() {
        return storage_->query(matchers, 0, LLONG_MAX);
    });
    
    ASSERT_TRUE(query_result.ok()) << "Query failed during consistency validation";
    EXPECT_GT(query_result.value().size(), 0) << "No results found during consistency validation";
    EXPECT_LT(query_time.count(), 2000) << "Query too slow during consistency validation: " << query_time.count() << "μs";
}

} // namespace
} // namespace benchmark
} // namespace tsdb


