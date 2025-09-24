#ifndef TSDB_STORAGE_MEMORY_OPTIMIZATION_SIMPLE_CACHE_ALIGNMENT_H
#define TSDB_STORAGE_MEMORY_OPTIMIZATION_SIMPLE_CACHE_ALIGNMENT_H

#include <cstddef>
#include <cstdint>

namespace tsdb {
namespace storage {
namespace memory_optimization {

/**
 * @brief Simple cache alignment utilities for memory optimization
 * 
 * This class provides basic cache alignment functionality without complex dependencies.
 * It focuses on the core concepts of cache line alignment and prefetching.
 */
class SimpleCacheAlignment {
public:
    /**
     * @brief Align a pointer to cache line boundary
     * @param ptr The pointer to align
     * @return Aligned pointer
     */
    static void* align_to_cache_line(void* ptr);
    
    /**
     * @brief Check if a pointer is cache line aligned
     * @param ptr The pointer to check
     * @return True if aligned, false otherwise
     */
    static bool is_cache_aligned(void* ptr);
    
    /**
     * @brief Prefetch data into cache
     * @param ptr Pointer to data to prefetch
     * @param size Size of data to prefetch
     */
    static void prefetch_data(void* ptr, size_t size);
    
    /**
     * @brief Get cache line size for the current system
     * @return Cache line size in bytes
     */
    static size_t get_cache_line_size();
    
    /**
     * @brief Align size to cache line boundary
     * @param size The size to align
     * @return Aligned size
     */
    static size_t align_size_to_cache_line(size_t size);

private:
    static constexpr size_t CACHE_LINE_SIZE = 64;
};

} // namespace memory_optimization
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_MEMORY_OPTIMIZATION_SIMPLE_CACHE_ALIGNMENT_H
