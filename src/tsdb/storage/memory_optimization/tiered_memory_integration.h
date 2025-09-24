#ifndef TSDB_STORAGE_TIERED_MEMORY_INTEGRATION_H_
#define TSDB_STORAGE_TIERED_MEMORY_INTEGRATION_H_

// Removed semantic vector dependencies - Phase 1 should be independent
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/config.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace tsdb {
namespace storage {

// Simple tiered memory management (replacing semantic vector dependencies)
enum class MemoryTier {
    RAM = 0,
    SSD = 1,
    HDD = 2
};

/**
 * @brief Simple tiered memory manager implementation
 */
class SimpleTieredMemoryManager {
public:
    SimpleTieredMemoryManager() = default;
    ~SimpleTieredMemoryManager() = default;
    
    // Simple tiered memory operations
    core::Result<void> add_series(const core::SeriesID& series_id, MemoryTier tier);
    core::Result<void> migrate_series(const core::SeriesID& series_id, MemoryTier target_tier);
    core::Result<MemoryTier> get_series_tier(const core::SeriesID& series_id);
    
private:
    std::unordered_map<core::SeriesID, MemoryTier> series_tiers_;
    std::mutex tier_mutex_;
};

/**
 * @brief Integration layer for existing TieredMemoryManager with StorageImpl
 * 
 * This class provides a bridge between the existing semantic vector tiered
 * memory management infrastructure and the StorageImpl optimization needs.
 */
class TieredMemoryIntegration {
public:
    /**
     * @brief Constructor
     * @param config Memory configuration for the integration
     */
    explicit TieredMemoryIntegration(const core::StorageConfig& config);
    
    /**
     * @brief Destructor
     */
    ~TieredMemoryIntegration();
    
    /**
     * @brief Initialize the integration with existing infrastructure
     * @return Result indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Add series to tiered memory management
     * @param series_id Series ID to add
     * @param tier Initial memory tier
     * @return Result indicating success or failure
     */
    core::Result<void> add_series(const core::SeriesID& series_id, 
                                  MemoryTier tier);
    
    /**
     * @brief Remove series from tiered memory management
     * @param series_id Series ID to remove
     * @return Result indicating success or failure
     */
    core::Result<void> remove_series(const core::SeriesID& series_id);
    
    /**
     * @brief Promote series to faster memory tier
     * @param series_id Series ID to promote
     * @return Result indicating success or failure
     */
    core::Result<void> promote_series(const core::SeriesID& series_id);
    
    /**
     * @brief Demote series to slower memory tier
     * @param series_id Series ID to demote
     * @return Result indicating success or failure
     */
    core::Result<void> demote_series(const core::SeriesID& series_id);
    
    /**
     * @brief Get current tier for a series
     * @param series_id Series ID to query
     * @return Result containing current tier or error
     */
    core::Result<MemoryTier> get_series_tier(const core::SeriesID& series_id);
    
    /**
     * @brief Optimize tiered memory layout based on access patterns
     * @return Result indicating success or failure
     */
    core::Result<void> optimize_tiered_layout();
    
    /**
     * @brief Get tiered memory statistics
     * @return String containing tiered memory statistics
     */
    std::string get_tiered_stats() const;
    
    /**
     * @brief Get series tier statistics
     * @return String containing series tier statistics
     */
    std::string get_series_tier_stats() const;
    
    /**
     * @brief Get migration statistics
     * @return String containing migration statistics
     */
    std::string get_migration_stats() const;

private:
    // Simple tiered memory management (replacing semantic vector dependencies)
    
    // Series tracking
    struct SeriesInfo {
        MemoryTier current_tier;
        std::atomic<size_t> access_count;
        std::atomic<uint64_t> last_access_time;
        std::atomic<bool> is_hot;
        std::chrono::system_clock::time_point created_at;
        
        // Default constructor
        SeriesInfo() : current_tier(MemoryTier::RAM), access_count(0), last_access_time(0), is_hot(false) {}
        
        // Copy constructor
        SeriesInfo(const SeriesInfo& other) 
            : current_tier(other.current_tier), access_count(other.access_count.load()), 
              last_access_time(other.last_access_time.load()), is_hot(other.is_hot.load()),
              created_at(other.created_at) {}
        
        // Move constructor
        SeriesInfo(SeriesInfo&& other) noexcept
            : current_tier(other.current_tier), access_count(other.access_count.load()), 
              last_access_time(other.last_access_time.load()), is_hot(other.is_hot.load()),
              created_at(other.created_at) {}
        
        // Copy assignment operator
        SeriesInfo& operator=(const SeriesInfo& other) {
            if (this != &other) {
                current_tier = other.current_tier;
                access_count.store(other.access_count.load());
                last_access_time.store(other.last_access_time.load());
                is_hot.store(other.is_hot.load());
                created_at = other.created_at;
            }
            return *this;
        }
        
        // Move assignment operator
        SeriesInfo& operator=(SeriesInfo&& other) noexcept {
            if (this != &other) {
                current_tier = other.current_tier;
                access_count.store(other.access_count.load());
                last_access_time.store(other.last_access_time.load());
                is_hot.store(other.is_hot.load());
                created_at = other.created_at;
            }
            return *this;
        }
    };
    
    std::unordered_map<core::SeriesID, SeriesInfo> series_info_;
    mutable std::mutex series_mutex_;
    
    // Migration tracking
    std::atomic<size_t> total_migrations_;
    std::atomic<size_t> successful_migrations_;
    std::atomic<size_t> failed_migrations_;
    
    // Tier statistics
    std::atomic<size_t> ram_series_count_;
    std::atomic<size_t> ssd_series_count_;
    std::atomic<size_t> hdd_series_count_;
    
    // Configuration
    core::StorageConfig config_;
    
    /**
     * @brief Update series access information
     * @param series_id Series ID to update
     */
    void update_series_access(const core::SeriesID& series_id);
    
    /**
     * @brief Identify hot series for promotion
     * @return Vector of series IDs that should be promoted
     */
    std::vector<core::SeriesID> identify_hot_series();
    
    /**
     * @brief Identify cold series for demotion
     * @return Vector of series IDs that should be demoted
     */
    std::vector<core::SeriesID> identify_cold_series();
    
    /**
     * @brief Migrate series between tiers
     * @param series_id Series ID to migrate
     * @param target_tier Target memory tier
     * @return Result indicating success or failure
     */
    core::Result<void> migrate_series(const core::SeriesID& series_id, 
                                      MemoryTier target_tier);
    
    /**
     * @brief Update tier statistics
     * @param old_tier Previous tier
     * @param new_tier New tier
     */
    void update_tier_statistics(MemoryTier old_tier, 
                                MemoryTier new_tier);
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_TIERED_MEMORY_INTEGRATION_H_
