#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_ADAPTIVE_MEMORY_POOL_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_ADAPTIVE_MEMORY_POOL_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <chrono>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

/**
 * @brief Adaptive Memory Pool Implementation
 * 
 * Provides adaptive memory allocation with size class management and defragmentation.
 */
class AdaptiveMemoryPoolImpl {
public:
    explicit AdaptiveMemoryPoolImpl(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config);
    virtual ~AdaptiveMemoryPoolImpl() = default;
    
    // Memory allocation operations
    core::Result<void*> allocate(size_t size_bytes, size_t alignment = sizeof(void*));
    core::Result<void> deallocate(void* ptr);
    core::Result<void*> reallocate(void* ptr, size_t new_size_bytes);
    core::Result<void> record_access(void* ptr);
    
    // Memory optimization operations
    core::Result<void> defragment();
    core::Result<void> compact();
    core::Result<void> optimize_allocation_strategy();
    
    // Memory statistics
    core::Result<core::semantic_vector::MemoryPoolStats> get_pool_stats() const;
    core::Result<double> get_allocation_efficiency() const;
    core::Result<double> get_fragmentation_ratio() const;
    
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
        std::atomic<double> average_deallocation_latency_ms{0.0};
        std::atomic<size_t> total_allocations{0};
        std::atomic<size_t> total_deallocations{0};
        std::atomic<size_t> total_allocated_bytes{0};
        std::atomic<double> allocation_efficiency{1.0};
        std::atomic<size_t> total_defragmentations{0};
        std::atomic<size_t> total_compactions{0};
        std::atomic<size_t> total_optimizations{0};
        std::atomic<double> average_defragmentation_time_ms{0.0};
        std::atomic<double> average_compaction_time_ms{0.0};
        std::atomic<size_t> allocation_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::MemoryConfig config_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal implementation structures
    std::unique_ptr<class SizeClassAllocator> size_class_allocator_;
    std::unique_ptr<class AllocationPatternTracker> pattern_tracker_;
    
    // Allocation tracking
    struct AllocationEntry {
        void* ptr;
        size_t size;
        size_t alignment;
        std::chrono::system_clock::time_point allocated_at;
        std::chrono::system_clock::time_point last_accessed;
        size_t access_count{0};
    };
    std::unordered_map<void*, AllocationEntry> allocations_;
    
    // Internal helper methods
    core::Result<void> initialize_adaptive_pool_structures();
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
};

// Factory functions
std::unique_ptr<AdaptiveMemoryPoolImpl> CreateAdaptiveMemoryPool(
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config);

std::unique_ptr<AdaptiveMemoryPoolImpl> CreateAdaptiveMemoryPoolForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& base_config = core::semantic_vector::SemanticVectorConfig::MemoryConfig());

core::Result<core::semantic_vector::ConfigValidationResult> ValidateAdaptiveMemoryPoolConfig(
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_ADAPTIVE_MEMORY_POOL_H_