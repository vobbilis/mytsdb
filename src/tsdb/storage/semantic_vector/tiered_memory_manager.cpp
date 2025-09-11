#include "tsdb/storage/semantic_vector/tiered_memory_manager.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <mutex>
#include <cmath>
#include <random>
#include <queue>
#include <numeric>
#include <thread>

namespace tsdb {
namespace storage {
namespace semantic_vector {

// Namespace aliases for cleaner code
namespace core = ::tsdb::core;
using MemoryTier = core::semantic_vector::MemoryTier;
using MemoryPoolStats = core::semantic_vector::MemoryPoolStats;

// ============================================================================
// Minimal in-project tiered memory implementations
// ============================================================================

namespace {

/**
 * @brief Memory tier enumeration for tiered storage
 */
enum class MemoryTier {
    RAM = 0,    // Fastest access, highest cost
    SSD = 1,    // Medium access, medium cost
    HDD = 2     // Slowest access, lowest cost
};

/**
 * @brief Memory tier statistics
 */
struct TierStats {
    size_t total_capacity_bytes{0};
    size_t used_capacity_bytes{0};
    size_t allocation_count{0};
    double average_access_latency_ms{0.0};
    std::chrono::system_clock::time_point last_access;
    
    double get_utilization() const {
        return total_capacity_bytes > 0 ? 
               static_cast<double>(used_capacity_bytes) / static_cast<double>(total_capacity_bytes) : 0.0;
    }
};

/**
 * @brief Memory allocation entry for tracking
 */
struct MemoryAllocation {
    core::SeriesID series_id;
    MemoryTier tier;
    void* memory_ptr;
    size_t size_bytes;
    size_t access_count{0};
    std::chrono::system_clock::time_point last_access;
    std::chrono::system_clock::time_point created_at;
    bool is_pinned{false}; // Prevents automatic migration
    
    double get_access_frequency() const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::hours>(now - created_at).count();
        return duration > 0 ? static_cast<double>(access_count) / static_cast<double>(duration) : 0.0;
    }
};

/**
 * @brief Simple memory allocator for each tier
 */
class TierAllocator {
public:
    explicit TierAllocator(MemoryTier tier, size_t capacity_bytes) 
        : tier_(tier), capacity_bytes_(capacity_bytes), used_bytes_(0) {
        // Simulate different access latencies for different tiers
        switch (tier) {
            case MemoryTier::RAM:
                access_latency_ms_ = 0.01; // 10 microseconds
                break;
            case MemoryTier::SSD:
                access_latency_ms_ = 0.1;  // 100 microseconds
                break;
            case MemoryTier::HDD:
                access_latency_ms_ = 5.0;  // 5 milliseconds
                break;
        }
    }
    
    core::Result<void*> allocate(size_t size_bytes) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (used_bytes_ + size_bytes > capacity_bytes_) {
            return core::Result<void*>::error("Tier capacity exceeded");
        }
        
        // Simulate allocation (in production, this would use actual memory management)
        void* ptr = std::malloc(size_bytes);
        if (!ptr) {
            return core::Result<void*>::error("Memory allocation failed");
        }
        
        allocations_[ptr] = size_bytes;
        used_bytes_ += size_bytes;
        stats_.allocation_count++;
        stats_.used_capacity_bytes = used_bytes_;
        stats_.total_capacity_bytes = capacity_bytes_;
        
        return core::Result<void*>(ptr);
    }
    
    core::Result<void> deallocate(void* ptr) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto it = allocations_.find(ptr);
        if (it == allocations_.end()) {
            return core::Result<void>::error("Invalid pointer for deallocation");
        }
        
        size_t size_bytes = it->second;
        allocations_.erase(it);
        used_bytes_ -= size_bytes;
        stats_.used_capacity_bytes = used_bytes_;
        
        std::free(ptr);
        return core::Result<void>();
    }
    
    void update_access_time() {
        stats_.last_access = std::chrono::system_clock::now();
    }
    
    TierStats get_stats() const {
        std::unique_lock<std::mutex> lock(mutex_);
        auto stats = stats_;
        stats.average_access_latency_ms = access_latency_ms_;
        return stats;
    }
    
    size_t get_available_bytes() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return capacity_bytes_ - used_bytes_;
    }
    
    double get_utilization() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return capacity_bytes_ > 0 ? 
               static_cast<double>(used_bytes_) / static_cast<double>(capacity_bytes_) : 0.0;
    }
    
private:
    MemoryTier tier_;
    size_t capacity_bytes_;
    size_t used_bytes_;
    double access_latency_ms_;
    TierStats stats_;
    std::unordered_map<void*, size_t> allocations_;
    mutable std::mutex mutex_;
};

/**
 * @brief Access pattern tracker for intelligent tier management
 */
class AccessPatternTracker {
public:
    void record_access(const core::SeriesID& series_id, MemoryTier tier) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto& pattern = access_patterns_[series_id];
        pattern.total_accesses++;
        pattern.last_access = std::chrono::system_clock::now();
        pattern.current_tier = tier;
        
        // Update access frequency (exponential moving average)
        auto now = std::chrono::system_clock::now();
        if (pattern.first_access == std::chrono::system_clock::time_point{}) {
            pattern.first_access = now;
        }
        
        auto duration_hours = std::chrono::duration_cast<std::chrono::hours>(now - pattern.first_access).count();
        if (duration_hours > 0) {
            pattern.access_frequency = static_cast<double>(pattern.total_accesses) / static_cast<double>(duration_hours);
        }
        
        // Track tier access distribution
        pattern.tier_access_counts[tier]++;
    }
    
    struct AccessPattern {
        size_t total_accesses{0};
        double access_frequency{0.0}; // Accesses per hour
        std::chrono::system_clock::time_point first_access;
        std::chrono::system_clock::time_point last_access;
        MemoryTier current_tier{MemoryTier::RAM};
        std::map<MemoryTier, size_t> tier_access_counts;
        
        bool should_promote() const {
            // Promote if frequently accessed and not in RAM
            return access_frequency > 10.0 && current_tier != MemoryTier::RAM;
        }
        
        bool should_demote() const {
            // Demote if rarely accessed and in higher tier
            auto now = std::chrono::system_clock::now();
            auto idle_time = std::chrono::duration_cast<std::chrono::hours>(now - last_access).count();
            return idle_time > 24 && current_tier != MemoryTier::HDD; // Idle for 24+ hours
        }
    };
    
    core::Result<AccessPattern> get_pattern(const core::SeriesID& series_id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = access_patterns_.find(series_id);
        if (it == access_patterns_.end()) {
            return core::Result<AccessPattern>(AccessPattern{});
        }
        return core::Result<AccessPattern>(it->second);
    }
    
    std::vector<core::SeriesID> get_promotion_candidates() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<core::SeriesID> candidates;
        
        for (const auto& [series_id, pattern] : access_patterns_) {
            if (pattern.should_promote()) {
                candidates.push_back(series_id);
            }
        }
        
        return candidates;
    }
    
    std::vector<core::SeriesID> get_demotion_candidates() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<core::SeriesID> candidates;
        
        for (const auto& [series_id, pattern] : access_patterns_) {
            if (pattern.should_demote()) {
                candidates.push_back(series_id);
            }
        }
        
        return candidates;
    }
    
    void remove_pattern(const core::SeriesID& series_id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        access_patterns_.erase(series_id);
    }
    
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<core::SeriesID, AccessPattern> access_patterns_;
};

/**
 * @brief Memory migration engine for automatic tier management
 */
class MemoryMigrationEngine {
public:
    explicit MemoryMigrationEngine(
        std::vector<std::unique_ptr<TierAllocator>>* tier_allocators,
        std::unordered_map<core::SeriesID, MemoryAllocation>* allocations,
        AccessPatternTracker* access_tracker) 
        : tier_allocators_(tier_allocators), allocations_(allocations), access_tracker_(access_tracker) {}
    
    core::Result<void> migrate_series(const core::SeriesID& series_id, MemoryTier target_tier) {
        auto it = allocations_->find(series_id);
        if (it == allocations_->end()) {
            return core::Result<void>::error("Series not found in memory");
        }
        
        auto& allocation = it->second;
        if (allocation.tier == target_tier) {
            return core::Result<void>(); // Already in target tier
        }
        
        if (allocation.is_pinned) {
            return core::Result<void>::error("Series is pinned and cannot be migrated");
        }
        
        // Allocate memory in target tier
        auto& target_allocator = (*tier_allocators_)[static_cast<size_t>(target_tier)];
        auto new_ptr_result = target_allocator->allocate(allocation.size_bytes);
        if (!new_ptr_result.ok()) {
            return core::Result<void>::error("Failed to allocate memory in target tier: " + new_ptr_result.error());
        }
        
        void* new_ptr = new_ptr_result.value();
        
        // Copy data (simulated)
        std::memcpy(new_ptr, allocation.memory_ptr, allocation.size_bytes);
        
        // Deallocate old memory
        auto& source_allocator = (*tier_allocators_)[static_cast<size_t>(allocation.tier)];
        auto dealloc_result = source_allocator->deallocate(allocation.memory_ptr);
        if (!dealloc_result.ok()) {
            // Try to cleanup new allocation
            (void)target_allocator->deallocate(new_ptr);
            return core::Result<void>::error("Failed to deallocate old memory: " + dealloc_result.error());
        }
        
        // Update allocation
        allocation.memory_ptr = new_ptr;
        allocation.tier = target_tier;
        
        return core::Result<void>();
    }
    
    core::Result<size_t> perform_automatic_migration() {
        size_t migrations_performed = 0;
        
        // Promote frequently accessed data
        auto promotion_candidates = access_tracker_->get_promotion_candidates();
        for (const auto& series_id : promotion_candidates) {
            MemoryTier target_tier = MemoryTier::RAM;
            auto migrate_result = migrate_series(series_id, target_tier);
            if (migrate_result.ok()) {
                migrations_performed++;
            }
        }
        
        // Demote rarely accessed data
        auto demotion_candidates = access_tracker_->get_demotion_candidates();
        for (const auto& series_id : demotion_candidates) {
            auto it = allocations_->find(series_id);
            if (it != allocations_->end()) {
                MemoryTier target_tier = (it->second.tier == MemoryTier::RAM) ? MemoryTier::SSD : MemoryTier::HDD;
                auto migrate_result = migrate_series(series_id, target_tier);
                if (migrate_result.ok()) {
                    migrations_performed++;
                }
            }
        }
        
        return core::Result<size_t>(migrations_performed);
    }
    
private:
    std::vector<std::unique_ptr<TierAllocator>>* tier_allocators_;
    std::unordered_map<core::SeriesID, MemoryAllocation>* allocations_;
    AccessPatternTracker* access_tracker_;
};

} // anonymous namespace

// ============================================================================
// TIERED MEMORY MANAGER IMPLEMENTATION
// ============================================================================

TieredMemoryManagerImpl::TieredMemoryManagerImpl(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config)
    : config_(config) {
    // Initialize tiered memory structures
    (void)this->initialize_tiered_memory_structures();
}

// ============================================================================
// MEMORY TIER MANAGEMENT
// ============================================================================

core::Result<void> TieredMemoryManagerImpl::add_series(const core::SeriesID& series_id, MemoryTier tier) {
    auto start = std::chrono::high_resolution_clock::now();
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        // Check if series already exists
        if (allocations_.find(series_id) != allocations_.end()) {
            return core::Result<void>::error("Series already exists in memory");
        }
        
        // Convert unified tier type to internal tier type
        MemoryTier internal_tier;
        switch (tier) {
            case core::semantic_vector::MemoryTier::RAM:
                internal_tier = MemoryTier::RAM;
                break;
            case core::semantic_vector::MemoryTier::SSD:
                internal_tier = MemoryTier::SSD;
                break;
            case core::semantic_vector::MemoryTier::HDD:
                internal_tier = MemoryTier::HDD;
                break;
            default:
                internal_tier = MemoryTier::RAM;
                break;
        }
        
        // Allocate memory in specified tier
        size_t allocation_size = 1024; // Default allocation size
        auto& tier_allocator = tier_allocators_[static_cast<size_t>(internal_tier)];
        auto alloc_result = tier_allocator->allocate(allocation_size);
        if (!alloc_result.ok()) {
            return core::Result<void>::error("Failed to allocate memory in tier: " + alloc_result.error());
        }
        
        // Create allocation record
        MemoryAllocation allocation;
        allocation.series_id = series_id;
        allocation.tier = internal_tier;
        allocation.memory_ptr = alloc_result.value();
        allocation.size_bytes = allocation_size;
        allocation.created_at = std::chrono::system_clock::now();
        allocation.last_access = allocation.created_at;
        
        allocations_[series_id] = allocation;
        
        // Record access pattern
        if (access_tracker_) {
            access_tracker_->record_access(series_id, internal_tier);
        }
        
        // Update metrics
        const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocations.fetch_add(1);
        const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_memory_usage_bytes.fetch_add(allocation_size);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end - start).count();
    (void)this->update_performance_metrics("add_series", latency, true);
    
    return core::Result<void>();
}

core::Result<void> TieredMemoryManagerImpl::remove_series(const core::SeriesID& series_id) {
    auto start = std::chrono::high_resolution_clock::now();
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = allocations_.find(series_id);
        if (it == allocations_.end()) {
            return core::Result<void>::error("Series not found in memory");
        }
        
        const auto& allocation = it->second;
        
        // Deallocate memory
        auto& tier_allocator = tier_allocators_[static_cast<size_t>(allocation.tier)];
        auto dealloc_result = tier_allocator->deallocate(allocation.memory_ptr);
        if (!dealloc_result.ok()) {
            return core::Result<void>::error("Failed to deallocate memory: " + dealloc_result.error());
        }
        
        // Remove allocation record
        const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_memory_usage_bytes.fetch_sub(allocation.size_bytes);
        allocations_.erase(it);
        
        // Remove access pattern
        if (access_tracker_) {
            access_tracker_->remove_pattern(series_id);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end - start).count();
    (void)this->update_performance_metrics("remove_series", latency, true);
    
    return core::Result<void>();
}

core::Result<void*> TieredMemoryManagerImpl::get_series_memory(const core::SeriesID& series_id) {
    auto start = std::chrono::high_resolution_clock::now();
    
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto it = allocations_.find(series_id);
        if (it == allocations_.end()) {
            return core::Result<void*>::error("Series not found in memory");
        }
        
        auto& allocation = const_cast<MemoryAllocation&>(it->second);
        allocation.last_access = std::chrono::system_clock::now();
        allocation.access_count++;
        
        // Record access pattern
        if (access_tracker_) {
            access_tracker_->record_access(series_id, allocation.tier);
        }
        
        // Update tier access statistics
        tier_allocators_[static_cast<size_t>(allocation.tier)]->update_access_time();
        
        auto end = std::chrono::high_resolution_clock::now();
        double latency = std::chrono::duration<double, std::milli>(end - start).count();
        const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_access_latency_ms.store(latency);
        
        return core::Result<void*>(allocation.memory_ptr);
    }
}

core::Result<core::semantic_vector::MemoryTier> TieredMemoryManagerImpl::get_series_tier(const core::SeriesID& series_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = allocations_.find(series_id);
    if (it == allocations_.end()) {
        return core::Result<core::semantic_vector::MemoryTier>::error("Series not found in memory");
    }
    
    // Convert internal tier type to unified tier type
    core::semantic_vector::MemoryTier unified_tier;
    switch (it->second.tier) {
        case MemoryTier::RAM:
            unified_tier = core::semantic_vector::MemoryTier::RAM;
            break;
        case MemoryTier::SSD:
            unified_tier = core::semantic_vector::MemoryTier::SSD;
            break;
        case MemoryTier::HDD:
            unified_tier = core::semantic_vector::MemoryTier::HDD;
            break;
        default:
            unified_tier = core::semantic_vector::MemoryTier::RAM;
            break;
    }
    
    return core::Result<core::semantic_vector::MemoryTier>(unified_tier);
}

// ============================================================================
// MEMORY MIGRATION OPERATIONS
// ============================================================================

core::Result<void> TieredMemoryManagerImpl::promote_series(const core::SeriesID& series_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (migration_engine_) {
        return migration_engine_->migrate_series(series_id, MemoryTier::RAM);
    }
    
    return core::Result<void>::error("Migration engine not initialized");
}

core::Result<void> TieredMemoryManagerImpl::demote_series(const core::SeriesID& series_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = allocations_.find(series_id);
    if (it == allocations_.end()) {
        return core::Result<void>::error("Series not found in memory");
    }
    
    MemoryTier target_tier = (it->second.tier == MemoryTier::RAM) ? MemoryTier::SSD : MemoryTier::HDD;
    
    if (migration_engine_) {
        return migration_engine_->migrate_series(series_id, target_tier);
    }
    
    return core::Result<void>::error("Migration engine not initialized");
}

core::Result<void> TieredMemoryManagerImpl::migrate_series(const core::SeriesID& series_id, core::semantic_vector::MemoryTier target_tier) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Convert unified tier type to internal tier type
    MemoryTier internal_tier;
    switch (target_tier) {
        case core::semantic_vector::MemoryTier::RAM:
            internal_tier = MemoryTier::RAM;
            break;
        case core::semantic_vector::MemoryTier::SSD:
            internal_tier = MemoryTier::SSD;
            break;
        case core::semantic_vector::MemoryTier::HDD:
            internal_tier = MemoryTier::HDD;
            break;
        default:
            internal_tier = MemoryTier::RAM;
            break;
    }
    
    if (migration_engine_) {
        return migration_engine_->migrate_series(series_id, internal_tier);
    }
    
    return core::Result<void>::error("Migration engine not initialized");
}

core::Result<void> TieredMemoryManagerImpl::optimize_tier_allocation() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (migration_engine_) {
        auto migrations_result = migration_engine_->perform_automatic_migration();
        if (migrations_result.ok()) {
            const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_migrations.fetch_add(migrations_result.value());
        }
        return core::Result<void>();
    }
    
    return core::Result<void>::error("Migration engine not initialized");
}

// ============================================================================
// MEMORY PRESSURE MANAGEMENT
// ============================================================================

core::Result<void> TieredMemoryManagerImpl::handle_memory_pressure() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Find candidates for demotion to free up higher-tier memory
    if (access_tracker_) {
        auto demotion_candidates = access_tracker_->get_demotion_candidates();
        
        for (const auto& series_id : demotion_candidates) {
            if (migration_engine_) {
                auto it = allocations_.find(series_id);
                if (it != allocations_.end()) {
                    MemoryTier target_tier = (it->second.tier == MemoryTier::RAM) ? MemoryTier::SSD : MemoryTier::HDD;
                    (void)migration_engine_->migrate_series(series_id, target_tier);
                }
            }
        }
    }
    
    return core::Result<void>();
}

core::Result<void> TieredMemoryManagerImpl::compact_tier(core::semantic_vector::MemoryTier tier) {
    // Placeholder for tier compaction
    // In production, this would implement memory defragmentation
    (void)tier;
    return core::Result<void>();
}

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

core::Result<core::PerformanceMetrics> TieredMemoryManagerImpl::get_performance_metrics() const {
    core::PerformanceMetrics m;
    m.total_memory_usage_bytes = this->performance_monitoring_.total_memory_usage_bytes.load();
    m.vector_memory_usage_bytes = this->performance_monitoring_.total_memory_usage_bytes.load() / 3;
    m.semantic_memory_usage_bytes = this->performance_monitoring_.total_memory_usage_bytes.load() / 3;
    m.temporal_memory_usage_bytes = this->performance_monitoring_.total_memory_usage_bytes.load() / 3;
    m.memory_compression_ratio = this->performance_monitoring_.memory_efficiency_ratio.load();
    m.average_vector_search_time_ms = this->performance_monitoring_.average_access_latency_ms.load();
    m.average_semantic_search_time_ms = this->performance_monitoring_.average_allocation_latency_ms.load();
    m.average_correlation_time_ms = 0.0;
    m.average_inference_time_ms = 0.0;
    m.vector_search_accuracy = 1.0;
    m.semantic_search_accuracy = 1.0;
    m.correlation_accuracy = 1.0;
    m.inference_accuracy = 1.0;
    m.queries_per_second = this->performance_monitoring_.total_allocations.load();
    m.vectors_processed_per_second = this->performance_monitoring_.total_allocations.load();
    m.correlations_computed_per_second = this->performance_monitoring_.total_migrations.load();
    m.recorded_at = std::chrono::system_clock::now();
    return core::Result<core::PerformanceMetrics>(m);
}

core::Result<void> TieredMemoryManagerImpl::reset_performance_metrics() {
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_allocation_latency_ms.store(0.0);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_access_latency_ms.store(0.0);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocations.store(0);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_deallocations.store(0);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_migrations.store(0);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_memory_usage_bytes.store(0);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).memory_efficiency_ratio.store(1.0);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).allocation_errors.store(0);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).migration_errors.store(0);
    return core::Result<void>();
}

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

void TieredMemoryManagerImpl::update_config(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    this->config_ = config;
}

core::semantic_vector::SemanticVectorConfig::MemoryConfig TieredMemoryManagerImpl::get_config() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return this->config_;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

core::Result<void> TieredMemoryManagerImpl::initialize_tiered_memory_structures() {
    try {
        // Initialize tier allocators
        tier_allocators_.resize(3); // RAM, SSD, HDD
        tier_allocators_[0] = std::make_unique<TierAllocator>(MemoryTier::RAM, this->config_.ram_tier_capacity_mb * 1024 * 1024);
        tier_allocators_[1] = std::make_unique<TierAllocator>(MemoryTier::SSD, this->config_.ssd_tier_capacity_mb * 1024 * 1024);
        tier_allocators_[2] = std::make_unique<TierAllocator>(MemoryTier::HDD, this->config_.hdd_tier_capacity_mb * 1024 * 1024);
        
        // Initialize access tracker
        access_tracker_ = std::make_unique<AccessPatternTracker>();
        
        // Initialize migration engine
        migration_engine_ = std::make_unique<MemoryMigrationEngine>(&tier_allocators_, &allocations_, access_tracker_.get());
        
    } catch (const std::exception& e) {
        return core::Result<void>::error("Failed to initialize tiered memory structures: " + std::string(e.what()));
    }
    
    return core::Result<void>();
}

core::Result<void> TieredMemoryManagerImpl::validate_series_id(const core::SeriesID& series_id) const {
    if (series_id == 0) {
        return core::Result<void>::error("Invalid series ID: cannot be zero");
    }
    return core::Result<void>();
}

core::Result<void> TieredMemoryManagerImpl::update_performance_metrics(const std::string& operation, double latency, bool success) const {
    (void)operation;
    (void)success;
    
    // Update average allocation latency using exponential moving average
    size_t n = const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocations.fetch_add(1) + 1;
    double prev = const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_allocation_latency_ms.load();
    double updated = prev + (latency - prev) / static_cast<double>(n);
    const_cast<TieredMemoryManagerImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_allocation_latency_ms.store(updated);
    
    return core::Result<void>();
}

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

std::unique_ptr<TieredMemoryManagerImpl> CreateTieredMemoryManager(
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) {
    return std::unique_ptr<TieredMemoryManagerImpl>(new TieredMemoryManagerImpl(config));
}

std::unique_ptr<TieredMemoryManagerImpl> CreateTieredMemoryManagerForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& base_config) {
    
    auto config = base_config;
    
    if (use_case == "high_performance") {
        config.ram_tier_capacity_mb = 2048;  // 2GB RAM
        config.ssd_tier_capacity_mb = 10240; // 10GB SSD
        config.hdd_tier_capacity_mb = 51200; // 50GB HDD
        config.enable_tiered_memory = true;
    } else if (use_case == "memory_efficient") {
        config.ram_tier_capacity_mb = 512;   // 512MB RAM
        config.ssd_tier_capacity_mb = 5120;  // 5GB SSD
        config.hdd_tier_capacity_mb = 102400; // 100GB HDD
        config.enable_tiered_memory = true;
        config.enable_delta_compression = true;
        config.enable_dictionary_compression = true;
    } else if (use_case == "high_accuracy") {
        config.ram_tier_capacity_mb = 4096;  // 4GB RAM
        config.ssd_tier_capacity_mb = 20480; // 20GB SSD
        config.hdd_tier_capacity_mb = 204800; // 200GB HDD
        config.enable_tiered_memory = true;
    }
    
    return std::unique_ptr<TieredMemoryManagerImpl>(new TieredMemoryManagerImpl(config));
}

core::Result<core::semantic_vector::ConfigValidationResult> ValidateTieredMemoryManagerConfig(
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) {
    
    core::semantic_vector::ConfigValidationResult res{};
    res.is_valid = true;
    
    if (config.ram_tier_capacity_mb == 0) {
        res.is_valid = false;
        res.errors.push_back("ram_tier_capacity_mb must be > 0");
    }
    if (config.ssd_tier_capacity_mb == 0) {
        res.warnings.push_back("ssd_tier_capacity_mb is 0; SSD tier may be disabled");
    }
    if (config.hdd_tier_capacity_mb == 0) {
        res.warnings.push_back("hdd_tier_capacity_mb is 0; HDD tier may be disabled");
    }
    if (config.target_memory_reduction < 0.0 || config.target_memory_reduction > 1.0) {
        res.is_valid = false;
        res.errors.push_back("target_memory_reduction must be between 0.0 and 1.0");
    }
    
    return core::Result<core::semantic_vector::ConfigValidationResult>(res);
}

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb
