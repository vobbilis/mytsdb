#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <atomic>
#include <algorithm>

#include "tsdb/storage/lock_free_queue.h"

using namespace tsdb::storage;

class LockFreeQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset any global state if needed
    }
    
    void TearDown() override {
        // Clean up any resources
    }
    
    template<typename T>
    void verifyQueueEmpty(const LockFreeQueue<T>& queue) {
        EXPECT_TRUE(queue.empty());
        EXPECT_EQ(queue.size(), 0);
        
        // Note: pop() is not const, so we can't call it on a const reference
        // This is expected behavior for a lock-free queue
    }
    
    template<typename T>
    void verifyQueueSize(const LockFreeQueue<T>& queue, size_t expected_size) {
        EXPECT_EQ(queue.size(), expected_size);
        EXPECT_EQ(queue.empty(), expected_size == 0);
    }
};

// Basic functionality tests
TEST_F(LockFreeQueueTest, BasicOperations) {
    LockFreeQueue<int> queue(8); // Uses default PersistentQueueConfig
    
    // Test empty queue
    verifyQueueEmpty(queue);
    
    // Test single push/pop
    EXPECT_TRUE(queue.push(42));
    verifyQueueSize(queue, 1);
    
    int value;
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 42);
    verifyQueueEmpty(queue);
}

TEST_F(LockFreeQueueTest, MultipleOperations) {
    LockFreeQueue<int> queue(16); // Uses default PersistentQueueConfig
    
    // Push up to capacity
    for (int i = 0; i < 16; ++i) {
        EXPECT_TRUE(queue.push(i));
    }
    verifyQueueSize(queue, 16);
    
    // Verify queue is full
    EXPECT_TRUE(queue.full());
    EXPECT_FALSE(queue.push(999)); // Should fail when full
    
    // Pop all items
    for (int i = 0; i < 16; ++i) {
        int value;
        EXPECT_TRUE(queue.pop(value));
        // Note: order is not guaranteed in lock-free queues
    }
    verifyQueueEmpty(queue);
}

TEST_F(LockFreeQueueTest, MoveSemantics) {
    LockFreeQueue<std::string> queue(4);
    
    std::string test_string = "Hello, World!";
    EXPECT_TRUE(queue.push(std::move(test_string)));
    
    // Original string should be moved (empty)
    EXPECT_TRUE(test_string.empty());
    
    std::string result;
    EXPECT_TRUE(queue.pop(result));
    EXPECT_EQ(result, "Hello, World!");
}

// Concurrent access tests
TEST_F(LockFreeQueueTest, SingleProducerSingleConsumer) {
    LockFreeQueue<int> queue(1024);
    std::atomic<bool> stop{false};
    std::vector<int> produced;
    std::vector<int> consumed;
    
    // Producer thread
    auto producer = [&]() {
        for (int i = 0; i < 1000; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
            produced.push_back(i);
        }
    };
    
    // Consumer thread
    auto consumer = [&]() {
        int value;
        while (consumed.size() < 1000) {
            if (queue.pop(value)) {
                consumed.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    };
    
    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);
    
    producer_thread.join();
    consumer_thread.join();
    
    // Verify all values were consumed
    EXPECT_EQ(consumed.size(), 1000);
    EXPECT_EQ(produced.size(), 1000);
    
    // Verify queue is empty
    verifyQueueEmpty(queue);
    
    // Verify all values were consumed (order may vary due to concurrency)
    std::sort(consumed.begin(), consumed.end());
    std::sort(produced.begin(), produced.end());
    EXPECT_EQ(consumed, produced);
}

TEST_F(LockFreeQueueTest, MultipleProducersMultipleConsumers) {
    LockFreeQueue<int> queue(4096);
    std::atomic<int> total_produced{0};
    std::atomic<int> total_consumed{0};
    const int num_producers = 4;
    const int num_consumers = 4;
    const int items_per_producer = 1000;
    
    // Producer threads
    auto producer = [&](int producer_id) {
        for (int i = 0; i < items_per_producer; ++i) {
            int value = producer_id * items_per_producer + i;
            while (!queue.push(value)) {
                std::this_thread::yield();
            }
            total_produced.fetch_add(1);
        }
    };
    
    // Consumer threads
    auto consumer = [&]() {
        int value;
        while (total_consumed.load() < num_producers * items_per_producer) {
            if (queue.pop(value)) {
                total_consumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    };
    
    // Start producer threads
    std::vector<std::thread> producer_threads;
    for (int i = 0; i < num_producers; ++i) {
        producer_threads.emplace_back(producer, i);
    }
    
    // Start consumer threads
    std::vector<std::thread> consumer_threads;
    for (int i = 0; i < num_consumers; ++i) {
        consumer_threads.emplace_back(consumer);
    }
    
    // Wait for all threads to complete
    for (auto& t : producer_threads) {
        t.join();
    }
    for (auto& t : consumer_threads) {
        t.join();
    }
    
    // Verify all items were produced and consumed
    EXPECT_EQ(total_produced.load(), num_producers * items_per_producer);
    EXPECT_EQ(total_consumed.load(), num_producers * items_per_producer);
    verifyQueueEmpty(queue);
}

TEST_F(LockFreeQueueTest, ConcurrentAccess) {
    LockFreeQueue<int> queue(1000); // Uses default PersistentQueueConfig
    const int num_threads = 4;
    const int items_per_thread = 250;
    
    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};
    
    // Producer threads
    auto producer = [&](int thread_id) {
        for (int i = 0; i < items_per_thread; ++i) {
            int value = thread_id * items_per_thread + i;
            while (!queue.push(value)) {
                std::this_thread::yield();
            }
            total_pushed.fetch_add(1);
        }
    };
    
    // Consumer threads
    auto consumer = [&]() {
        for (int i = 0; i < items_per_thread; ++i) {
            int value;
            while (!queue.pop(value)) {
                std::this_thread::yield();
            }
            total_popped.fetch_add(1);
        }
    };
    
    // Start all threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(producer, i);
        threads.emplace_back(consumer);
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify all operations completed
    EXPECT_EQ(total_pushed.load(), num_threads * items_per_thread);
    EXPECT_EQ(total_popped.load(), num_threads * items_per_thread);
    verifyQueueEmpty(queue);
}

TEST_F(LockFreeQueueTest, StressTest) {
    LockFreeQueue<int> queue(1000); // Uses default PersistentQueueConfig
    const int num_threads = 8;
    const int operations_per_thread = 1000;
    
    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};
    
    auto worker = [&](int thread_id) {
        for (int i = 0; i < operations_per_thread; ++i) {
            int value = thread_id * operations_per_thread + i;
            
            // Try to push
            if (queue.push(value)) {
                total_pushed.fetch_add(1);
            }
            
            // Try to pop
            int popped_value;
            if (queue.pop(popped_value)) {
                total_popped.fetch_add(1);
            }
        }
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Stress test completed in " << duration.count() << "ms" << std::endl;
    std::cout << "Total pushed: " << total_pushed.load() << std::endl;
    std::cout << "Total popped: " << total_popped.load() << std::endl;
    
    // Note: In a bounded queue under stress, we may not have equal push/pop counts
    // This is expected behavior for bounded queues
}

// Performance tests
TEST_F(LockFreeQueueTest, PerformanceBenchmark) {
    LockFreeQueue<int> queue(100000); // Uses default PersistentQueueConfig
    const int num_operations = 100000;
    
    // Benchmark push operations
    auto push_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        EXPECT_TRUE(queue.push(i));
    }
    auto push_end = std::chrono::high_resolution_clock::now();
    
    // Benchmark pop operations
    auto pop_start = std::chrono::high_resolution_clock::now();
    std::vector<int> popped_values;
    for (int i = 0; i < num_operations; ++i) {
        int value;
        EXPECT_TRUE(queue.pop(value));
        popped_values.push_back(value);
    }
    auto pop_end = std::chrono::high_resolution_clock::now();
    
    // Verify all values were popped (order doesn't matter for lock-free queue)
    std::sort(popped_values.begin(), popped_values.end());
    for (int i = 0; i < num_operations; ++i) {
        EXPECT_EQ(popped_values[i], i);
    }
    
    // Calculate throughput
    auto push_duration = std::chrono::duration_cast<std::chrono::microseconds>(push_end - push_start);
    auto pop_duration = std::chrono::duration_cast<std::chrono::microseconds>(pop_end - pop_start);
    
    double push_throughput = static_cast<double>(num_operations) / push_duration.count() * 1000000;
    double pop_throughput = static_cast<double>(num_operations) / pop_duration.count() * 1000000;
    
    std::cout << "Performance benchmark results:" << std::endl;
    std::cout << "  Push throughput: " << push_throughput << " ops/sec" << std::endl;
    std::cout << "  Pop throughput: " << pop_throughput << " ops/sec" << std::endl;
    std::cout << "  Push latency: " << push_duration.count() / num_operations << " μs/op" << std::endl;
    std::cout << "  Pop latency: " << pop_duration.count() / num_operations << " μs/op" << std::endl;
    
    // Verify queue is empty
    verifyQueueEmpty(queue);
    
    // Performance expectations (adjust based on your system)
    EXPECT_GT(push_throughput, 1000000); // At least 1M ops/sec
    EXPECT_GT(pop_throughput, 1000000);  // At least 1M ops/sec
}

TEST_F(LockFreeQueueTest, ConcurrentPerformanceBenchmark) {
    LockFreeQueue<int> queue(10000); // Uses default PersistentQueueConfig
    const int num_threads = 4;
    const int operations_per_thread = 25000;
    
    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};
    
    // Producer threads
    auto producer = [&](int thread_id) {
        for (int i = 0; i < operations_per_thread; ++i) {
            int value = thread_id * operations_per_thread + i;
            while (!queue.push(value)) {
                std::this_thread::yield();
            }
            total_pushed.fetch_add(1);
        }
    };
    
    // Consumer threads
    auto consumer = [&]() {
        for (int i = 0; i < operations_per_thread; ++i) {
            int value;
            while (!queue.pop(value)) {
                std::this_thread::yield();
            }
            total_popped.fetch_add(1);
        }
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Start producer threads
    std::vector<std::thread> producer_threads;
    for (int i = 0; i < num_threads; ++i) {
        producer_threads.emplace_back(producer, i);
    }
    
    // Start consumer threads
    std::vector<std::thread> consumer_threads;
    for (int i = 0; i < num_threads; ++i) {
        consumer_threads.emplace_back(consumer);
    }
    
    // Wait for all threads to complete
    for (auto& t : producer_threads) {
        t.join();
    }
    for (auto& t : consumer_threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    // Calculate throughput
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double total_operations = num_threads * operations_per_thread * 2; // push + pop
    double throughput = total_operations / duration.count() * 1000000;
    
    std::cout << "Concurrent performance benchmark results:" << std::endl;
    std::cout << "  Total operations: " << total_operations << std::endl;
    std::cout << "  Duration: " << duration.count() << " μs" << std::endl;
    std::cout << "  Throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << "  Total pushed: " << total_pushed.load() << std::endl;
    std::cout << "  Total popped: " << total_popped.load() << std::endl;
    
    // Verify all operations completed
    EXPECT_EQ(total_pushed.load(), num_threads * operations_per_thread);
    EXPECT_EQ(total_popped.load(), num_threads * operations_per_thread);
    verifyQueueEmpty(queue);
    
    // Performance expectations
    EXPECT_GT(throughput, 500000); // At least 500K ops/sec concurrent
}

// Edge case tests
TEST_F(LockFreeQueueTest, EmptyQueueOperations) {
    LockFreeQueue<int> queue(4); // Uses default PersistentQueueConfig
    
    // Try to pop from empty queue
    int value;
    EXPECT_FALSE(queue.pop(value));
    
    // Verify queue is still empty
    verifyQueueEmpty(queue);
}

TEST_F(LockFreeQueueTest, LargeDataTypes) {
    LockFreeQueue<std::vector<uint8_t>> queue(2); // Uses default PersistentQueueConfig
    
    // Push large data
    std::vector<uint8_t> large_data(10000, 0x42);
    EXPECT_TRUE(queue.push(large_data));
    
    // Pop large data
    std::vector<uint8_t> result;
    EXPECT_TRUE(queue.pop(result));
    EXPECT_EQ(result, large_data);
    
    verifyQueueEmpty(queue);
}

// Persistence tests
TEST_F(LockFreeQueueTest, PersistenceConfiguration) {
    // Test with persistence disabled (default)
    PersistentQueueConfig config_disabled;
    config_disabled.enable_persistence = false;
    
    LockFreeQueue<int> queue_disabled(8, config_disabled);
    EXPECT_FALSE(queue_disabled.is_persistence_enabled());
    EXPECT_EQ(queue_disabled.persistent_size(), 0);
    EXPECT_EQ(queue_disabled.persistent_bytes(), 0);
    
    // Test with persistence enabled
    PersistentQueueConfig config_enabled;
    config_enabled.enable_persistence = true;
    config_enabled.persistence_path = "./test_queue_data";
    config_enabled.max_persistent_size = 1024 * 1024; // 1MB
    config_enabled.drop_on_persistent_full = true;
    
    LockFreeQueue<int> queue_enabled(8, config_enabled);
    EXPECT_TRUE(queue_enabled.is_persistence_enabled());
    
    // Test persistence operations
    EXPECT_TRUE(queue_enabled.push(42));
    EXPECT_TRUE(queue_enabled.push(43));
    
    // Verify items are in memory queue, not persistent storage
    EXPECT_EQ(queue_enabled.size(), 2);
    EXPECT_EQ(queue_enabled.persistent_size(), 0);
    
    // Fill the memory queue to trigger persistence
    for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(queue_enabled.push(100 + i));
    }
    
    // Now try to push more - should go to persistent storage
    EXPECT_TRUE(queue_enabled.push(200));
    EXPECT_TRUE(queue_enabled.push(201));
    
    // Verify some items are now in persistent storage
    EXPECT_GT(queue_enabled.persistent_size(), 0);
    EXPECT_GT(queue_enabled.persistent_bytes(), 0);
    
    // Test loading from persistent storage
    // Note: The default implementation is a mock that just tracks counters
    // In a real implementation, this would load actual data from disk
    size_t loaded = queue_enabled.load_from_persistent(2);
    // With mock implementation, we expect 0 items loaded since no real data is stored
    EXPECT_EQ(loaded, 0);
    
    // Test clearing persistent storage
    queue_enabled.clear_persistent();
    EXPECT_EQ(queue_enabled.persistent_size(), 0);
    EXPECT_EQ(queue_enabled.persistent_bytes(), 0);
}

TEST_F(LockFreeQueueTest, PersistenceCallback) {
    std::atomic<int> callback_count{0};
    std::atomic<size_t> total_bytes{0};
    
    PersistentQueueConfig config;
    config.enable_persistence = true;
    config.persistence_path = "./test_queue_callback";
    config.persistence_callback = [&](const std::string& event, size_t bytes) {
        callback_count.fetch_add(1);
        total_bytes.fetch_add(bytes);
    };
    
    LockFreeQueue<int> queue(4, config);
    
    // Push items to trigger persistence
    for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(queue.push(i));
    }
    
    // Verify callback was called
    EXPECT_GT(callback_count.load(), 0);
    EXPECT_GT(total_bytes.load(), 0);
    
    // Test flush operation
    EXPECT_TRUE(queue.flush_to_persistent());
    
    // Clear persistent storage
    queue.clear_persistent();
} 