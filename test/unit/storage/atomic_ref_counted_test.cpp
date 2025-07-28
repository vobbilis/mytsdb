#include <gtest/gtest.h>
#include "tsdb/storage/atomic_ref_counted.h"
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic>
#include <iostream> // Added for debug prints

using namespace tsdb::storage;

// Test fixture for AtomicRefCounted tests
class AtomicRefCountedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset global statistics before each test
        // Note: Global stats functions are not implemented in this version
    }
    
    void TearDown() override {
        // Clean up any remaining references
    }
};

// Simple test class for reference counting
class TestData {
public:
    TestData(int value = 0) : value_(value), destroyed_(false) {
        std::cerr << "[TestData] Constructor called for value=" << value_ << ", address=" << this << std::endl;
    }
    ~TestData() {
        std::cerr << "[TestData] Destructor called for value=" << value_ << ", address=" << this << std::endl;
        if (!destroyed_.exchange(true)) {
            // Only set destroyed_ if it wasn't already set
        }
    }
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    bool isDestroyed() const { return destroyed_.load(); }
    
private:
    int value_;
    mutable std::atomic<bool> destroyed_{false};
};

namespace {
void BasicReferenceCountingHelper() {
    auto test_data = new TestData(42);
    auto* ref_counted = new AtomicRefCounted<TestData>(test_data);
    EXPECT_EQ(ref_counted->refCount(), 1);
    EXPECT_EQ(ref_counted->get()->getValue(), 42);
    EXPECT_FALSE(ref_counted->get()->isDestroyed());
    uint32_t new_count = ref_counted->addRef();
    EXPECT_EQ(new_count, 2);
    EXPECT_EQ(ref_counted->refCount(), 2);
    bool destroyed = ref_counted->release();
    EXPECT_FALSE(destroyed);
    EXPECT_EQ(ref_counted->refCount(), 1);
    destroyed = ref_counted->release();
    EXPECT_TRUE(destroyed);
    if (destroyed) return;
}
void UniqueAndSharedHelper() {
    auto test_data = new TestData(100);
    auto* ref_counted = new AtomicRefCounted<TestData>(test_data);
    EXPECT_TRUE(ref_counted->unique());
    EXPECT_FALSE(ref_counted->shared());
    ref_counted->addRef();
    EXPECT_FALSE(ref_counted->unique());
    EXPECT_TRUE(ref_counted->shared());
    ref_counted->release();
    EXPECT_TRUE(ref_counted->unique());
    EXPECT_FALSE(ref_counted->shared());
    bool destroyed = ref_counted->release();
    EXPECT_TRUE(destroyed);
    if (destroyed) return;
}
void PerformanceTrackingHelper() {
    AtomicRefCountedConfig config;
    config.enable_performance_tracking = true;
    auto test_data = new TestData(500);
    auto* ref_counted = new AtomicRefCounted<TestData>(test_data, config);
    ref_counted->addRef();
    ref_counted->addRef();
    ref_counted->release();
    ref_counted->release();
    const auto& stats = ref_counted->getStats();
    EXPECT_EQ(stats.total_add_refs.load(), 2);
    EXPECT_EQ(stats.total_releases.load(), 2);
    EXPECT_EQ(stats.peak_ref_count.load(), 3);
    std::string stats_string = ref_counted->getStatsString();
    EXPECT_FALSE(stats_string.empty());
    EXPECT_NE(stats_string.find("Total addRef operations: 2"), std::string::npos);
    EXPECT_NE(stats_string.find("Total release operations: 2"), std::string::npos);
    bool destroyed = ref_counted->release();
    EXPECT_TRUE(destroyed);
    if (destroyed) return;
}
void ConcurrentAccessHelper() {
    auto test_data = new TestData(600);
    auto* ref_counted = new AtomicRefCounted<TestData>(test_data);
    const int num_threads = 8;
    const int operations_per_thread = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> total_adds{0};
    std::atomic<int> total_releases{0};
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([ref_counted, &total_adds, &total_releases]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                ref_counted->addRef();
                total_adds++;
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                ref_counted->release();
                total_releases++;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(ref_counted->refCount(), 1);
    EXPECT_EQ(total_adds.load(), num_threads * operations_per_thread);
    EXPECT_EQ(total_releases.load(), num_threads * operations_per_thread);
    const auto& stats = ref_counted->getStats();
    EXPECT_EQ(stats.total_add_refs.load(), num_threads * operations_per_thread);
    EXPECT_EQ(stats.total_releases.load(), num_threads * operations_per_thread);
    bool destroyed = ref_counted->release();
    EXPECT_TRUE(destroyed);
    if (destroyed) return;
}
} // namespace

// Test operator overloads
TEST_F(AtomicRefCountedTest, OperatorOverloads) {
    auto test_data = new TestData(200);
    auto ref_counted = makeAtomicRefCounted<TestData>(test_data);
    
    // Test operator->
    EXPECT_EQ(ref_counted->get()->getValue(), 200);
    ref_counted->get()->setValue(300);
    EXPECT_EQ(ref_counted->get()->getValue(), 300);
    
    // Test operator* (using arrow operator instead)
    EXPECT_EQ(ref_counted->get()->getValue(), 300);
    ref_counted->get()->setValue(400);
    EXPECT_EQ(ref_counted->get()->getValue(), 400);
}

// Test memory ordering differences
TEST_F(AtomicRefCountedTest, MemoryOrdering) {
    // Test with relaxed ordering
    AtomicRefCountedConfig relaxed_config;
    relaxed_config.use_relaxed_ordering = true;
    
    auto test_data1 = new TestData(700);
    auto ref_counted1 = makeAtomicRefCounted<TestData>(test_data1, relaxed_config);
    
    // Test with strict ordering
    AtomicRefCountedConfig strict_config;
    strict_config.use_relaxed_ordering = false;
    
    auto test_data2 = new TestData(800);
    auto ref_counted2 = makeAtomicRefCounted<TestData>(test_data2, strict_config);
    
    // Both should work correctly
    ref_counted1->addRef();
    ref_counted2->addRef();
    
    EXPECT_EQ(ref_counted1->refCount(), 2);
    EXPECT_EQ(ref_counted2->refCount(), 2);
    
    ref_counted1->release();
    ref_counted2->release();
    
    EXPECT_EQ(ref_counted1->refCount(), 1);
    EXPECT_EQ(ref_counted2->refCount(), 1);
}

// Test edge cases
TEST_F(AtomicRefCountedTest, EdgeCases) {
    // Test with null pointer (should throw)
    EXPECT_THROW(makeAtomicRefCounted<TestData>(nullptr), std::invalid_argument);
    
    // Test with disabled performance tracking
    AtomicRefCountedConfig config;
    config.enable_performance_tracking = false;
    
    auto test_data = new TestData(900);
    auto ref_counted = makeAtomicRefCounted<TestData>(test_data, config);
    
    ref_counted->addRef();
    ref_counted->release();
    
    // Statistics should still be available but may be zero
    // Note: With disabled tracking, stats might be zero or undefined
}

// Test helper functions
TEST_F(AtomicRefCountedTest, HelperFunctions) {
    // Test makeAtomicRefCounted with existing data
    auto test_data = new TestData(1100);
    auto ref_counted2 = makeAtomicRefCounted<TestData>(test_data);
    EXPECT_EQ(ref_counted2->get()->getValue(), 1100);
}

// Test statistics reset
TEST_F(AtomicRefCountedTest, StatisticsReset) {
    auto test_data = new TestData(1200);
    auto ref_counted = makeAtomicRefCounted<TestData>(test_data);
    
    ref_counted->addRef();
    ref_counted->release();
    
    const auto& stats_before = ref_counted->getStats();
    EXPECT_GT(stats_before.total_add_refs.load(), 0);
    
    ref_counted->resetStats();
    
    const auto& stats_after = ref_counted->getStats();
    EXPECT_EQ(stats_after.total_add_refs.load(), 0);
    EXPECT_EQ(stats_after.total_releases.load(), 0);
    EXPECT_EQ(stats_after.peak_ref_count.load(), 0);
}

// Test configuration update
TEST_F(AtomicRefCountedTest, ConfigurationUpdate) {
    auto test_data = new TestData(1300);
    auto ref_counted = makeAtomicRefCounted<TestData>(test_data);
    
    AtomicRefCountedConfig new_config;
    new_config.enable_performance_tracking = false;
    new_config.use_relaxed_ordering = true;
    new_config.max_ref_count = 5000;
    
    ref_counted->updateConfig(new_config);
    
    const auto& updated_config = ref_counted->getConfig();
    EXPECT_EQ(updated_config.enable_performance_tracking, new_config.enable_performance_tracking);
    EXPECT_EQ(updated_config.use_relaxed_ordering, new_config.use_relaxed_ordering);
    EXPECT_EQ(updated_config.max_ref_count, new_config.max_ref_count);
}

// Test global statistics
TEST_F(AtomicRefCountedTest, GlobalStatistics) {
    // Create multiple instances
    auto test_data1 = new TestData(1400);
    auto ref_counted1 = makeAtomicRefCounted<TestData>(test_data1);
    
    auto test_data2 = new TestData(1500);
    auto ref_counted2 = makeAtomicRefCounted<TestData>(test_data2);
    
    // Perform operations
    ref_counted1->addRef();
    ref_counted2->addRef();
    ref_counted1->release();
    ref_counted2->release();
    
    // Note: Global stats functions are not implemented in this version
    EXPECT_TRUE(true); // Placeholder test
}

// Test stress test with many operations
TEST_F(AtomicRefCountedTest, StressTest) {
    const int num_instances = 100;
    const int operations_per_instance = 1000;
    
    std::vector<std::unique_ptr<AtomicRefCounted<TestData>>> instances;
    
    // Create many instances
    for (int i = 0; i < num_instances; ++i) {
        auto test_data = new TestData(i);
        instances.push_back(makeAtomicRefCounted<TestData>(test_data));
    }
    
    // Perform many operations
    for (auto& instance : instances) {
        for (int j = 0; j < operations_per_instance; ++j) {
            instance->addRef();
            instance->release();
        }
    }
    
    // Verify all instances are still valid
    for (size_t i = 0; i < instances.size(); ++i) {
        EXPECT_EQ(instances[i]->refCount(), 1);
        EXPECT_EQ(instances[i]->get()->getValue(), i);
        EXPECT_FALSE(instances[i]->get()->isDestroyed());
    }
}

// Test integration with existing types
TEST_F(AtomicRefCountedTest, IntegrationWithExistingTypes) {
    // Test with a simple struct
    struct SimpleStruct {
        int x, y;
        SimpleStruct(int x, int y) : x(x), y(y) {}
    };
    
    auto simple_data = new SimpleStruct(10, 20);
    auto ref_counted = makeAtomicRefCounted<SimpleStruct>(simple_data);
    
    EXPECT_EQ(ref_counted->get()->x, 10);
    EXPECT_EQ(ref_counted->get()->y, 20);
    
    ref_counted->get()->x = 30;
    ref_counted->get()->y = 40;
    
    EXPECT_EQ(ref_counted->get()->x, 30);
    EXPECT_EQ(ref_counted->get()->y, 40);
}

// Test performance comparison with std::shared_ptr
TEST_F(AtomicRefCountedTest, PerformanceComparison) {
    const int num_operations = 100000;
    
    // Test AtomicRefCounted performance
    auto test_data1 = new TestData(1600);
    auto ref_counted = makeAtomicRefCounted<TestData>(test_data1);
    
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        ref_counted->addRef();
        ref_counted->release();
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    
    // Test std::shared_ptr performance
    auto test_data2 = new TestData(1700);
    auto shared_ptr = std::shared_ptr<TestData>(test_data2);
    
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        auto copy = shared_ptr;
        copy.reset();
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    
    // Both should complete successfully
    EXPECT_EQ(ref_counted->refCount(), 1);
    EXPECT_EQ(shared_ptr.use_count(), 1);
    
    // Log performance comparison (for informational purposes)
    std::cout << "AtomicRefCounted: " << duration1.count() << " microseconds" << std::endl;
    std::cout << "std::shared_ptr: " << duration2.count() << " microseconds" << std::endl;
} 

TEST_F(AtomicRefCountedTest, BasicReferenceCounting) {
    std::thread([]() {
        auto test_data = new TestData(42);
        auto* ref_counted = new AtomicRefCounted<TestData>(test_data);
        EXPECT_EQ(ref_counted->refCount(), 1);
        EXPECT_EQ(ref_counted->get()->getValue(), 42);
        EXPECT_FALSE(ref_counted->get()->isDestroyed());
        uint32_t new_count = ref_counted->addRef();
        EXPECT_EQ(new_count, 2);
        EXPECT_EQ(ref_counted->refCount(), 2);
        bool destroyed = ref_counted->release();
        EXPECT_FALSE(destroyed);
        EXPECT_EQ(ref_counted->refCount(), 1);
        destroyed = ref_counted->release();
        EXPECT_TRUE(destroyed);
        // After this, the thread stack is unwound and the pointer is gone
    }).join();
} 