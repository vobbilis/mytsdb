#ifndef TSDB_STORAGE_ENHANCED_LABELS_POOL_H_
#define TSDB_STORAGE_ENHANCED_LABELS_POOL_H_

#include "tsdb/storage/object_pool.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstddef>
#include <cstring>

namespace tsdb {
namespace storage {

/**
 * @brief Enhanced LabelsPool with cache alignment and bulk operations
 * 
 * This enhanced version of LabelsPool provides:
 * - Cache-aligned memory blocks for optimal cache performance
 * - Bulk allocation/deallocation for better throughput
 * - Cache optimization methods for hot data
 * - Prefetching capabilities for hot objects
 */
class EnhancedLabelsPool : public LabelsPool {
public:
    /**
     * @brief Constructor
     * @param initial_size Initial number of objects to pre-allocate
     * @param max_size Maximum number of objects in the pool
     */
    explicit EnhancedLabelsPool(size_t initial_size = 200, size_t max_size = 20000);
    
    /**
     * @brief Destructor
     */
    ~EnhancedLabelsPool();
    
    /**
     * @brief Acquire a cache-aligned Labels object from the pool
     * @return Unique pointer to a cache-aligned Labels object
     */
    std::unique_ptr<core::Labels> acquire_aligned();
    
    /**
     * @brief Acquire multiple Labels objects in bulk
     * @param count Number of objects to acquire
     * @return Vector of unique pointers to Labels objects
     */
    std::vector<std::unique_ptr<core::Labels>> acquire_bulk(size_t count);
    
    /**
     * @brief Release multiple Labels objects in bulk
     * @param objects Vector of unique pointers to objects to release
     */
    void release_bulk(std::vector<std::unique_ptr<core::Labels>>& objects);
    
    /**
     * @brief Optimize cache layout for better performance
     */
    void optimize_cache_layout();
    
    /**
     * @brief Prefetch hot objects into cache
     */
    void prefetch_hot_objects();
    
    /**
     * @brief Get cache alignment statistics
     * @return String containing cache alignment statistics
     */
    std::string cache_stats() const;
    
    /**
     * @brief Get the number of cache-aligned blocks
     * @return Number of cache-aligned blocks
     */
    size_t cache_aligned_blocks() const;
    
    /**
     * @brief Get cache hit ratio
     * @return Cache hit ratio as a percentage
     */
    double cache_hit_ratio() const;

private:
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t CACHE_ALIGNED_BLOCK_SIZE = 512; // 512B cache-aligned blocks for labels
    
    /**
     * @brief Cache-aligned memory block
     */
    struct alignas(CACHE_LINE_SIZE) CacheAlignedBlock {
        char data[CACHE_ALIGNED_BLOCK_SIZE];
        std::atomic<bool> in_use;
        std::atomic<size_t> access_count;
        std::atomic<uint64_t> last_access_time;
        
        CacheAlignedBlock() : in_use(false), access_count(0), last_access_time(0) {}
        
        // Copy constructor
        CacheAlignedBlock(const CacheAlignedBlock& other) 
            : in_use(other.in_use.load()), access_count(other.access_count.load()), 
              last_access_time(other.last_access_time.load()) {
            std::memcpy(data, other.data, CACHE_ALIGNED_BLOCK_SIZE);
        }
        
        // Move constructor
        CacheAlignedBlock(CacheAlignedBlock&& other) noexcept
            : in_use(other.in_use.load()), access_count(other.access_count.load()), 
              last_access_time(other.last_access_time.load()) {
            std::memcpy(data, other.data, CACHE_ALIGNED_BLOCK_SIZE);
        }
        
        // Copy assignment operator
        CacheAlignedBlock& operator=(const CacheAlignedBlock& other) {
            if (this != &other) {
                std::memcpy(data, other.data, CACHE_ALIGNED_BLOCK_SIZE);
                in_use.store(other.in_use.load());
                access_count.store(other.access_count.load());
                last_access_time.store(other.last_access_time.load());
            }
            return *this;
        }
        
        // Move assignment operator
        CacheAlignedBlock& operator=(CacheAlignedBlock&& other) noexcept {
            if (this != &other) {
                std::memcpy(data, other.data, CACHE_ALIGNED_BLOCK_SIZE);
                in_use.store(other.in_use.load());
                access_count.store(other.access_count.load());
                last_access_time.store(other.last_access_time.load());
            }
            return *this;
        }
    };
    
    // Cache-aligned memory blocks
    std::vector<CacheAlignedBlock> cache_aligned_blocks_;
    std::atomic<size_t> next_block_;
    
    // Cache optimization
    std::atomic<size_t> cache_hits_;
    std::atomic<size_t> cache_misses_;
    std::atomic<size_t> hot_objects_count_;
    
    // Thread safety
    mutable std::mutex cache_mutex_;
    
    /**
     * @brief Get the next available cache-aligned block
     * @return Pointer to available block or nullptr
     */
    CacheAlignedBlock* get_next_available_block();
    
    /**
     * @brief Mark block as in use
     * @param block Pointer to the block to mark
     */
    void mark_block_in_use(CacheAlignedBlock* block);
    
    /**
     * @brief Mark block as free
     * @param block Pointer to the block to mark
     */
    void mark_block_free(CacheAlignedBlock* block);
    
    /**
     * @brief Update access statistics
     * @param block Pointer to the accessed block
     */
    void update_access_stats(CacheAlignedBlock* block);
    
    /**
     * @brief Identify hot objects for prefetching
     * @return Vector of pointers to hot blocks
     */
    std::vector<CacheAlignedBlock*> identify_hot_objects();
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_ENHANCED_LABELS_POOL_H_
