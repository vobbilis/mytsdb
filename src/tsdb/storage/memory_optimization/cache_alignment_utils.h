#ifndef TSDB_STORAGE_CACHE_ALIGNMENT_UTILS_H_
#define TSDB_STORAGE_CACHE_ALIGNMENT_UTILS_H_

#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/config.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>

namespace tsdb {
namespace storage {

/**
 * @brief Cache Alignment Utilities for memory optimization
 * 
 * This class provides utilities for cache-aligned memory allocation
 * and optimization to improve cache performance.
 */
class CacheAlignmentUtils {
public:
    /**
     * @brief Constructor
     * @param config Storage configuration
     */
    explicit CacheAlignmentUtils(const core::StorageConfig& config);
    
    /**
     * @brief Destructor
     */
    ~CacheAlignmentUtils();
    
    /**
     * @brief Initialize the cache alignment utilities
     * @return Result indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Align pointer to cache line boundary
     * @param ptr Pointer to align
     * @param alignment Cache line alignment (default: 64 bytes)
     * @return Aligned pointer
     */
    void* align_to_cache_line(void* ptr, size_t alignment = 64);
    
    /**
     * @brief Allocate cache-aligned memory
     * @param size Size of memory to allocate
     * @param alignment Cache line alignment
     * @return Result containing aligned pointer or error
     */
    core::Result<void*> allocate_aligned(size_t size, size_t alignment = 64);
    
    /**
     * @brief Deallocate cache-aligned memory
     * @param ptr Pointer to deallocate
     * @return Result indicating success or failure
     */
    core::Result<void> deallocate_aligned(void* ptr);
    
    /**
     * @brief Optimize data layout for cache performance
     * @param data Vector of data to optimize
     * @return Result indicating success or failure
     */
    core::Result<void> optimize_data_layout(std::vector<void*>& data);
    
    /**
     * @brief Prefetch data into cache
     * @param ptr Pointer to prefetch
     * @param size Size of data to prefetch
     * @return Result indicating success or failure
     */
    core::Result<void> prefetch_data(void* ptr, size_t size);
    
    /**
     * @brief Promote hot data to faster cache
     * @param ptr Pointer to promote
     * @return Result indicating success or failure
     */
    core::Result<void> promote_hot_data(void* ptr);
    
    /**
     * @brief Demote cold data to slower cache
     * @param ptr Pointer to demote
     * @return Result indicating success or failure
     */
    core::Result<void> demote_cold_data(void* ptr);
    
    /**
     * @brief Get cache alignment statistics
     * @return String containing cache alignment statistics
     */
    std::string get_cache_stats() const;
    
    /**
     * @brief Get memory usage statistics
     * @return String containing memory usage statistics
     */
    std::string get_memory_stats() const;
    
    /**
     * @brief Get prefetch statistics
     * @return String containing prefetch statistics
     */
    std::string get_prefetch_stats() const;

private:
    // Configuration
    core::StorageConfig config_;
    
    // Cache alignment tracking
    struct CacheAlignmentInfo {
        std::atomic<size_t> aligned_allocations;
        std::atomic<size_t> alignment_operations;
        std::atomic<size_t> prefetch_operations;
        std::atomic<size_t> promotion_operations;
        std::atomic<size_t> demotion_operations;
    };
    
    CacheAlignmentInfo cache_info_;
    
    // Memory tracking
    struct MemoryInfo {
        void* ptr;
        size_t size;
        size_t alignment;
        std::chrono::system_clock::time_point allocated_at;
        std::atomic<size_t> access_count;
        std::atomic<bool> is_hot;
        
        // Default constructor
        MemoryInfo() : ptr(nullptr), size(0), alignment(0), access_count(0), is_hot(false) {}
        
        // Copy constructor
        MemoryInfo(const MemoryInfo& other) 
            : ptr(other.ptr), size(other.size), alignment(other.alignment), 
              allocated_at(other.allocated_at), access_count(other.access_count.load()), 
              is_hot(other.is_hot.load()) {}
        
        // Move constructor
        MemoryInfo(MemoryInfo&& other) noexcept
            : ptr(other.ptr), size(other.size), alignment(other.alignment), 
              allocated_at(other.allocated_at), access_count(other.access_count.load()), 
              is_hot(other.is_hot.load()) {}
        
        // Copy assignment operator
        MemoryInfo& operator=(const MemoryInfo& other) {
            if (this != &other) {
                ptr = other.ptr;
                size = other.size;
                alignment = other.alignment;
                allocated_at = other.allocated_at;
                access_count.store(other.access_count.load());
                is_hot.store(other.is_hot.load());
            }
            return *this;
        }
        
        // Move assignment operator
        MemoryInfo& operator=(MemoryInfo&& other) noexcept {
            if (this != &other) {
                ptr = other.ptr;
                size = other.size;
                alignment = other.alignment;
                allocated_at = other.allocated_at;
                access_count.store(other.access_count.load());
                is_hot.store(other.is_hot.load());
            }
            return *this;
        }
    };
    
    std::unordered_map<void*, MemoryInfo> memory_tracking_;
    mutable std::mutex memory_mutex_;
    
    // Prefetch tracking
    std::atomic<size_t> total_prefetches_;
    std::atomic<size_t> successful_prefetches_;
    std::atomic<size_t> failed_prefetches_;
    
    /**
     * @brief Track memory allocation
     * @param ptr Pointer to track
     * @param size Size of allocation
     * @param alignment Alignment of allocation
     */
    void track_memory_allocation(void* ptr, size_t size, size_t alignment);
    
    /**
     * @brief Untrack memory allocation
     * @param ptr Pointer to untrack
     */
    void untrack_memory_allocation(void* ptr);
    
    /**
     * @brief Update access statistics
     * @param ptr Pointer that was accessed
     */
    void update_access_statistics(void* ptr);
    
    /**
     * @brief Identify hot data based on access patterns
     * @return Vector of pointers to hot data
     */
    std::vector<void*> identify_hot_data();
    
    /**
     * @brief Identify cold data based on access patterns
     * @return Vector of pointers to cold data
     */
    std::vector<void*> identify_cold_data();
    
    /**
     * @brief Calculate cache line alignment
     * @param ptr Pointer to align
     * @param alignment Cache line alignment
     * @return Aligned pointer
     */
    void* calculate_aligned_pointer(void* ptr, size_t alignment);
    
    /**
     * @brief Check if pointer is cache-aligned
     * @param ptr Pointer to check
     * @param alignment Cache line alignment
     * @return True if pointer is aligned
     */
    bool is_cache_aligned(void* ptr, size_t alignment = 64);
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_CACHE_ALIGNMENT_UTILS_H_
