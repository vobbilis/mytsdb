#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <memory>

namespace tsdb {
namespace integration {

class ObjectPoolEducationTest : public ::testing::Test {
protected:
    void SetUp() override {
        core::StorageConfig config;
        config.data_dir = "./test/data/storageimpl_phases/education";
        config.object_pool_config.time_series_initial_size = 10;
        config.object_pool_config.time_series_max_size = 100;
        config.object_pool_config.labels_initial_size = 20;
        config.object_pool_config.labels_max_size = 200;
        config.object_pool_config.samples_initial_size = 50;
        config.object_pool_config.samples_max_size = 500;
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
    }
    
    std::unique_ptr<storage::StorageImpl> storage_;
};

TEST_F(ObjectPoolEducationTest, DemonstrateObjectPoolReuse) {
    std::cout << "\n=== OBJECT POOL REUSE DEMONSTRATION ===" << std::endl;
    
    // Step 1: Show initial pool state
    std::cout << "\n1. INITIAL POOL STATE:" << std::endl;
    std::string initial_stats = storage_->stats();
    std::cout << initial_stats << std::endl;
    
    // Step 2: Perform multiple write operations
    std::cout << "\n2. PERFORMING 5 WRITE OPERATIONS:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        core::Labels labels;
        labels.add("__name__", "test_metric_" + std::to_string(i));
        labels.add("test", "education");
        
        core::TimeSeries series(labels);
        for (int j = 0; j < 10; ++j) {
            series.add_sample(1000 + j, 100.0 + j + i);
        }
        
        auto write_result = storage_->write(series);
        ASSERT_TRUE(write_result.ok()) << "Write failed for series " << i;
        
        std::cout << "  Written series " << i << std::endl;
    }
    
    // Step 3: Show pool state after writes
    std::cout << "\n3. POOL STATE AFTER 5 WRITES:" << std::endl;
    std::string after_writes_stats = storage_->stats();
    std::cout << after_writes_stats << std::endl;
    
    // Step 4: Perform multiple read operations
    std::cout << "\n4. PERFORMING 5 READ OPERATIONS:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        core::Labels query_labels;
        query_labels.add("__name__", "test_metric_" + std::to_string(i));
        query_labels.add("test", "education");
        
        auto read_result = storage_->read(query_labels, 1000, 1010);
        if (!read_result.ok()) {
            std::cout << "Read failed for series " << i << ": " << read_result.error() << std::endl;
        }
        ASSERT_TRUE(read_result.ok()) << "Read failed for series " << i << ": " << (read_result.ok() ? "" : read_result.error());
        
        std::cout << "  Read series " << i << std::endl;
    }
    
    // Step 5: Show final pool state
    std::cout << "\n5. FINAL POOL STATE AFTER 5 READS:" << std::endl;
    std::string final_stats = storage_->stats();
    std::cout << final_stats << std::endl;
    
    // Step 6: Explain what happened
    std::cout << "\n=== EXPLANATION ===" << std::endl;
    std::cout << "Notice the key metrics:" << std::endl;
    std::cout << "- Total created: How many objects were allocated from system memory" << std::endl;
    std::cout << "- Total acquired: How many times objects were requested from the pool" << std::endl;
    std::cout << "- Total released: How many times objects were returned to the pool" << std::endl;
    std::cout << "- Reuse ratio: Percentage of requests that reused existing objects" << std::endl;
    std::cout << std::endl;
    std::cout << "Efficiency = (Total acquired - Total created) / Total acquired * 100%" << std::endl;
    std::cout << "This shows how much memory allocation we avoided!" << std::endl;
}

TEST_F(ObjectPoolEducationTest, CompareWithDirectAllocation) {
    std::cout << "\n=== COMPARISON: OBJECT POOL vs DIRECT ALLOCATION ===" << std::endl;
    
    // Method 1: Using object pools (our current implementation)
    std::cout << "\nMETHOD 1: USING OBJECT POOLS" << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        core::Labels labels;
        labels.add("__name__", "pool_test_" + std::to_string(i));
        labels.add("method", "pool");
        
        core::TimeSeries series(labels);
        for (int j = 0; j < 50; ++j) {
            series.add_sample(1000 + j, 100.0 + j + i);
        }
        
        auto write_result = storage_->write(series);
        ASSERT_TRUE(write_result.ok());
        
        // Read it back
        auto read_result = storage_->read(labels, 1000, 1050);
        ASSERT_TRUE(read_result.ok());
    }
    
    auto pool_end_time = std::chrono::high_resolution_clock::now();
    auto pool_duration = std::chrono::duration_cast<std::chrono::microseconds>(pool_end_time - start_time);
    
    std::cout << "Pool method completed in: " << pool_duration.count() << " microseconds" << std::endl;
    
    // Show pool statistics
    std::string pool_stats = storage_->stats();
    std::cout << "Pool statistics:" << std::endl;
    std::cout << pool_stats << std::endl;
    
    // Method 2: Simulating direct allocation (what would happen without pools)
    std::cout << "\nMETHOD 2: SIMULATING DIRECT ALLOCATION" << std::endl;
    start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::unique_ptr<core::TimeSeries>> direct_series;
    std::vector<std::unique_ptr<core::Labels>> direct_labels;
    
    for (int i = 0; i < 100; ++i) {
        // Simulate direct allocation
        auto labels = std::make_unique<core::Labels>();
        labels->add("__name__", "direct_test_" + std::to_string(i));
        labels->add("method", "direct");
        
        auto series = std::make_unique<core::TimeSeries>(*labels);
        for (int j = 0; j < 50; ++j) {
            series->add_sample(1000 + j, 100.0 + j + i);
        }
        
        direct_series.push_back(std::move(series));
        direct_labels.push_back(std::move(labels));
    }
    
    auto direct_end_time = std::chrono::high_resolution_clock::now();
    auto direct_duration = std::chrono::duration_cast<std::chrono::microseconds>(direct_end_time - start_time);
    
    std::cout << "Direct allocation completed in: " << direct_duration.count() << " microseconds" << std::endl;
    std::cout << "Objects created: " << direct_series.size() << " TimeSeries + " << direct_labels.size() << " Labels" << std::endl;
    
    // Comparison
    std::cout << "\n=== COMPARISON RESULTS ===" << std::endl;
    std::cout << "Object Pool Method:" << std::endl;
    std::cout << "  - Time: " << pool_duration.count() << " microseconds" << std::endl;
    std::cout << "  - Memory allocations: Minimal (reused objects)" << std::endl;
    std::cout << "  - Memory deallocations: Minimal (objects returned to pool)" << std::endl;
    std::cout << std::endl;
    std::cout << "Direct Allocation Method:" << std::endl;
    std::cout << "  - Time: " << direct_duration.count() << " microseconds" << std::endl;
    std::cout << "  - Memory allocations: " << (direct_series.size() + direct_labels.size()) << " objects" << std::endl;
    std::cout << "  - Memory deallocations: " << (direct_series.size() + direct_labels.size()) << " objects" << std::endl;
    std::cout << std::endl;
    std::cout << "Efficiency Gain: " << std::endl;
    std::cout << "  - Fewer system calls to malloc/free" << std::endl;
    std::cout << "  - Better cache locality (objects stay in memory)" << std::endl;
    std::cout << "  - Reduced memory fragmentation" << std::endl;
    std::cout << "  - Predictable memory usage patterns" << std::endl;
}

TEST_F(ObjectPoolEducationTest, MemoryFragmentationDemonstration) {
    std::cout << "\n=== MEMORY FRAGMENTATION DEMONSTRATION ===" << std::endl;
    
    std::cout << "Memory fragmentation occurs when:" << std::endl;
    std::cout << "1. Objects are allocated and deallocated frequently" << std::endl;
    std::cout << "2. Different sized objects create 'holes' in memory" << std::endl;
    std::cout << "3. System memory becomes fragmented over time" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Object pools prevent fragmentation by:" << std::endl;
    std::cout << "1. Pre-allocating objects of the same size" << std::endl;
    std::cout << "2. Reusing the same memory locations" << std::endl;
    std::cout << "3. Keeping objects in contiguous memory blocks" << std::endl;
    std::cout << std::endl;
    
    // Demonstrate with our pool
    std::cout << "Our TimeSeriesPool:" << std::endl;
    std::string stats = storage_->stats();
    std::cout << stats << std::endl;
    
    std::cout << "Notice:" << std::endl;
    std::cout << "- 'Available objects': Pre-allocated pool size" << std::endl;
    std::cout << "- 'Total created': Only created once, then reused" << std::endl;
    std::cout << "- 'Total acquired/released': Objects cycling through the pool" << std::endl;
}

} // namespace integration
} // namespace tsdb 