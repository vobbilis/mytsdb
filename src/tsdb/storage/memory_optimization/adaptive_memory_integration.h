#ifndef TSDB_STORAGE_ADAPTIVE_MEMORY_INTEGRATION_H_
#define TSDB_STORAGE_ADAPTIVE_MEMORY_INTEGRATION_H_

// Removed semantic vector dependencies - Phase 1 should be independent
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/config.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace tsdb {
namespace storage {

/**
 * @brief Memory tier enumeration for adaptive memory management
 */
enum class MemoryTier {
    RAM,    // Fastest tier - main memory
    SSD,    // Medium tier - solid state storage
    HDD     // Slowest tier - hard disk storage
};

/**
 * @brief Integration layer for existing AdaptiveMemoryPool with StorageImpl
 * 
 * This class provides a bridge between the existing semantic vector memory
 * management infrastructure and the StorageImpl optimization needs.
 */
class AdaptiveMemoryIntegration {
public:
    /**
     * @brief Constructor
     * @param config Memory configuration for the integration
     */
    explicit AdaptiveMemoryIntegration(const core::StorageConfig& config);
    
    /**
     * @brief Destructor
     */
    ~AdaptiveMemoryIntegration();
    
    /**
     * @brief Initialize the integration with existing infrastructure
     * @return Result indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Allocate memory using the adaptive memory pool
     * @param size Size of memory to allocate
     * @param alignment Memory alignment requirement
     * @return Result containing allocated pointer or error
     */
    core::Result<void*> allocate_optimized(size_t size, size_t alignment = 64);
    
    /**
     * @brief Deallocate memory using the adaptive memory pool
     * @param ptr Pointer to deallocate
     * @return Result indicating success or failure
     */
    core::Result<void> deallocate_optimized(void* ptr);
    
    /**
     * @brief Record access pattern for optimization
     * @param ptr Pointer that was accessed
     * @return Result indicating success or failure
     */
    core::Result<void> record_access_pattern(void* ptr);
    
    /**
     * @brief Optimize memory layout based on access patterns
     * @return Result indicating success or failure
     */
    core::Result<void> optimize_memory_layout();
    
    /**
     * @brief Promote hot data to faster memory tier
     * @param series_id Series ID to promote
     * @return Result indicating success or failure
     */
    core::Result<void> promote_hot_data(const core::SeriesID& series_id);
    
    /**
     * @brief Demote cold data to slower memory tier
     * @param series_id Series ID to demote
     * @return Result indicating success or failure
     */
    core::Result<void> demote_cold_data(const core::SeriesID& series_id);
    
    /**
     * @brief Get memory usage statistics
     * @return String containing memory usage statistics
     */
    std::string get_memory_stats() const;
    
    /**
     * @brief Get access pattern statistics
     * @return String containing access pattern statistics
     */
    std::string get_access_pattern_stats() const;
    
    /**
     * @brief Get tiered memory statistics
     * @return String containing tiered memory statistics
     */
    std::string get_tiered_memory_stats() const;

private:
    // Simple memory management (replacing semantic vector dependencies)
    std::vector<void*> allocated_blocks_;
    std::atomic<size_t> total_allocated_size_;
    
    // Access pattern tracking
    struct AccessPattern {
        std::atomic<size_t> access_count;
        std::atomic<uint64_t> last_access_time;
        std::atomic<bool> is_hot;
        
        // Default constructor
        AccessPattern() : access_count(0), last_access_time(0), is_hot(false) {}
        
        // Copy constructor
        AccessPattern(const AccessPattern& other) 
            : access_count(other.access_count.load()), 
              last_access_time(other.last_access_time.load()),
              is_hot(other.is_hot.load()) {}
        
        // Move constructor
        AccessPattern(AccessPattern&& other) noexcept
            : access_count(other.access_count.load()), 
              last_access_time(other.last_access_time.load()),
              is_hot(other.is_hot.load()) {}
        
        // Copy assignment operator
        AccessPattern& operator=(const AccessPattern& other) {
            if (this != &other) {
                access_count.store(other.access_count.load());
                last_access_time.store(other.last_access_time.load());
                is_hot.store(other.is_hot.load());
            }
            return *this;
        }
        
        // Move assignment operator
        AccessPattern& operator=(AccessPattern&& other) noexcept {
            if (this != &other) {
                access_count.store(other.access_count.load());
                last_access_time.store(other.last_access_time.load());
                is_hot.store(other.is_hot.load());
            }
            return *this;
        }
    };
    
    std::unordered_map<void*, AccessPattern> access_patterns_;
    mutable std::mutex access_mutex_;
    
    // Memory optimization
    std::atomic<size_t> total_allocations_;
    std::atomic<size_t> total_deallocations_;
    std::atomic<size_t> hot_data_count_;
    std::atomic<size_t> cold_data_count_;
    
    // Configuration
    core::StorageConfig config_;
    
    /**
     * @brief Update access pattern for a pointer
     * @param ptr Pointer to update
     */
    void update_access_pattern(void* ptr);
    
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
     * @brief Migrate data between memory tiers
     * @param ptr Pointer to migrate
     * @param target_tier Target memory tier
     * @return Result indicating success or failure
     */
    core::Result<void> migrate_data(void* ptr, MemoryTier target_tier);
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_ADAPTIVE_MEMORY_INTEGRATION_H_
