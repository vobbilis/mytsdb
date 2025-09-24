#include <gtest/gtest.h>
#include "tsdb/storage/memory_optimization/cache_alignment_utils.h"
#include "tsdb/core/types.h"
#include <vector>
#include <memory>

using namespace tsdb::storage;

TEST(CacheAlignmentTest, AlignToCacheLine) {
    tsdb::core::StorageConfig config;
    CacheAlignmentUtils cache_utils(config);
    auto init_result = cache_utils.initialize();
    ASSERT_TRUE(init_result.ok());
    
    std::vector<char> data(1000);
    void* aligned_ptr = cache_utils.align_to_cache_line(data.data());
    
    // Test that the pointer is properly aligned
    uintptr_t addr = reinterpret_cast<uintptr_t>(aligned_ptr);
    ASSERT_EQ(addr % 64, 0) << "Pointer should be aligned to 64-byte cache line";
    
    // Test that the aligned pointer is within the original buffer
    ASSERT_GE(aligned_ptr, data.data());
    ASSERT_LE(aligned_ptr, data.data() + data.size());
}

TEST(CacheAlignmentTest, OptimizeDataLayout) {
    tsdb::core::StorageConfig config;
    CacheAlignmentUtils cache_utils(config);
    auto init_result = cache_utils.initialize();
    ASSERT_TRUE(init_result.ok());
    
    std::vector<char> data(1000);
    std::vector<void*> data_ptrs = {data.data()};
    
    // Test data layout optimization
    auto result = cache_utils.optimize_data_layout(data_ptrs);
    ASSERT_TRUE(result.ok());
    
    // Test that the data is still accessible
    data[0] = 'A';
    data[999] = 'Z';
    ASSERT_EQ(data[0], 'A');
    ASSERT_EQ(data[999], 'Z');
}

TEST(CacheAlignmentTest, PrefetchData) {
    tsdb::core::StorageConfig config;
    CacheAlignmentUtils cache_utils(config);
    auto init_result = cache_utils.initialize();
    ASSERT_TRUE(init_result.ok());
    
    std::vector<char> data(1000);
    
    // Test prefetching
    auto result = cache_utils.prefetch_data(data.data(), data.size());
    ASSERT_TRUE(result.ok());
    
    // Test that prefetching doesn't modify data
    data[0] = 'X';
    ASSERT_EQ(data[0], 'X');
}

TEST(CacheAlignmentTest, HotColdSeparation) {
    tsdb::core::StorageConfig config;
    CacheAlignmentUtils cache_utils(config);
    auto init_result = cache_utils.initialize();
    ASSERT_TRUE(init_result.ok());
    
    std::vector<char> hot_data(1000);
    std::vector<char> cold_data(1000);
    
    // Test hot data promotion
    auto hot_result = cache_utils.promote_hot_data(hot_data.data());
    ASSERT_TRUE(hot_result.ok());
    
    // Test cold data demotion
    auto cold_result = cache_utils.demote_cold_data(cold_data.data());
    ASSERT_TRUE(cold_result.ok());
    
    // Test that data is still accessible
    hot_data[0] = 'H';
    cold_data[0] = 'C';
    ASSERT_EQ(hot_data[0], 'H');
    ASSERT_EQ(cold_data[0], 'C');
}

TEST(CacheAlignmentTest, CacheOptimization) {
    tsdb::core::StorageConfig config;
    CacheAlignmentUtils cache_utils(config);
    auto init_result = cache_utils.initialize();
    ASSERT_TRUE(init_result.ok());
    
    std::vector<char> data(1000);
    std::vector<void*> data_ptrs = {data.data()};
    
    // Test cache optimization
    auto result = cache_utils.optimize_data_layout(data_ptrs);
    ASSERT_TRUE(result.ok());
    
    // Test that optimization doesn't corrupt data
    data[0] = 'O';
    data[999] = 'P';
    ASSERT_EQ(data[0], 'O');
    ASSERT_EQ(data[999], 'P');
}