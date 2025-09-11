#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_TIERED_MEMORY_MANAGER_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_TIERED_MEMORY_MANAGER_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

/**
 * @brief Tiered Memory Manager Implementation
 * 
 * Provides tiered memory management with RAM/SSD/HDD allocation tiers.
 * Updated for linter cache refresh.
 */
class TieredMemoryManagerImpl {
public:
    explicit TieredMemoryManagerImpl(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config);
    virtual ~TieredMemoryManagerImpl() = default;
    
    // Memory tier management operations
    core::Result<void> add_series(const core::SeriesID& series_id, core::semantic_vector::MemoryTier tier);
    core::Result<void> remove_series(const core::SeriesID& series_id);
    core::Result<void*> get_series_memory(const core::SeriesID& series_id);
    core::Result<core::semantic_vector::MemoryTier> get_series_tier(const core::SeriesID& series_id) const;
    
    // Memory migration operations
    core::Result<void> promote_series(const core::SeriesID& series_id);
    core::Result<void> demote_series(const core::SeriesID& series_id);
    core::Result<void> migrate_series(const core::SeriesID& series_id, core::semantic_vector::MemoryTier target_tier);
    core::Result<void> optimize_tier_allocation();
    
    // Memory pressure management
    core::Result<void> handle_memory_pressure();
    core::Result<void> compact_tier(core::semantic_vector::MemoryTier tier);
    
    // Performance monitoring
    core::Result<core::PerformanceMetrics> get_performance_metrics() const;
    core::Result<void> reset_performance_metrics();
    
    // Configuration management
    void update_config(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config);
    core::semantic_vector::SemanticVectorConfig::MemoryConfig get_config() const;

private:
    // Performance monitoring structures
    struct PerformanceMonitoring {
        std::atomic<double> average_allocation_latency_ms{0.0};
        std::atomic<double> average_access_latency_ms{0.0};
        std::atomic<size_t> total_allocations{0};
        std::atomic<size_t> total_deallocations{0};
        std::atomic<size_t> total_migrations{0};
        std::atomic<size_t> total_memory_usage_bytes{0};
        std::atomic<double> memory_efficiency_ratio{1.0};
        std::atomic<size_t> allocation_errors{0};
        std::atomic<size_t> migration_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::MemoryConfig config_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal implementation structures
    std::vector<std::unique_ptr<class TierAllocator>> tier_allocators_;
    std::unordered_map<core::SeriesID, struct MemoryAllocation> allocations_;
    std::unique_ptr<class AccessPatternTracker> access_tracker_;
    std::unique_ptr<class MemoryMigrationEngine> migration_engine_;
    
    // Internal helper methods
    core::Result<void> initialize_tiered_memory_structures();
    core::Result<void> validate_series_id(const core::SeriesID& series_id) const;
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
};

// Factory functions
std::unique_ptr<TieredMemoryManagerImpl> CreateTieredMemoryManager(
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config);

std::unique_ptr<TieredMemoryManagerImpl> CreateTieredMemoryManagerForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& base_config = core::semantic_vector::SemanticVectorConfig::MemoryConfig());

core::Result<core::semantic_vector::ConfigValidationResult> ValidateTieredMemoryManagerConfig(
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_TIERED_MEMORY_MANAGER_H_