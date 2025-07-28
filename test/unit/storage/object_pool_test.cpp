#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <random>

#include "tsdb/storage/object_pool.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {
namespace test {

class ObjectPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test fixtures
    }
    
    void TearDown() override {
        // Clean up test fixtures
    }
};

// ============================================================================
// TimeSeriesPool Tests
// ============================================================================

TEST_F(ObjectPoolTest, TimeSeriesPoolBasicOperations) {
    TimeSeriesPool pool(10, 100);
    
    // Test initial state
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.total_created(), 10);
    EXPECT_EQ(pool.max_size(), 100);
    
    // Test acquire
    auto obj1 = pool.acquire();
    EXPECT_NE(obj1, nullptr);
    EXPECT_EQ(pool.available(), 9);
    EXPECT_EQ(pool.total_created(), 10); // No new objects created yet
    
    // Test release
    pool.release(std::move(obj1));
    EXPECT_EQ(pool.available(), 10);
    
    // Test multiple acquires
    std::vector<std::unique_ptr<core::TimeSeries>> objects;
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool.acquire());
        EXPECT_NE(objects.back(), nullptr);
    }
    EXPECT_EQ(pool.available(), 5);
    
    // Test multiple releases
    for (auto& obj : objects) {
        pool.release(std::move(obj));
    }
    EXPECT_EQ(pool.available(), 10);
}

TEST_F(ObjectPoolTest, TimeSeriesPoolObjectReuse) {
    TimeSeriesPool pool(5, 10);
    
    // Acquire and release objects multiple times
    for (int i = 0; i < 20; ++i) {
        auto obj = pool.acquire();
        EXPECT_NE(obj, nullptr);
        
        // Add some data to the object
        obj->add_sample(1000 + i, 42.0 + i);
        
        pool.release(std::move(obj));
    }
    
    // Check that we're reusing objects (not creating new ones)
    EXPECT_LE(pool.total_created(), 10); // Should not exceed max size
    // Note: With efficient pooling, we might not create new objects if pool size is sufficient
}

TEST_F(ObjectPoolTest, TimeSeriesPoolMaxSizeLimit) {
    TimeSeriesPool pool(2, 3);
    
    // Acquire all available objects
    auto obj1 = pool.acquire();
    auto obj2 = pool.acquire();
    auto obj3 = pool.acquire(); // This should create a new object
    
    EXPECT_EQ(pool.available(), 0);
    EXPECT_EQ(pool.total_created(), 3);
    
    // Release one object
    pool.release(std::move(obj1));
    EXPECT_EQ(pool.available(), 1);
    
    // Acquire again - should reuse the released object
    auto obj4 = pool.acquire();
    EXPECT_EQ(pool.available(), 0);
    EXPECT_EQ(pool.total_created(), 3); // Should not create new object
    
    // Release all objects
    pool.release(std::move(obj2));
    pool.release(std::move(obj3));
    pool.release(std::move(obj4));
    
    // Should not exceed max size
    EXPECT_EQ(pool.available(), 3);
}

TEST_F(ObjectPoolTest, TimeSeriesPoolThreadSafety) {
    TimeSeriesPool pool(10, 100);
    const int num_threads = 4;
    const int operations_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> total_operations{0};
    
    // Start multiple threads that acquire and release objects
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&pool, operations_per_thread, &total_operations]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 10);
            
            for (int j = 0; j < operations_per_thread; ++j) {
                auto obj = pool.acquire();
                EXPECT_NE(obj, nullptr);
                
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::microseconds(dis(gen)));
                
                pool.release(std::move(obj));
                total_operations.fetch_add(1);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(total_operations.load(), num_threads * operations_per_thread);
    
    // Check that pool is still in a valid state
    EXPECT_LE(pool.available(), pool.max_size());
    EXPECT_GT(pool.total_created(), 0);
}

TEST_F(ObjectPoolTest, TimeSeriesPoolStats) {
    TimeSeriesPool pool(5, 10);
    
    // Perform some operations
    auto obj1 = pool.acquire();
    auto obj2 = pool.acquire();
    pool.release(std::move(obj1));
    
    std::string stats = pool.stats();
    
    // Check that stats contain expected information
    EXPECT_NE(stats.find("TimeSeriesPool Statistics"), std::string::npos);
    EXPECT_NE(stats.find("Available objects"), std::string::npos);
    EXPECT_NE(stats.find("Total created"), std::string::npos);
    EXPECT_NE(stats.find("Total acquired"), std::string::npos);
    EXPECT_NE(stats.find("Total released"), std::string::npos);
    EXPECT_NE(stats.find("Object reuse ratio"), std::string::npos);
}

// ============================================================================
// LabelsPool Tests
// ============================================================================

TEST_F(ObjectPoolTest, LabelsPoolBasicOperations) {
    LabelsPool pool(10, 100);
    
    // Test initial state
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.total_created(), 10);
    
    // Test acquire and release
    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.available(), 9);
    
    // Add some labels
    obj->add("test_key", "test_value");
    EXPECT_EQ(obj->size(), 1);
    
    pool.release(std::move(obj));
    EXPECT_EQ(pool.available(), 10);
}

TEST_F(ObjectPoolTest, LabelsPoolObjectReuse) {
    LabelsPool pool(5, 10);
    
    // Acquire and release objects multiple times
    for (int i = 0; i < 15; ++i) {
        auto obj = pool.acquire();
        EXPECT_NE(obj, nullptr);
        
        // Add some labels
        obj->add("key_" + std::to_string(i), "value_" + std::to_string(i));
        EXPECT_EQ(obj->size(), 1);
        
        pool.release(std::move(obj));
    }
    
    // Check that we're reusing objects
    EXPECT_LE(pool.total_created(), 10);
    // Note: With efficient pooling, we might not create new objects if pool size is sufficient
}

// ============================================================================
// SamplePool Tests
// ============================================================================

TEST_F(ObjectPoolTest, SamplePoolBasicOperations) {
    SamplePool pool(10, 100);
    
    // Test initial state
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.total_created(), 10);
    
    // Test acquire and release
    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.available(), 9);
    
    // Check default values
    EXPECT_EQ(obj->timestamp(), 0);
    EXPECT_EQ(obj->value(), 0.0);
    
    pool.release(std::move(obj));
    EXPECT_EQ(pool.available(), 10);
}

TEST_F(ObjectPoolTest, SamplePoolObjectReuse) {
    SamplePool pool(5, 10);
    
    // Acquire and release objects multiple times
    for (int i = 0; i < 15; ++i) {
        auto obj = pool.acquire();
        EXPECT_NE(obj, nullptr);
        
        // Check that object is properly initialized
        EXPECT_EQ(obj->timestamp(), 0);
        EXPECT_EQ(obj->value(), 0.0);
        
        pool.release(std::move(obj));
    }
    
    // Check that we're reusing objects
    EXPECT_LE(pool.total_created(), 10);
    // Note: With efficient pooling, we might not create new objects if pool size is sufficient
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(ObjectPoolTest, TimeSeriesPoolPerformance) {
    TimeSeriesPool pool(100, 1000);
    const int num_operations = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform many acquire/release operations
    for (int i = 0; i < num_operations; ++i) {
        auto obj = pool.acquire();
        obj->add_sample(1000 + i, 42.0 + i);
        pool.release(std::move(obj));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance should be reasonable (less than 1ms per operation)
    double avg_time_per_op = static_cast<double>(duration.count()) / num_operations;
    EXPECT_LT(avg_time_per_op, 1000.0); // Less than 1ms per operation
    
    // Check reuse ratio
    double reuse_ratio = static_cast<double>(num_operations - pool.total_created()) / num_operations * 100.0;
    EXPECT_GT(reuse_ratio, 80.0); // Should reuse at least 80% of objects
}

TEST_F(ObjectPoolTest, LabelsPoolPerformance) {
    LabelsPool pool(200, 2000);
    const int num_operations = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform many acquire/release operations
    for (int i = 0; i < num_operations; ++i) {
        auto obj = pool.acquire();
        obj->add("key_" + std::to_string(i), "value_" + std::to_string(i));
        pool.release(std::move(obj));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance should be reasonable
    double avg_time_per_op = static_cast<double>(duration.count()) / num_operations;
    EXPECT_LT(avg_time_per_op, 1000.0); // Less than 1ms per operation
    
    // Check reuse ratio
    double reuse_ratio = static_cast<double>(num_operations - pool.total_created()) / num_operations * 100.0;
    EXPECT_GT(reuse_ratio, 80.0); // Should reuse at least 80% of objects
}

TEST_F(ObjectPoolTest, SamplePoolPerformance) {
    SamplePool pool(1000, 10000);
    const int num_operations = 50000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform many acquire/release operations
    for (int i = 0; i < num_operations; ++i) {
        auto obj = pool.acquire();
        pool.release(std::move(obj));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance should be very fast for simple objects
    double avg_time_per_op = static_cast<double>(duration.count()) / num_operations;
    EXPECT_LT(avg_time_per_op, 100.0); // Less than 100μs per operation
    
    // Check reuse ratio
    double reuse_ratio = static_cast<double>(num_operations - pool.total_created()) / num_operations * 100.0;
    EXPECT_GT(reuse_ratio, 90.0); // Should reuse at least 90% of objects
}

// ============================================================================
// Memory Allocation Reduction Test
// ============================================================================

TEST_F(ObjectPoolTest, MemoryAllocationReduction) {
    // This test verifies that object pooling reduces memory allocations
    // by comparing the number of objects created vs. the number of operations
    
    TimeSeriesPool pool(100, 1000);
    const int num_operations = 10000;
    
    // Perform many operations
    for (int i = 0; i < num_operations; ++i) {
        auto obj = pool.acquire();
        obj->add_sample(1000 + i, 42.0 + i);
        pool.release(std::move(obj));
    }
    
    // Calculate allocation reduction
    size_t total_created = pool.total_created();
    double allocation_reduction = static_cast<double>(num_operations - total_created) / num_operations * 100.0;
    
    // Should achieve at least 30% reduction in allocations
    EXPECT_GT(allocation_reduction, 30.0);
    
    // Log the results for verification
    std::cout << "Memory allocation reduction: " << allocation_reduction << "%" << std::endl;
    std::cout << "Total operations: " << num_operations << std::endl;
    std::cout << "Total objects created: " << total_created << std::endl;
}

} // namespace test
} // namespace storage
} // namespace tsdb 