/**
 * @file phase1_object_pool_integration_test.cpp
 * @brief Phase 1: Comprehensive Object Pool Integration Tests for StorageImpl
 * 
 * This file provides exhaustive testing of object pool integration (TimeSeriesPool, 
 * LabelsPool, SamplePool) into the StorageImpl class. It validates memory efficiency,
 * performance characteristics, thread safety, and edge cases to ensure robust
 * object pool integration.
 * 
 * Test Categories:
 * - Memory allocation efficiency and reduction
 * - Object pool lifecycle management
 * - Performance benchmarking and optimization
 * - Thread safety and concurrent access
 * - Pool boundary conditions and edge cases
 * - Statistics accuracy and monitoring
 * - Memory leak detection and prevention
 * - Pool configuration validation
 * 
 * Expected Outcomes:
 * - 80-95% memory allocation reduction compared to direct allocation
 * - Zero memory leaks across all operations
 * - Thread-safe pool operations under high concurrency
 * - Accurate pool statistics and monitoring
 * - Optimal performance under various workloads
 * - Proper pool object lifecycle management
 * - Graceful handling of pool exhaustion scenarios
 */

#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include <random>
#include <set>
#include <map>
#include <sstream>
#include <iomanip>

namespace tsdb {
namespace integration {

class Phase1ObjectPoolIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create comprehensive test configuration
        core::StorageConfig config;
        config.data_dir = "./test_data_phase1";
        
        // Configure pools for thorough testing
        config.object_pool_config.time_series_initial_size = 50;
        config.object_pool_config.time_series_max_size = 1000;
        config.object_pool_config.labels_initial_size = 100;
        config.object_pool_config.labels_max_size = 2000;
        config.object_pool_config.samples_initial_size = 500;
        config.object_pool_config.samples_max_size = 10000;
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
        
        // Initialize performance tracking
        reset_performance_metrics();
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
    }
    
    // Performance tracking structures
    struct PerformanceMetrics {
        std::atomic<size_t> total_allocations{0};
        std::atomic<size_t> total_deallocations{0};
        std::atomic<size_t> pool_hits{0};
        std::atomic<size_t> pool_misses{0};
        std::chrono::high_resolution_clock::time_point start_time;
        std::chrono::high_resolution_clock::time_point end_time;
    };
    
    PerformanceMetrics metrics_;
    
    void reset_performance_metrics() {
        metrics_.total_allocations = 0;
        metrics_.total_deallocations = 0;
        metrics_.pool_hits = 0;
        metrics_.pool_misses = 0;
        metrics_.start_time = std::chrono::high_resolution_clock::now();
    }
    
    double get_elapsed_time_ms() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - metrics_.start_time).count() / 1000.0;
    }
    
    // Helper method to create test time series with controlled characteristics
    core::TimeSeries createTestSeries(const std::string& name, int sample_count, 
                                     const std::map<std::string, std::string>& additional_labels = {}) {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test", "phase1");
        labels.add("pool_test", "true");
        
        // Add additional labels for testing
        for (const auto& [key, value] : additional_labels) {
            labels.add(key, value);
        }
        
        core::TimeSeries series(labels);
        for (int i = 0; i < sample_count; ++i) {
            series.add_sample(1000 + i, 100.0 + i * 0.1 + (i % 10) * 0.01);
        }
        return series;
    }
    
    // Helper method to create large time series for stress testing
    core::TimeSeries createLargeTestSeries(const std::string& name, int sample_count) {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test", "phase1");
        labels.add("size", "large");
        labels.add("pool_test", "true");
        
        core::TimeSeries series(labels);
        for (int i = 0; i < sample_count; ++i) {
            series.add_sample(1000 + i, 100.0 + i * 0.1 + std::sin(i * 0.1) * 10.0);
        }
        return series;
    }
    
    // Helper method to extract and parse pool statistics
    struct DetailedPoolStats {
        size_t time_series_available;
        size_t time_series_total_created;
        size_t time_series_total_acquired;
        size_t time_series_total_released;
        size_t labels_available;
        size_t labels_total_created;
        size_t labels_total_acquired;
        size_t labels_total_released;
        size_t samples_available;
        size_t samples_total_created;
        size_t samples_total_acquired;
        size_t samples_total_released;
        double time_series_reuse_rate;
        double labels_reuse_rate;
        double samples_reuse_rate;
    };
    
    DetailedPoolStats getDetailedPoolStats() {
        std::string stats = storage_->stats();
        DetailedPoolStats result = {};
        
        // Parse the stats string to extract detailed information
        std::istringstream iss(stats);
        std::string line;
        
        while (std::getline(iss, line)) {
            // Parse TimeSeriesPool statistics
            if (line.find("TimeSeriesPool Statistics:") != std::string::npos) {
                // Read next few lines for TimeSeriesPool stats
                for (int i = 0; i < 6; ++i) {
                    if (!std::getline(iss, line)) break;
                    
                    if (line.find("Available objects:") != std::string::npos) {
                        sscanf(line.c_str(), "  Available objects: %zu", &result.time_series_available);
                    } else if (line.find("Total created:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total created: %zu", &result.time_series_total_created);
                    } else if (line.find("Total acquired:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total acquired: %zu", &result.time_series_total_acquired);
                    } else if (line.find("Total released:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total released: %zu", &result.time_series_total_released);
                    }
                }
            }
            
            // Parse LabelsPool statistics
            if (line.find("LabelsPool Statistics:") != std::string::npos) {
                // Read next few lines for LabelsPool stats
                for (int i = 0; i < 6; ++i) {
                    if (!std::getline(iss, line)) break;
                    
                    if (line.find("Available objects:") != std::string::npos) {
                        sscanf(line.c_str(), "  Available objects: %zu", &result.labels_available);
                    } else if (line.find("Total created:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total created: %zu", &result.labels_total_created);
                    } else if (line.find("Total acquired:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total acquired: %zu", &result.labels_total_acquired);
                    } else if (line.find("Total released:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total released: %zu", &result.labels_total_released);
                    }
                }
            }
            
            // Parse SamplePool statistics
            if (line.find("SamplePool Statistics:") != std::string::npos) {
                // Read next few lines for SamplePool stats
                for (int i = 0; i < 6; ++i) {
                    if (!std::getline(iss, line)) break;
                    
                    if (line.find("Available objects:") != std::string::npos) {
                        sscanf(line.c_str(), "  Available objects: %zu", &result.samples_available);
                    } else if (line.find("Total created:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total created: %zu", &result.samples_total_created);
                    } else if (line.find("Total acquired:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total acquired: %zu", &result.samples_total_acquired);
                    } else if (line.find("Total released:") != std::string::npos) {
                        sscanf(line.c_str(), "  Total released: %zu", &result.samples_total_released);
                    }
                }
            }
        }
        
        return result;
    }
    
    // Helper method to validate pool efficiency
    void validatePoolEfficiency(const std::string& operation_name, size_t operations_count) {
        auto stats = getDetailedPoolStats();
        
        // Calculate reuse rates
        double time_series_reuse_rate = (stats.time_series_total_acquired > 0) ? 
            (double)(stats.time_series_total_acquired - stats.time_series_total_created) / stats.time_series_total_acquired : 0.0;
        
        double labels_reuse_rate = (stats.labels_total_acquired > 0) ? 
            (double)(stats.labels_total_acquired - stats.labels_total_created) / stats.labels_total_acquired : 0.0;
        
        double samples_reuse_rate = (stats.samples_total_acquired > 0) ? 
            (double)(stats.samples_total_acquired - stats.samples_total_created) / stats.samples_total_acquired : 0.0;
        
        // Validate minimum reuse rates (should be > 50% for efficient pools)
        EXPECT_GT(time_series_reuse_rate, 0.5) << "TimeSeries pool reuse rate too low for " << operation_name;
        EXPECT_GT(labels_reuse_rate, 0.5) << "Labels pool reuse rate too low for " << operation_name;
        EXPECT_GT(samples_reuse_rate, 0.5) << "Samples pool reuse rate too low for " << operation_name;
        
        // Log efficiency metrics
        std::cout << "Pool Efficiency for " << operation_name << " (" << operations_count << " operations):" << std::endl;
        std::cout << "  TimeSeries reuse rate: " << std::fixed << std::setprecision(2) << (time_series_reuse_rate * 100) << "%" << std::endl;
        std::cout << "  Labels reuse rate: " << std::fixed << std::setprecision(2) << (labels_reuse_rate * 100) << "%" << std::endl;
        std::cout << "  Samples reuse rate: " << std::fixed << std::setprecision(2) << (samples_reuse_rate * 100) << "%" << std::endl;
    }
    
    // Helper method to measure memory usage (simplified)
    size_t estimateMemoryUsage() {
        // This is a simplified estimation - in production you'd use proper memory profiling
        auto stats = getDetailedPoolStats();
        return (stats.time_series_total_created * sizeof(core::TimeSeries)) +
               (stats.labels_total_created * sizeof(core::Labels)) +
               (stats.samples_total_created * sizeof(core::Sample));
    }
    
    std::unique_ptr<storage::StorageImpl> storage_;
};

// Test 1: Comprehensive Memory Allocation Efficiency Validation
TEST_F(Phase1ObjectPoolIntegrationTest, MemoryAllocationEfficiency) {
    const int num_operations = 1000;
    const int samples_per_series = 100;
    
    std::cout << "Testing memory allocation efficiency with " << num_operations 
              << " operations, " << samples_per_series << " samples per series" << std::endl;
    
    reset_performance_metrics();
    
    // Phase 1: Write operations to populate pools
    for (int i = 0; i < num_operations; ++i) {
        auto series = createTestSeries("efficiency_test_" + std::to_string(i), samples_per_series);
        auto write_result = storage_->write(series);
        ASSERT_TRUE(write_result.ok()) << "Write failed for operation " << i;
    }
    
    // Phase 2: Read operations to test pool reuse
    for (int i = 0; i < num_operations; ++i) {
        core::Labels query_labels;
        query_labels.add("__name__", "efficiency_test_" + std::to_string(i));
        query_labels.add("test", "phase1");
        
        auto read_result = storage_->read(query_labels, 1000, 1000 + samples_per_series);
        ASSERT_TRUE(read_result.ok()) << "Read failed for operation " << i;
    }
    
    // Phase 3: Query operations to test multiple result handling
    for (int i = 0; i < num_operations / 10; ++i) {
        std::vector<std::pair<std::string, std::string>> matchers;
        matchers.emplace_back("test", "phase1");
        matchers.emplace_back("pool_test", "true");
        
        auto query_result = storage_->query(matchers, 1000, 1000 + samples_per_series);
        ASSERT_TRUE(query_result.ok()) << "Query failed for batch " << i;
        EXPECT_GT(query_result.value().size(), 0) << "Query returned no results for batch " << i;
    }
    
    // Validate pool efficiency
    validatePoolEfficiency("MemoryAllocationEfficiency", num_operations);
    
    // Measure and report performance
    double elapsed_time = get_elapsed_time_ms();
    size_t estimated_memory = estimateMemoryUsage();
    
    std::cout << "Performance Summary:" << std::endl;
    std::cout << "  Total operations: " << (num_operations * 2 + num_operations / 10) << std::endl;
    std::cout << "  Elapsed time: " << std::fixed << std::setprecision(2) << elapsed_time << " ms" << std::endl;
    std::cout << "  Operations per second: " << std::fixed << std::setprecision(2) 
              << ((num_operations * 2 + num_operations / 10) / (elapsed_time / 1000.0)) << std::endl;
    std::cout << "  Estimated memory usage: " << estimated_memory << " bytes" << std::endl;
    
    // Performance assertions
    EXPECT_GT(elapsed_time, 0) << "Performance measurement failed";
    EXPECT_LT(elapsed_time, 10000) << "Performance too slow (should complete within 10 seconds)";
}

// Test 2: Object Pool Lifecycle Management and Memory Leak Detection
TEST_F(Phase1ObjectPoolIntegrationTest, PoolLifecycleAndMemoryLeakDetection) {
    const int num_iterations = 100;
    const int operations_per_iteration = 50;
    
    std::cout << "Testing pool lifecycle management and memory leak detection" << std::endl;
    
    // Track initial memory state
    auto initial_stats = getDetailedPoolStats();
    size_t initial_memory = estimateMemoryUsage();
    
    // Perform multiple cycles of intensive operations
    for (int cycle = 0; cycle < num_iterations; ++cycle) {
        // Write phase
        for (int i = 0; i < operations_per_iteration; ++i) {
            auto series = createTestSeries("lifecycle_test_" + std::to_string(cycle) + "_" + std::to_string(i), 50);
            auto write_result = storage_->write(series);
            ASSERT_TRUE(write_result.ok()) << "Write failed in cycle " << cycle << ", operation " << i;
        }
        
        // Read phase
        for (int i = 0; i < operations_per_iteration; ++i) {
            core::Labels query_labels;
            query_labels.add("__name__", "lifecycle_test_" + std::to_string(cycle) + "_" + std::to_string(i));
            query_labels.add("test", "phase1");
            
            auto read_result = storage_->read(query_labels, 1000, 1050);
            ASSERT_TRUE(read_result.ok()) << "Read failed in cycle " << cycle << ", operation " << i;
        }
        
        // Query phase
        for (int i = 0; i < operations_per_iteration / 5; ++i) {
            std::vector<std::pair<std::string, std::string>> matchers;
            matchers.emplace_back("test", "phase1");
            matchers.emplace_back("pool_test", "true");
            
            auto query_result = storage_->query(matchers, 1000, 1050);
            ASSERT_TRUE(query_result.ok()) << "Query failed in cycle " << cycle << ", batch " << i;
        }
        
        // Validate pool state after each cycle
        auto cycle_stats = getDetailedPoolStats();
        
        // Check for memory leaks (total created should not grow unbounded)
        EXPECT_LE(cycle_stats.time_series_total_created, initial_stats.time_series_total_created + 1000)
            << "TimeSeries pool memory leak detected in cycle " << cycle;
        EXPECT_LE(cycle_stats.labels_total_created, initial_stats.labels_total_created + 2000)
            << "Labels pool memory leak detected in cycle " << cycle;
        EXPECT_LE(cycle_stats.samples_total_created, initial_stats.samples_total_created + 10000)
            << "Samples pool memory leak detected in cycle " << cycle;
    }
    
    // Final validation
    auto final_stats = getDetailedPoolStats();
    size_t final_memory = estimateMemoryUsage();
    
    // Memory should not have grown significantly (indicating proper cleanup)
    double memory_growth_ratio = (double)final_memory / initial_memory;
    EXPECT_LT(memory_growth_ratio, 2.0) << "Excessive memory growth detected (ratio: " << memory_growth_ratio << ")";
    
    std::cout << "Lifecycle Test Results:" << std::endl;
    std::cout << "  Initial memory: " << initial_memory << " bytes" << std::endl;
    std::cout << "  Final memory: " << final_memory << " bytes" << std::endl;
    std::cout << "  Memory growth ratio: " << std::fixed << std::setprecision(2) << memory_growth_ratio << std::endl;
    std::cout << "  Total operations: " << (num_iterations * operations_per_iteration * 2 + num_iterations * operations_per_iteration / 5) << std::endl;
}

// Test 3: Comprehensive Thread Safety and Concurrent Access Testing
TEST_F(Phase1ObjectPoolIntegrationTest, ThreadSafetyAndConcurrentAccess) {
    const int num_threads = 8;
    const int operations_per_thread = 200;
    const int num_rounds = 5;
    
    std::cout << "Testing thread safety with " << num_threads << " threads, " 
              << operations_per_thread << " operations per thread, " << num_rounds << " rounds" << std::endl;
    
    std::atomic<int> total_successful_operations{0};
    std::atomic<int> total_failed_operations{0};
    std::atomic<int> data_integrity_errors{0};
    
    // Multiple rounds of concurrent testing
    for (int round = 0; round < num_rounds; ++round) {
        std::vector<std::thread> threads;
        
        // Create worker threads
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([this, t, operations_per_thread, round, &total_successful_operations, 
                                 &total_failed_operations, &data_integrity_errors]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> sample_count_dist(10, 100);
                std::uniform_int_distribution<> delay_dist(0, 10);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    try {
                        // Random delay to increase contention
                        std::this_thread::sleep_for(std::chrono::microseconds(delay_dist(gen)));
                        
                        // Write operation
                        int sample_count = sample_count_dist(gen);
                        auto series = createTestSeries("thread_test_" + std::to_string(t) + "_" + 
                                                     std::to_string(round) + "_" + std::to_string(i), sample_count);
                        auto write_result = storage_->write(series);
                        
                        if (write_result.ok()) {
                            total_successful_operations++;
                            
                            // Read operation to verify data integrity
                            core::Labels query_labels;
                            query_labels.add("__name__", "thread_test_" + std::to_string(t) + "_" + 
                                           std::to_string(round) + "_" + std::to_string(i));
                            query_labels.add("test", "phase1");
                            
                            auto read_result = storage_->read(query_labels, 1000, 1000 + sample_count);
                            if (read_result.ok()) {
                                // Verify data integrity
                                auto& retrieved_series = read_result.value();
                                if (retrieved_series.samples().size() != sample_count) {
                                    data_integrity_errors++;
                                }
                                total_successful_operations++;
                            } else {
                                total_failed_operations++;
                            }
                        } else {
                            total_failed_operations++;
                        }
                        
                        // Query operation
                        std::vector<std::pair<std::string, std::string>> matchers;
                        matchers.emplace_back("test", "phase1");
                        matchers.emplace_back("pool_test", "true");
                        
                        auto query_result = storage_->query(matchers, 1000, 1000 + sample_count);
                        if (query_result.ok()) {
                            total_successful_operations++;
                        } else {
                            total_failed_operations++;
                        }
                        
                    } catch (const std::exception& e) {
                        total_failed_operations++;
                    }
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Validate pool state after each round
        auto round_stats = getDetailedPoolStats();
        EXPECT_GT(round_stats.time_series_available, 0) << "TimeSeries pool exhausted in round " << round;
        EXPECT_GT(round_stats.labels_available, 0) << "Labels pool exhausted in round " << round;
        EXPECT_GT(round_stats.samples_available, 0) << "Samples pool exhausted in round " << round;
    }
    
    // Final validation
    int total_expected_operations = num_threads * operations_per_thread * num_rounds * 3; // write + read + query
    double success_rate = (double)total_successful_operations.load() / total_expected_operations;
    
    std::cout << "Thread Safety Test Results:" << std::endl;
    std::cout << "  Total successful operations: " << total_successful_operations.load() << std::endl;
    std::cout << "  Total failed operations: " << total_failed_operations.load() << std::endl;
    std::cout << "  Data integrity errors: " << data_integrity_errors.load() << std::endl;
    std::cout << "  Success rate: " << std::fixed << std::setprecision(2) << (success_rate * 100) << "%" << std::endl;
    
    // Assertions
    EXPECT_GT(success_rate, 0.95) << "Thread safety test success rate too low: " << (success_rate * 100) << "%";
    EXPECT_EQ(data_integrity_errors.load(), 0) << "Data integrity errors detected during concurrent access";
    EXPECT_LT(total_failed_operations.load(), total_expected_operations * 0.05) << "Too many failed operations";
}

// Test 4: Pool Boundary Conditions and Edge Cases
TEST_F(Phase1ObjectPoolIntegrationTest, PoolBoundaryConditionsAndEdgeCases) {
    std::cout << "Testing pool boundary conditions and edge cases" << std::endl;
    
    // Test 4.1: Empty series handling
    {
        core::TimeSeries empty_series(core::Labels{});
        auto write_result = storage_->write(empty_series);
        // Should fail gracefully
        EXPECT_FALSE(write_result.ok()) << "Empty series should be rejected";
    }
    
    // Test 4.2: Very large series (near pool limits)
    {
        auto large_series = createLargeTestSeries("boundary_large", 1000);
        auto write_result = storage_->write(large_series);
        ASSERT_TRUE(write_result.ok()) << "Large series write failed";
        
        // Read back and verify
        core::Labels query_labels;
        query_labels.add("__name__", "boundary_large");
        query_labels.add("test", "phase1");
        
        auto read_result = storage_->read(query_labels, 1000, 2000);
        ASSERT_TRUE(read_result.ok()) << "Large series read failed";
        EXPECT_EQ(read_result.value().samples().size(), 1000) << "Large series data integrity failed";
    }
    
    // Test 4.3: Rapid pool exhaustion and recovery
    {
        const int rapid_operations = 5000;
        std::vector<core::TimeSeries> series_batch;
        
        // Create many series rapidly
        for (int i = 0; i < rapid_operations; ++i) {
            series_batch.push_back(createTestSeries("rapid_test_" + std::to_string(i), 50));
        }
        
        // Write them all rapidly
        for (int i = 0; i < rapid_operations; ++i) {
            auto write_result = storage_->write(series_batch[i]);
            ASSERT_TRUE(write_result.ok()) << "Rapid write failed at operation " << i;
        }
        
        // Read them all rapidly
        for (int i = 0; i < rapid_operations; ++i) {
            core::Labels query_labels;
            query_labels.add("__name__", "rapid_test_" + std::to_string(i));
            query_labels.add("test", "phase1");
            
            auto read_result = storage_->read(query_labels, 1000, 1050);
            ASSERT_TRUE(read_result.ok()) << "Rapid read failed at operation " << i;
        }
        
        // Validate pool state after rapid operations
        auto rapid_stats = getDetailedPoolStats();
        EXPECT_GT(rapid_stats.time_series_available, 0) << "TimeSeries pool exhausted during rapid operations";
        EXPECT_GT(rapid_stats.labels_available, 0) << "Labels pool exhausted during rapid operations";
        EXPECT_GT(rapid_stats.samples_available, 0) << "Samples pool exhausted during rapid operations";
    }
    
    // Test 4.4: Mixed operation types under stress
    {
        const int mixed_operations = 1000;
        std::atomic<int> write_success{0};
        std::atomic<int> read_success{0};
        std::atomic<int> query_success{0};
        
        std::vector<std::thread> mixed_threads;
        
        for (int t = 0; t < 4; ++t) {
            mixed_threads.emplace_back([this, t, mixed_operations, &write_success, &read_success, &query_success]() {
                for (int i = 0; i < mixed_operations; ++i) {
                    // Write operation
                    auto series = createTestSeries("mixed_test_" + std::to_string(t) + "_" + std::to_string(i), 25);
                    if (storage_->write(series).ok()) {
                        write_success++;
                    }
                    
                    // Read operation
                    core::Labels query_labels;
                    query_labels.add("__name__", "mixed_test_" + std::to_string(t) + "_" + std::to_string(i));
                    query_labels.add("test", "phase1");
                    
                    if (storage_->read(query_labels, 1000, 1025).ok()) {
                        read_success++;
                    }
                    
                    // Query operation
                    std::vector<std::pair<std::string, std::string>> matchers;
                    matchers.emplace_back("test", "phase1");
                    matchers.emplace_back("pool_test", "true");
                    
                    if (storage_->query(matchers, 1000, 1025).ok()) {
                        query_success++;
                    }
                }
            });
        }
        
        for (auto& thread : mixed_threads) {
            thread.join();
        }
        
        std::cout << "Mixed Operations Results:" << std::endl;
        std::cout << "  Write success: " << write_success.load() << "/" << (4 * mixed_operations) << std::endl;
        std::cout << "  Read success: " << read_success.load() << "/" << (4 * mixed_operations) << std::endl;
        std::cout << "  Query success: " << query_success.load() << "/" << (4 * mixed_operations) << std::endl;
        
        // Validate success rates
        double write_rate = (double)write_success.load() / (4 * mixed_operations);
        double read_rate = (double)read_success.load() / (4 * mixed_operations);
        double query_rate = (double)query_success.load() / (4 * mixed_operations);
        
        EXPECT_GT(write_rate, 0.95) << "Write success rate too low: " << (write_rate * 100) << "%";
        EXPECT_GT(read_rate, 0.95) << "Read success rate too low: " << (read_rate * 100) << "%";
        EXPECT_GT(query_rate, 0.95) << "Query success rate too low: " << (query_rate * 100) << "%";
    }
}

// Test 5: Comprehensive Statistics Accuracy and Monitoring
TEST_F(Phase1ObjectPoolIntegrationTest, StatisticsAccuracyAndMonitoring) {
    std::cout << "Testing statistics accuracy and monitoring capabilities" << std::endl;
    
    // Baseline statistics
    auto baseline_stats = getDetailedPoolStats();
    
    // Perform controlled operations
    const int controlled_operations = 500;
    std::vector<std::string> series_names;
    
    // Write phase
    for (int i = 0; i < controlled_operations; ++i) {
        std::string series_name = "stats_test_" + std::to_string(i);
        series_names.push_back(series_name);
        
        auto series = createTestSeries(series_name, 30);
        auto write_result = storage_->write(series);
        ASSERT_TRUE(write_result.ok()) << "Write failed for stats test " << i;
    }
    
    auto write_stats = getDetailedPoolStats();
    
    // Read phase
    for (int i = 0; i < controlled_operations; ++i) {
        core::Labels query_labels;
        query_labels.add("__name__", series_names[i]);
        query_labels.add("test", "phase1");
        
        auto read_result = storage_->read(query_labels, 1000, 1030);
        ASSERT_TRUE(read_result.ok()) << "Read failed for stats test " << i;
    }
    
    auto read_stats = getDetailedPoolStats();
    
    // Query phase
    for (int i = 0; i < controlled_operations / 10; ++i) {
        std::vector<std::pair<std::string, std::string>> matchers;
        matchers.emplace_back("test", "phase1");
        matchers.emplace_back("pool_test", "true");
        
        auto query_result = storage_->query(matchers, 1000, 1030);
        ASSERT_TRUE(query_result.ok()) << "Query failed for stats test batch " << i;
    }
    
    auto final_stats = getDetailedPoolStats();
    
    // Validate statistics progression
    EXPECT_GE(write_stats.time_series_total_created, baseline_stats.time_series_total_created)
        << "TimeSeries pool creation count should increase after writes";
    EXPECT_GE(read_stats.time_series_total_acquired, write_stats.time_series_total_acquired)
        << "TimeSeries pool acquisition count should increase after reads";
    EXPECT_GE(final_stats.time_series_total_acquired, read_stats.time_series_total_acquired)
        << "TimeSeries pool acquisition count should increase after queries";
    
    // Calculate and validate reuse rates
    double write_reuse_rate = (write_stats.time_series_total_acquired > 0) ? 
        (double)(write_stats.time_series_total_acquired - write_stats.time_series_total_created) / write_stats.time_series_total_acquired : 0.0;
    
    double read_reuse_rate = (read_stats.time_series_total_acquired > write_stats.time_series_total_acquired) ? 
        (double)(read_stats.time_series_total_acquired - write_stats.time_series_total_created) / (read_stats.time_series_total_acquired - write_stats.time_series_total_acquired) : 0.0;
    
    double final_reuse_rate = (final_stats.time_series_total_acquired > read_stats.time_series_total_acquired) ? 
        (double)(final_stats.time_series_total_acquired - write_stats.time_series_total_created) / (final_stats.time_series_total_acquired - read_stats.time_series_total_acquired) : 0.0;
    
    std::cout << "Statistics Validation Results:" << std::endl;
    std::cout << "  Write phase reuse rate: " << std::fixed << std::setprecision(2) << (write_reuse_rate * 100) << "%" << std::endl;
    std::cout << "  Read phase reuse rate: " << std::fixed << std::setprecision(2) << (read_reuse_rate * 100) << "%" << std::endl;
    std::cout << "  Final reuse rate: " << std::fixed << std::setprecision(2) << (final_reuse_rate * 100) << "%" << std::endl;
    std::cout << "  Total TimeSeries created: " << final_stats.time_series_total_created << std::endl;
    std::cout << "  Total TimeSeries acquired: " << final_stats.time_series_total_acquired << std::endl;
    std::cout << "  Total TimeSeries released: " << final_stats.time_series_total_released << std::endl;
    
    // Validate statistics consistency
    EXPECT_GE(final_stats.time_series_total_acquired, final_stats.time_series_total_released)
        << "Total acquired should be >= total released";
    EXPECT_GT(final_reuse_rate, 0.3) << "Final reuse rate should be significant";
    
    // Test statistics string generation
    std::string stats_string = storage_->stats();
    EXPECT_FALSE(stats_string.empty()) << "Stats string should not be empty";
    EXPECT_TRUE(stats_string.find("Storage Statistics:") != std::string::npos) << "Stats should contain header";
    EXPECT_TRUE(stats_string.find("TimeSeriesPool") != std::string::npos) << "Stats should contain TimeSeriesPool info";
    EXPECT_TRUE(stats_string.find("LabelsPool") != std::string::npos) << "Stats should contain LabelsPool info";
    EXPECT_TRUE(stats_string.find("SamplePool") != std::string::npos) << "Stats should contain SamplePool info";
}

// Test 6: Performance Benchmarking and Optimization Validation
TEST_F(Phase1ObjectPoolIntegrationTest, PerformanceBenchmarkingAndOptimization) {
    std::cout << "Testing performance benchmarking and optimization validation" << std::endl;
    
    const int benchmark_operations = 2000;
    const int samples_per_series = 75;
    
    // Benchmark 1: Write performance
    {
        reset_performance_metrics();
        
        for (int i = 0; i < benchmark_operations; ++i) {
            auto series = createTestSeries("benchmark_write_" + std::to_string(i), samples_per_series);
            auto write_result = storage_->write(series);
            ASSERT_TRUE(write_result.ok()) << "Benchmark write failed at " << i;
        }
        
        double write_time = get_elapsed_time_ms();
        double write_ops_per_sec = benchmark_operations / (write_time / 1000.0);
        
        std::cout << "Write Performance Benchmark:" << std::endl;
        std::cout << "  Operations: " << benchmark_operations << std::endl;
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << write_time << " ms" << std::endl;
        std::cout << "  Operations per second: " << std::fixed << std::setprecision(2) << write_ops_per_sec << std::endl;
        
        EXPECT_GT(write_ops_per_sec, 100) << "Write performance too slow: " << write_ops_per_sec << " ops/sec";
    }
    
    // Benchmark 2: Read performance
    {
        reset_performance_metrics();
        
        for (int i = 0; i < benchmark_operations; ++i) {
            core::Labels query_labels;
            query_labels.add("__name__", "benchmark_write_" + std::to_string(i));
            query_labels.add("test", "phase1");
            
            auto read_result = storage_->read(query_labels, 1000, 1000 + samples_per_series);
            ASSERT_TRUE(read_result.ok()) << "Benchmark read failed at " << i;
        }
        
        double read_time = get_elapsed_time_ms();
        double read_ops_per_sec = benchmark_operations / (read_time / 1000.0);
        
        std::cout << "Read Performance Benchmark:" << std::endl;
        std::cout << "  Operations: " << benchmark_operations << std::endl;
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << read_time << " ms" << std::endl;
        std::cout << "  Operations per second: " << std::fixed << std::setprecision(2) << read_ops_per_sec << std::endl;
        
        EXPECT_GT(read_ops_per_sec, 100) << "Read performance too slow: " << read_ops_per_sec << " ops/sec";
    }
    
    // Benchmark 3: Query performance
    {
        reset_performance_metrics();
        
        for (int i = 0; i < benchmark_operations / 10; ++i) {
            std::vector<std::pair<std::string, std::string>> matchers;
            matchers.emplace_back("test", "phase1");
            matchers.emplace_back("pool_test", "true");
            
            auto query_result = storage_->query(matchers, 1000, 1000 + samples_per_series);
            ASSERT_TRUE(query_result.ok()) << "Benchmark query failed at " << i;
            EXPECT_GT(query_result.value().size(), 0) << "Benchmark query returned no results at " << i;
        }
        
        double query_time = get_elapsed_time_ms();
        double query_ops_per_sec = (benchmark_operations / 10) / (query_time / 1000.0);
        
        std::cout << "Query Performance Benchmark:" << std::endl;
        std::cout << "  Operations: " << (benchmark_operations / 10) << std::endl;
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << query_time << " ms" << std::endl;
        std::cout << "  Operations per second: " << std::fixed << std::setprecision(2) << query_ops_per_sec << std::endl;
        
        EXPECT_GT(query_ops_per_sec, 10) << "Query performance too slow: " << query_ops_per_sec << " ops/sec";
    }
    
    // Benchmark 4: Mixed workload performance
    {
        reset_performance_metrics();
        
        std::vector<std::thread> benchmark_threads;
        std::atomic<int> total_operations{0};
        
        for (int t = 0; t < 4; ++t) {
            benchmark_threads.emplace_back([this, t, benchmark_operations, &total_operations]() {
                for (int i = 0; i < benchmark_operations; ++i) {
                    // Write
                    auto series = createTestSeries("mixed_benchmark_" + std::to_string(t) + "_" + std::to_string(i), 50);
                    if (storage_->write(series).ok()) {
                        total_operations++;
                    }
                    
                    // Read
                    core::Labels query_labels;
                    query_labels.add("__name__", "mixed_benchmark_" + std::to_string(t) + "_" + std::to_string(i));
                    query_labels.add("test", "phase1");
                    
                    if (storage_->read(query_labels, 1000, 1050).ok()) {
                        total_operations++;
                    }
                }
            });
        }
        
        for (auto& thread : benchmark_threads) {
            thread.join();
        }
        
        double mixed_time = get_elapsed_time_ms();
        double mixed_ops_per_sec = total_operations.load() / (mixed_time / 1000.0);
        
        std::cout << "Mixed Workload Performance Benchmark:" << std::endl;
        std::cout << "  Total operations: " << total_operations.load() << std::endl;
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << mixed_time << " ms" << std::endl;
        std::cout << "  Operations per second: " << std::fixed << std::setprecision(2) << mixed_ops_per_sec << std::endl;
        
        EXPECT_GT(mixed_ops_per_sec, 200) << "Mixed workload performance too slow: " << mixed_ops_per_sec << " ops/sec";
    }
    
    // Final pool efficiency validation
    validatePoolEfficiency("PerformanceBenchmarking", benchmark_operations * 2);
}

} // namespace integration
} // namespace tsdb 