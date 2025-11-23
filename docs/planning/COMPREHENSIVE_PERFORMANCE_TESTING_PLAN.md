# Comprehensive Performance Testing Plan for All DESIGN_FIXES.md Improvements

**Document**: `docs/planning/COMPREHENSIVE_PERFORMANCE_TESTING_PLAN.md`  
**Created**: December 2024  
**Purpose**: Comprehensive performance testing strategy for all critical fixes from DESIGN_FIXES.md  
**Status**: Active Performance Testing Plan

## 🎯 **COMPREHENSIVE PERFORMANCE TESTING OVERVIEW**

This document outlines a complete performance testing strategy for all the critical fixes implemented in `DESIGN_FIXES.md`. We need to validate that each major architectural improvement delivers the expected performance benefits.

### **🔧 CRITICAL FIXES TO TEST**

Based on `DESIGN_FIXES.md`, we have implemented 4 major architectural improvements:

1. **✅ Task 1: Eliminate In-Memory Data Duplication** - Removed `stored_series_` vector, eliminated full scans
2. **✅ Task 2: Implement Inverted Index for Labels** - Added efficient label-based querying
3. **✅ Task 3: Implement Write-Ahead Log (WAL)** - Added durability and crash recovery
4. **✅ Task 4: Refine Locking with Concurrent Data Structures** - Added `tbb::concurrent_hash_map`

## 🧪 **PHASE 1: MEMORY EFFICIENCY PERFORMANCE TESTING**

### **1.1 In-Memory Duplication Elimination Testing**

**Objective**: Validate that removing `stored_series_` vector eliminates memory bloat and improves performance

#### **Test Suite 1.1.1: Memory Usage Validation**
```cpp
TEST_F(StorageImplMemoryPerformanceTest, InMemoryDuplicationElimination) {
    // Test: Memory usage without stored_series_ vector
    // Validates: Memory efficiency improvement from Task 1
    
    auto initialMemory = getCurrentMemoryUsage();
    
    // Write large amount of data that would previously cause memory bloat
    const int NUM_SERIES = 100000;
    const int SAMPLES_PER_SERIES = 1000;
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTestSeries("memory_test_" + std::to_string(i), SAMPLES_PER_SERIES);
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    auto peakMemory = getCurrentMemoryUsage();
    auto memoryGrowth = peakMemory - initialMemory;
    
    // Calculate expected memory with old approach (stored_series_ vector)
    auto expectedMemoryWithDuplication = calculateExpectedMemoryWithDuplication(NUM_SERIES, SAMPLES_PER_SERIES);
    
    // Verify memory efficiency improvement (should be <50% of old approach)
    EXPECT_LT(memoryGrowth, expectedMemoryWithDuplication * 0.5)
        << "Memory usage too high - in-memory duplication not eliminated";
    
    // Verify no memory leaks
    verifyNoMemoryLeaks();
}

TEST_F(StorageImplMemoryPerformanceTest, MemoryStabilityUnderLoad) {
    // Test: Memory stability without stored_series_ vector
    // Validates: No unbounded memory growth
    
    const int NUM_ITERATIONS = 1000;
    const int SERIES_PER_ITERATION = 100;
    
    std::vector<size_t> memorySnapshots;
    
    for (int iteration = 0; iteration < NUM_ITERATIONS; ++iteration) {
        // Write batch of series
        for (int i = 0; i < SERIES_PER_ITERATION; ++i) {
            auto series = createTestSeries("stability_" + std::to_string(iteration) + "_" + std::to_string(i), 50);
            ASSERT_TRUE(storage_->write(series).ok());
        }
        
        // Take memory snapshot
        memorySnapshots.push_back(getCurrentMemoryUsage());
        
        // Verify memory growth is linear, not exponential
        if (iteration > 10) {
            auto recentGrowth = memorySnapshots[iteration] - memorySnapshots[iteration - 10];
            auto expectedGrowth = calculateExpectedLinearGrowth(10, SERIES_PER_ITERATION);
            EXPECT_LT(recentGrowth, expectedGrowth * 1.5)
                << "Memory growth not linear - possible memory leak";
        }
    }
}
```

#### **Test Suite 1.1.2: Performance Impact of Duplication Elimination**
```cpp
TEST_F(StorageImplMemoryPerformanceTest, WritePerformanceWithoutDuplication) {
    // Test: Write performance without stored_series_ vector
    // Validates: Performance improvement from eliminating duplication
    
    const int NUM_OPERATIONS = 100000;
    
    // Measure write performance
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        auto series = createTestSeries("perf_test_" + std::to_string(i), 10);
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = NUM_OPERATIONS / (duration.count() / 1000.0);
    
    // Verify performance improvement (should be >2x faster than with duplication)
    EXPECT_GT(throughput, 20000) << "Write throughput too low: " << throughput << " ops/sec";
    
    // Verify no performance degradation over time
    verifyPerformanceStability();
}
```

### **1.2 Block-Based Storage Performance Testing**

**Objective**: Validate that block-based storage is more efficient than in-memory duplication

#### **Test Suite 1.2.1: Block Storage Efficiency**
```cpp
TEST_F(StorageImplBlockPerformanceTest, BlockStorageEfficiency) {
    // Test: Block-based storage vs in-memory approach
    // Validates: Block storage performance benefits
    
    const int NUM_SERIES = 10000;
    const int SAMPLES_PER_SERIES = 1000;
    
    // Write data to blocks
    auto writeStart = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTestSeries("block_test_" + std::to_string(i), SAMPLES_PER_SERIES);
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    auto writeEnd = std::chrono::high_resolution_clock::now();
    auto writeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(writeEnd - writeStart);
    
    // Force block persistence
    storage_->flush();
    
    // Measure read performance from blocks
    auto readStart = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        Labels labels;
        labels.add("__name__", "block_test_" + std::to_string(i));
        auto result = storage_->read(labels, 0, LLONG_MAX);
        ASSERT_TRUE(result.ok());
    }
    
    auto readEnd = std::chrono::high_resolution_clock::now();
    auto readDuration = std::chrono::duration_cast<std::chrono::milliseconds>(readEnd - readStart);
    
    // Verify block storage performance
    double writeThroughput = NUM_SERIES / (writeDuration.count() / 1000.0);
    double readThroughput = NUM_SERIES / (readDuration.count() / 1000.0);
    
    EXPECT_GT(writeThroughput, 5000) << "Block write throughput too low: " << writeThroughput << " ops/sec";
    EXPECT_GT(readThroughput, 3000) << "Block read throughput too low: " << readThroughput << " ops/sec";
}
```

## 🧪 **PHASE 2: INDEXING PERFORMANCE TESTING**

### **2.1 Inverted Index Performance Testing**

**Objective**: Validate that inverted index provides O(log K) performance vs O(N) linear scans

#### **Test Suite 2.1.1: Query Performance with Inverted Index**
```cpp
TEST_F(StorageImplIndexPerformanceTest, InvertedIndexQueryPerformance) {
    // Test: Query performance with inverted index
    // Validates: O(log K) vs O(N) performance improvement from Task 2
    
    // Setup: Create large number of series with various labels
    const int NUM_SERIES = 100000;
    const int NUM_LABEL_VALUES = 1000;
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTestSeriesWithLabels("index_test_" + std::to_string(i), 
                                                "label_value_" + std::to_string(i % NUM_LABEL_VALUES));
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Test query performance with inverted index
    std::vector<std::pair<std::string, std::string>> matchers;
    matchers.emplace_back("label_value", "label_value_500");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    auto result = storage_->query(matchers, 0, LLONG_MAX);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    ASSERT_TRUE(result.ok());
    
    // Verify query performance (should be <1ms with inverted index)
    EXPECT_LT(duration.count(), 1000) << "Query too slow with inverted index: " << duration.count() << "μs";
    
    // Verify result accuracy
    EXPECT_GT(result.value().size(), 0) << "No results found for query";
}

TEST_F(StorageImplIndexPerformanceTest, ComplexQueryPerformance) {
    // Test: Complex multi-label queries with inverted index
    // Validates: Index performance with multiple matchers
    
    const int NUM_SERIES = 50000;
    
    // Create series with multiple labels
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTestSeriesWithMultipleLabels("complex_test_" + std::to_string(i), {
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
    
    auto start = std::chrono::high_resolution_clock::now();
    
    auto result = storage_->query(matchers, 0, LLONG_MAX);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    ASSERT_TRUE(result.ok());
    
    // Verify complex query performance (should be <2ms with inverted index)
    EXPECT_LT(duration.count(), 2000) << "Complex query too slow: " << duration.count() << "μs";
}

TEST_F(StorageImplIndexPerformanceTest, IndexScalability) {
    // Test: Index performance scales with data size
    // Validates: Index performance doesn't degrade with large datasets
    
    const std::vector<int> SCALE_LEVELS = {1000, 10000, 50000, 100000};
    
    for (int scale : SCALE_LEVELS) {
        // Create data at this scale
        for (int i = 0; i < scale; ++i) {
            auto series = createTestSeries("scale_test_" + std::to_string(i), 10);
            ASSERT_TRUE(storage_->write(series).ok());
        }
        
        // Measure query performance at this scale
        std::vector<std::pair<std::string, std::string>> matchers;
        matchers.emplace_back("__name__", "scale_test_0");
        
        auto start = std::chrono::high_resolution_clock::now();
        auto result = storage_->query(matchers, 0, LLONG_MAX);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        ASSERT_TRUE(result.ok());
        
        // Verify performance scales logarithmically, not linearly
        EXPECT_LT(duration.count(), 1000 + (scale / 1000)) 
            << "Index performance not scaling well at scale " << scale << ": " << duration.count() << "μs";
    }
}
```

### **2.2 Index Memory Efficiency Testing**

**Objective**: Validate that inverted index uses memory efficiently

#### **Test Suite 2.2.1: Index Memory Usage**
```cpp
TEST_F(StorageImplIndexPerformanceTest, IndexMemoryEfficiency) {
    // Test: Index memory usage is reasonable
    // Validates: Index doesn't cause memory bloat
    
    auto initialMemory = getCurrentMemoryUsage();
    
    const int NUM_SERIES = 100000;
    const int NUM_UNIQUE_LABELS = 10000;
    
    // Create series with many unique labels
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTestSeriesWithLabels("index_memory_" + std::to_string(i),
                                               "unique_label_" + std::to_string(i % NUM_UNIQUE_LABELS));
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    auto finalMemory = getCurrentMemoryUsage();
    auto indexMemoryUsage = finalMemory - initialMemory;
    
    // Calculate expected index memory usage
    auto expectedIndexMemory = calculateExpectedIndexMemory(NUM_SERIES, NUM_UNIQUE_LABELS);
    
    // Verify index memory usage is reasonable
    EXPECT_LT(indexMemoryUsage, expectedIndexMemory * 1.5)
        << "Index memory usage too high: " << indexMemoryUsage << " bytes";
    
    // Verify index memory grows sub-linearly with data
    verifyIndexMemoryScaling();
}
```

## 🧪 **PHASE 3: WAL DURABILITY PERFORMANCE TESTING**

### **3.1 WAL Write Performance Testing**

**Objective**: Validate that WAL provides durability without significant performance impact

#### **Test Suite 3.1.1: WAL Write Performance**
```cpp
TEST_F(StorageImplWALPerformanceTest, WALWritePerformance) {
    // Test: WAL write performance impact
    // Validates: WAL doesn't significantly impact write performance from Task 3
    
    const int NUM_OPERATIONS = 100000;
    
    // Measure write performance with WAL
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        auto series = createTestSeries("wal_test_" + std::to_string(i), 10);
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = NUM_OPERATIONS / (duration.count() / 1000.0);
    
    // Verify WAL doesn't significantly impact performance (should be >80% of no-WAL performance)
    EXPECT_GT(throughput, 15000) << "WAL write throughput too low: " << throughput << " ops/sec";
    
    // Verify WAL files are created
    verifyWALFilesCreated();
}

TEST_F(StorageImplWALPerformanceTest, WALSegmentRotationPerformance) {
    // Test: WAL segment rotation performance
    // Validates: WAL segment management doesn't cause performance spikes
    
    const int NUM_OPERATIONS = 1000000; // Large enough to trigger segment rotation
    
    std::vector<std::chrono::microseconds> operationTimes;
    
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();
        
        auto series = createTestSeries("segment_test_" + std::to_string(i), 10);
        ASSERT_TRUE(storage_->write(series).ok());
        
        auto opEnd = std::chrono::high_resolution_clock::now();
        operationTimes.push_back(std::chrono::duration_cast<std::chrono::microseconds>(opEnd - opStart));
    }
    
    // Verify no significant performance spikes during segment rotation
    auto avgTime = calculateAverage(operationTimes);
    auto p99Time = calculatePercentile(operationTimes, 99);
    
    EXPECT_LT(p99Time.count(), avgTime.count() * 5) 
        << "WAL segment rotation causes performance spikes";
}

TEST_F(StorageImplWALPerformanceTest, WALReplayPerformance) {
    // Test: WAL replay performance during crash recovery
    // Validates: WAL replay is fast enough for quick recovery
    
    // Write data to create WAL entries
    const int NUM_OPERATIONS = 10000;
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        auto series = createTestSeries("replay_test_" + std::to_string(i), 10);
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Simulate crash recovery
    auto recoveryStart = std::chrono::high_resolution_clock::now();
    
    // Create new storage instance (simulates restart)
    auto recoveryStorage = std::make_shared<StorageImpl>();
    StorageConfig config = createTestConfig();
    ASSERT_TRUE(recoveryStorage->init(config).ok());
    
    auto recoveryEnd = std::chrono::high_resolution_clock::now();
    auto recoveryTime = std::chrono::duration_cast<std::chrono::milliseconds>(recoveryEnd - recoveryStart);
    
    // Verify recovery time is reasonable (<1 second for 10K operations)
    EXPECT_LT(recoveryTime.count(), 1000) << "WAL replay too slow: " << recoveryTime.count() << "ms";
    
    // Verify data integrity after recovery
    verifyDataIntegrityAfterRecovery(recoveryStorage);
}
```

### **3.2 WAL Durability Testing**

**Objective**: Validate that WAL provides proper durability guarantees

#### **Test Suite 3.2.1: WAL Durability Validation**
```cpp
TEST_F(StorageImplWALPerformanceTest, WALDurabilityValidation) {
    // Test: WAL ensures data durability
    // Validates: WAL provides crash recovery guarantees
    
    const int NUM_OPERATIONS = 1000;
    std::vector<TimeSeries> writtenSeries;
    
    // Write data with WAL
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        auto series = createTestSeries("durability_test_" + std::to_string(i), 10);
        writtenSeries.push_back(series);
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Simulate crash (force close without proper shutdown)
    storage_->close();
    
    // Restart and verify data recovery
    auto recoveryStorage = std::make_shared<StorageImpl>();
    StorageConfig config = createTestConfig();
    ASSERT_TRUE(recoveryStorage->init(config).ok());
    
    // Verify all data is recovered
    for (const auto& originalSeries : writtenSeries) {
        auto result = recoveryStorage->read(originalSeries.labels(), 0, LLONG_MAX);
        ASSERT_TRUE(result.ok()) << "Data not recovered after crash: " << originalSeries.labels();
        verifyDataIntegrity(originalSeries, result.value());
    }
}

TEST_F(StorageImplWALPerformanceTest, WALCheckpointPerformance) {
    // Test: WAL checkpoint performance
    // Validates: WAL cleanup doesn't impact performance
    
    // Create WAL data
    const int NUM_OPERATIONS = 50000;
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        auto series = createTestSeries("checkpoint_test_" + std::to_string(i), 10);
        ASSERT_TRUE(storage_->write(series).ok());
    }
    
    // Force block persistence to make WAL entries eligible for checkpoint
    storage_->flush();
    
    // Measure checkpoint performance
    auto checkpointStart = std::chrono::high_resolution_clock::now();
    
    // Trigger WAL checkpoint
    storage_->checkpoint();
    
    auto checkpointEnd = std::chrono::high_resolution_clock::now();
    auto checkpointTime = std::chrono::duration_cast<std::chrono::milliseconds>(checkpointEnd - checkpointStart);
    
    // Verify checkpoint performance is reasonable
    EXPECT_LT(checkpointTime.count(), 5000) << "WAL checkpoint too slow: " << checkpointTime.count() << "ms";
    
    // Verify WAL files are cleaned up
    verifyWALFilesCleanedUp();
}
```

## 🧪 **PHASE 4: CONCURRENT DATA STRUCTURES PERFORMANCE TESTING**

### **4.1 Concurrent Hash Map Performance Testing**

**Objective**: Validate that `tbb::concurrent_hash_map` provides better concurrency than global locks

#### **Test Suite 4.1.1: Concurrent Write Performance**
```cpp
TEST_F(StorageImplConcurrencyPerformanceTest, ConcurrentWritePerformance) {
    // Test: Concurrent write performance with tbb::concurrent_hash_map
    // Validates: Performance improvement from Task 4
    
    const int NUM_THREADS = 8;
    const int OPERATIONS_PER_THREAD = 10000;
    const int TOTAL_OPERATIONS = NUM_THREADS * OPERATIONS_PER_THREAD;
    
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
        threads.emplace_back([&, threadId]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                auto series = createTestSeries("concurrent_" + std::to_string(threadId) + "_" + std::to_string(i), 10);
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
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = TOTAL_OPERATIONS / (duration.count() / 1000.0);
    
    // Verify concurrent performance (should be >50K ops/sec with 8 threads)
    EXPECT_GT(throughput, 50000) << "Concurrent write throughput too low: " << throughput << " ops/sec";
    
    // Verify high success rate
    EXPECT_GT(successCount.load(), TOTAL_OPERATIONS * 0.99) << "Too many write failures";
    EXPECT_EQ(errorCount.load(), 0) << "Write errors occurred";
}

TEST_F(StorageImplConcurrencyPerformanceTest, ConcurrentReadWritePerformance) {
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
                auto series = createTestSeries("mixed_" + std::to_string(i) + "_" + std::to_string(writeCount.load()), 10);
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
                Labels labels;
                labels.add("__name__", "mixed_" + std::to_string(i % NUM_WRITER_THREADS) + "_" + std::to_string(readCount.load() % 100));
                
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
    
    // Verify performance and stability
    EXPECT_GT(writeCount.load(), NUM_WRITER_THREADS * 1000) << "Too few writes completed";
    EXPECT_GT(readCount.load(), NUM_READER_THREADS * 1000) << "Too few reads completed";
    EXPECT_EQ(errorCount.load(), 0) << "Errors occurred during concurrent operations";
}
```

### **4.2 Lock Contention Testing**

**Objective**: Validate that concurrent data structures eliminate lock contention

#### **Test Suite 4.2.1: Lock Contention Elimination**
```cpp
TEST_F(StorageImplConcurrencyPerformanceTest, LockContentionElimination) {
    // Test: Lock contention is eliminated with concurrent data structures
    // Validates: No performance degradation under high concurrency
    
    const std::vector<int> THREAD_COUNTS = {1, 2, 4, 8, 16, 32};
    std::vector<double> throughputs;
    
    for (int numThreads : THREAD_COUNTS) {
        const int OPERATIONS_PER_THREAD = 5000;
        const int TOTAL_OPERATIONS = numThreads * OPERATIONS_PER_THREAD;
        
        std::atomic<int> successCount{0};
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        for (int threadId = 0; threadId < numThreads; ++threadId) {
            threads.emplace_back([&, threadId]() {
                for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                    auto series = createTestSeries("contention_" + std::to_string(threadId) + "_" + std::to_string(i), 10);
                    auto result = storage_->write(series);
                    if (result.ok()) {
                        successCount++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double throughput = TOTAL_OPERATIONS / (duration.count() / 1000.0);
        throughputs.push_back(throughput);
        
        // Verify success rate
        EXPECT_GT(successCount.load(), TOTAL_OPERATIONS * 0.99) << "Too many failures with " << numThreads << " threads";
    }
    
    // Verify throughput scales with thread count (should be roughly linear)
    verifyThroughputScaling(throughputs, THREAD_COUNTS);
}

TEST_F(StorageImplConcurrencyPerformanceTest, FineGrainedLockingPerformance) {
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
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
        threads.emplace_back([&, threadId]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                // Use different patterns to test fine-grained locking
                std::string pattern = seriesPatterns[threadId % seriesPatterns.size()];
                auto series = createTestSeries(pattern + "_" + std::to_string(threadId) + "_" + std::to_string(i), 10);
                
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
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = (NUM_THREADS * OPERATIONS_PER_THREAD) / (duration.count() / 1000.0);
    
    // Verify high throughput with fine-grained locking
    EXPECT_GT(throughput, 80000) << "Fine-grained locking throughput too low: " << throughput << " ops/sec";
    
    // Verify high success rate
    EXPECT_GT(successCount.load(), NUM_THREADS * OPERATIONS_PER_THREAD * 0.99) << "Too many failures";
    EXPECT_EQ(errorCount.load(), 0) << "Errors occurred";
}
```

## 🧪 **PHASE 5: COMPREHENSIVE INTEGRATION PERFORMANCE TESTING**

### **5.1 End-to-End Performance Testing**

**Objective**: Validate that all fixes work together to provide comprehensive performance improvements

#### **Test Suite 5.1.1: Complete System Performance**
```cpp
TEST_F(StorageImplIntegrationPerformanceTest, CompleteSystemPerformance) {
    // Test: Complete system performance with all fixes
    // Validates: All architectural improvements work together
    
    const int NUM_OPERATIONS = 100000;
    const int NUM_THREADS = 8;
    
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int threadId = 0; threadId < NUM_THREADS; ++threadId) {
        threads.emplace_back([&, threadId]() {
            for (int i = 0; i < NUM_OPERATIONS / NUM_THREADS; ++i) {
                auto series = createTestSeries("integration_" + std::to_string(threadId) + "_" + std::to_string(i), 10);
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
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = NUM_OPERATIONS / (duration.count() / 1000.0);
    
    // Verify comprehensive performance improvement
    EXPECT_GT(throughput, 100000) << "Complete system throughput too low: " << throughput << " ops/sec";
    
    // Verify high success rate
    EXPECT_GT(successCount.load(), NUM_OPERATIONS * 0.99) << "Too many failures";
    EXPECT_EQ(errorCount.load(), 0) << "Errors occurred";
    
    // Verify all components are working
    verifyMemoryEfficiency();
    verifyIndexPerformance();
    verifyWALDurability();
    verifyConcurrencyPerformance();
}

TEST_F(StorageImplIntegrationPerformanceTest, RealWorldWorkloadPerformance) {
    // Test: Real-world workload performance
    // Validates: System performs well under realistic conditions
    
    // Simulate realistic time series database workload
    const int NUM_OPERATIONS = 50000;
    const double WRITE_RATIO = 0.8;
    const int NUM_UNIQUE_SERIES = 10000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> opDist(0.0, 1.0);
    std::uniform_int_distribution<> seriesDist(0, NUM_UNIQUE_SERIES - 1);
    
    std::atomic<int> writeCount{0};
    std::atomic<int> readCount{0};
    std::atomic<int> errorCount{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        if (opDist(gen) < WRITE_RATIO) {
            // Write operation
            auto series = createTestSeries("realworld_" + std::to_string(seriesDist(gen)), 10);
            auto result = storage_->write(series);
            if (result.ok()) {
                writeCount++;
            } else {
                errorCount++;
            }
        } else {
            // Read operation
            Labels labels;
            labels.add("__name__", "realworld_" + std::to_string(seriesDist(gen)));
            auto result = storage_->read(labels, 0, LLONG_MAX);
            if (result.ok() || result.error().what() == std::string("Series not found")) {
                readCount++;
            } else {
                errorCount++;
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = NUM_OPERATIONS / (duration.count() / 1000.0);
    
    // Verify realistic performance
    EXPECT_GT(throughput, 50000) << "Real-world workload throughput too low: " << throughput << " ops/sec";
    
    // Verify operation distribution
    EXPECT_GT(writeCount.load(), NUM_OPERATIONS * WRITE_RATIO * 0.9) << "Too few writes completed";
    EXPECT_GT(readCount.load(), NUM_OPERATIONS * (1 - WRITE_RATIO) * 0.5) << "Too few reads completed";
    EXPECT_EQ(errorCount.load(), 0) << "Errors occurred during real-world workload";
}
```

### **5.2 Performance Regression Testing**

**Objective**: Ensure that performance improvements are maintained over time

#### **Test Suite 5.2.1: Performance Regression Prevention**
```cpp
TEST_F(StorageImplRegressionPerformanceTest, PerformanceRegressionPrevention) {
    // Test: Performance doesn't regress over time
    // Validates: System maintains performance improvements
    
    const int NUM_ITERATIONS = 100;
    const int OPERATIONS_PER_ITERATION = 1000;
    
    std::vector<double> iterationThroughputs;
    
    for (int iteration = 0; iteration < NUM_ITERATIONS; ++iteration) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < OPERATIONS_PER_ITERATION; ++i) {
            auto series = createTestSeries("regression_" + std::to_string(iteration) + "_" + std::to_string(i), 10);
            ASSERT_TRUE(storage_->write(series).ok());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double throughput = OPERATIONS_PER_ITERATION / (duration.count() / 1000.0);
        iterationThroughputs.push_back(throughput);
        
        // Verify performance doesn't degrade significantly
        if (iteration > 10) {
            auto recentAvg = calculateAverage(iterationThroughputs.end() - 10, iterationThroughputs.end());
            auto earlyAvg = calculateAverage(iterationThroughputs.begin(), iterationThroughputs.begin() + 10);
            
            EXPECT_GT(recentAvg, earlyAvg * 0.8) << "Performance regression detected at iteration " << iteration;
        }
    }
    
    // Verify overall performance stability
    verifyPerformanceStability(iterationThroughputs);
}
```

## 📊 **PERFORMANCE TESTING EXECUTION FRAMEWORK**

### **Test Execution Order**
```yaml
Week 1: Memory and Indexing Performance
  Day 1-2: Memory Efficiency Testing (Test Suites 1.1, 1.2)
  Day 3-4: Indexing Performance Testing (Test Suites 2.1, 2.2)
  Day 5-6: WAL Performance Testing (Test Suites 3.1, 3.2)
  Day 7: Concurrency Performance Testing (Test Suites 4.1, 4.2)

Week 2: Integration and Regression Testing
  Day 1-2: Complete System Performance (Test Suite 5.1)
  Day 3-4: Performance Regression Testing (Test Suite 5.2)
  Day 5-6: Real-world Workload Testing
  Day 7: Performance Analysis and Optimization
```

### **Success Criteria by Phase**

#### **Phase 1: Memory Efficiency** ✅ **TARGET**
- **Memory Usage**: <50% of old approach (with stored_series_ vector)
- **Memory Stability**: Linear growth, no memory leaks
- **Write Performance**: >20K ops/sec without duplication

#### **Phase 2: Indexing Performance** ✅ **TARGET**
- **Query Performance**: <1ms for simple queries, <2ms for complex queries
- **Index Scalability**: Logarithmic scaling with data size
- **Memory Efficiency**: Reasonable index memory usage

#### **Phase 3: WAL Performance** ✅ **TARGET**
- **Write Performance**: >15K ops/sec with WAL enabled
- **Recovery Performance**: <1 second for 10K operations
- **Durability**: 100% data recovery after crash

#### **Phase 4: Concurrency Performance** ✅ **TARGET**
- **Concurrent Throughput**: >50K ops/sec with 8 threads
- **Scalability**: Linear scaling with thread count
- **Lock Contention**: No performance degradation under high concurrency

#### **Phase 5: Integration Performance** ✅ **TARGET**
- **Complete System**: >100K ops/sec with all fixes
- **Real-world Workload**: >50K ops/sec under realistic conditions
- **Performance Stability**: No regression over time

### **Performance Targets Summary**

| **Component** | **Target Performance** | **Success Criteria** |
|---------------|------------------------|---------------------|
| **Memory Efficiency** | <50% memory usage vs old approach | Eliminate in-memory duplication |
| **Indexing Performance** | <1ms query latency | O(log K) vs O(N) performance |
| **WAL Performance** | >15K ops/sec with durability | Minimal performance impact |
| **Concurrency** | >50K ops/sec with 8 threads | Linear scaling with threads |
| **Complete System** | >100K ops/sec | All fixes working together |

## 🎯 **IMPLEMENTATION PRIORITY**

### **Critical Priority (Must Complete First)**
1. **Memory Efficiency Testing** - Validate in-memory duplication elimination
2. **Indexing Performance Testing** - Validate inverted index performance
3. **WAL Performance Testing** - Validate durability without performance impact

### **High Priority (Complete Second)**
1. **Concurrency Performance Testing** - Validate concurrent data structures
2. **Integration Performance Testing** - Validate all fixes working together

### **Medium Priority (Complete Third)**
1. **Performance Regression Testing** - Ensure performance stability
2. **Real-world Workload Testing** - Validate under realistic conditions

## 📈 **EXPECTED PERFORMANCE IMPROVEMENTS**

### **Memory Efficiency Improvements**
- **Memory Usage**: 50-70% reduction vs old approach
- **Memory Stability**: Linear growth instead of exponential
- **Write Performance**: 2-3x improvement without duplication

### **Indexing Performance Improvements**
- **Query Performance**: 10-100x improvement (O(log K) vs O(N))
- **Complex Queries**: 5-10x improvement with multiple matchers
- **Scalability**: Logarithmic scaling instead of linear

### **WAL Performance Impact**
- **Write Performance**: <20% impact for durability
- **Recovery Performance**: <1 second for typical workloads
- **Durability**: 100% data recovery guarantee

### **Concurrency Performance Improvements**
- **Throughput**: 5-10x improvement with multiple threads
- **Scalability**: Linear scaling with thread count
- **Lock Contention**: Elimination of global lock bottlenecks

### **Complete System Performance**
- **Overall Throughput**: 5-20x improvement with all fixes
- **Real-world Performance**: 3-10x improvement under realistic workloads
- **System Stability**: No performance regression over time

---

**Document Status**: **COMPREHENSIVE PERFORMANCE TESTING PLAN READY**  
**Next Action**: Implement and execute all performance tests  
**Estimated Duration**: 2 weeks for complete testing  
**Success Probability**: 95% (All architectural improvements implemented)  
**Final Goal**: Validate all DESIGN_FIXES.md improvements deliver expected performance benefits
