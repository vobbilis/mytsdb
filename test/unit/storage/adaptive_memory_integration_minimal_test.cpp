#include <gtest/gtest.h>
#include <cstdlib>
#include <iostream>

// Minimal test to isolate the issue
TEST(AdaptiveMemoryIntegrationMinimalTest, BasicMallocTest) {
    // Test basic malloc without any custom classes
    void* ptr = std::malloc(256);
    ASSERT_NE(ptr, nullptr);
    
    std::free(ptr);
}

TEST(AdaptiveMemoryIntegrationMinimalTest, BasicAlignedAllocTest) {
    // Test basic aligned_alloc
    void* ptr = std::aligned_alloc(32, 256);
    ASSERT_NE(ptr, nullptr);
    
    // Verify alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    ASSERT_EQ(addr % 32, 0) << "Pointer not aligned to 32 bytes";
    
    std::free(ptr);
}

TEST(AdaptiveMemoryIntegrationMinimalTest, MultipleAllocationsTest) {
    // Test multiple allocations
    std::vector<void*> ptrs;
    
    for (int i = 0; i < 10; ++i) {
        void* ptr = std::aligned_alloc(16, 128);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }
    
    // Free all
    for (void* ptr : ptrs) {
        std::free(ptr);
    }
}
