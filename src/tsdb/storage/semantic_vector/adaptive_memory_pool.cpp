#include "tsdb/storage/semantic_vector/adaptive_memory_pool.h"
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
#include <list>

namespace tsdb {
namespace storage {
namespace semantic_vector {

// Namespace aliases for cleaner code
namespace core = ::tsdb::core;
using MemoryTier = core::semantic_vector::MemoryTier;
using MemoryPoolStats = core::semantic_vector::MemoryPoolStats;

// ============================================================================
// Minimal in-project adaptive memory pool implementations
// ============================================================================

namespace {

/**
 * @brief Memory block for efficient allocation
 */
struct MemoryBlock {
    void* ptr;
    size_t size;
    bool is_free;
    std::chrono::system_clock::time_point allocated_at;
    std::chrono::system_clock::time_point last_accessed;
    size_t access_count{0};
    
    MemoryBlock(void* p, size_t s) : ptr(p), size(s), is_free(true), 
                                     allocated_at(std::chrono::system_clock::now()),
                                     last_accessed(allocated_at) {}
};

/**
 * @brief Size class for efficient memory allocation
 */
struct SizeClass {
    size_t size;
    size_t alignment;
    std::list<MemoryBlock> free_blocks;
    std::list<MemoryBlock> allocated_blocks;
    size_t total_allocations{0};
    size_t total_deallocations{0};
    
    explicit SizeClass(size_t s, size_t a = sizeof(void*)) : size(s), alignment(a) {}
    
    double get_fragmentation_ratio() const {
        size_t total_allocated = allocated_blocks.size() * size;
        size_t total_free = free_blocks.size() * size;
        size_t total_memory = total_allocated + total_free;
        
        if (total_memory == 0) return 0.0;
        return static_cast<double>(total_free) / static_cast<double>(total_memory);
    }
};

/**
 * @brief Memory pool allocator with size classes
 */
class SizeClassAllocator {
public:
    explicit SizeClassAllocator(size_t pool_size_bytes) : pool_size_(pool_size_bytes), allocated_bytes_(0) {
        // Initialize common size classes (powers of 2)
        for (size_t size = 8; size <= 4096; size *= 2) {
            size_classes_.emplace_back(size);
        }
        
        // Add some specific sizes for common use cases
        size_classes_.emplace_back(24);   // Small objects
        size_classes_.emplace_back(48);   // Small objects
        size_classes_.emplace_back(96);   // Small objects
        size_classes_.emplace_back(192);  // Medium objects
        size_classes_.emplace_back(384);  // Medium objects
        
        // Sort by size for efficient lookup
        std::sort(size_classes_.begin(), size_classes_.end(), 
                  [](const SizeClass& a, const SizeClass& b) { return a.size < b.size; });
    }
    
    core::Result<void*> allocate(size_t size_bytes, size_t alignment = sizeof(void*)) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (allocated_bytes_ + size_bytes > pool_size_) {
            return core::Result<void*>::error("Pool capacity exceeded");
        }
        
        // Find appropriate size class
        auto& size_class = find_size_class(size_bytes);
        
        // Try to reuse from free list
        if (!size_class.free_blocks.empty()) {
            auto block_it = size_class.free_blocks.begin();
            void* ptr = block_it->ptr;
            
            // Move to allocated list
            size_class.allocated_blocks.splice(size_class.allocated_blocks.end(), 
                                               size_class.free_blocks, block_it);
            size_class.allocated_blocks.back().is_free = false;
            size_class.allocated_blocks.back().allocated_at = std::chrono::system_clock::now();
            size_class.allocated_blocks.back().last_accessed = size_class.allocated_blocks.back().allocated_at;
            size_class.allocated_blocks.back().access_count = 1;
            
            size_class.total_allocations++;
            allocated_bytes_ += size_class.size;
            
            return core::Result<void*>(ptr);
        }
        
        // Allocate new block
        void* ptr = std::aligned_alloc(alignment, size_class.size);
        if (!ptr) {
            return core::Result<void*>::error("Memory allocation failed");
        }
        
        size_class.allocated_blocks.emplace_back(ptr, size_class.size);
        size_class.allocated_blocks.back().is_free = false;
        size_class.allocated_blocks.back().access_count = 1;
        
        size_class.total_allocations++;
        allocated_bytes_ += size_class.size;
        
        return core::Result<void*>(ptr);
    }
    
    core::Result<void> deallocate(void* ptr) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Find the block in allocated lists
        for (auto& size_class : size_classes_) {
            auto block_it = std::find_if(size_class.allocated_blocks.begin(), 
                                         size_class.allocated_blocks.end(),
                                         [ptr](const MemoryBlock& block) { return block.ptr == ptr; });
            
            if (block_it != size_class.allocated_blocks.end()) {
                // Move to free list
                size_class.free_blocks.splice(size_class.free_blocks.end(), 
                                              size_class.allocated_blocks, block_it);
                size_class.free_blocks.back().is_free = true;
                
                size_class.total_deallocations++;
                allocated_bytes_ -= size_class.size;
                
                return core::Result<void>();
            }
        }
        
        return core::Result<void>::error("Pointer not found in pool");
    }
    
    core::Result<void> record_access(void* ptr) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Find the block and update access pattern
        for (auto& size_class : size_classes_) {
            auto block_it = std::find_if(size_class.allocated_blocks.begin(), 
                                         size_class.allocated_blocks.end(),
                                         [ptr](const MemoryBlock& block) { return block.ptr == ptr; });
            
            if (block_it != size_class.allocated_blocks.end()) {
                block_it->last_accessed = std::chrono::system_clock::now();
                block_it->access_count++;
                return core::Result<void>();
            }
        }
        
        return core::Result<void>::error("Pointer not found in pool");
    }
    
    core::Result<void> defragment() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Simple defragmentation: consolidate free blocks
        for (auto& size_class : size_classes_) {
            // Sort free blocks by address for potential consolidation
            size_class.free_blocks.sort([](const MemoryBlock& a, const MemoryBlock& b) {
                return a.ptr < b.ptr;
            });
            
            // Remove empty blocks from unused size classes
            if (size_class.allocated_blocks.empty() && size_class.free_blocks.size() > 1) {
                // Keep only one free block per unused size class
                while (size_class.free_blocks.size() > 1) {
                    auto& block = size_class.free_blocks.back();
                    std::free(block.ptr);
                    size_class.free_blocks.pop_back();
                }
            }
        }
        
        return core::Result<void>();
    }
    
    core::Result<void> compact() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Simple compaction: reduce free block count for underutilized size classes
        for (auto& size_class : size_classes_) {
            double fragmentation = size_class.get_fragmentation_ratio();
            
            // If fragmentation is high (>50%), reduce free blocks
            if (fragmentation > 0.5 && size_class.free_blocks.size() > 2) {
                size_t blocks_to_remove = size_class.free_blocks.size() / 2;
                
                for (size_t i = 0; i < blocks_to_remove; ++i) {
                    auto& block = size_class.free_blocks.back();
                    std::free(block.ptr);
                    size_class.free_blocks.pop_back();
                }
            }
        }
        
        return core::Result<void>();
    }
    
    size_t get_allocated_bytes() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return allocated_bytes_;
    }
    
    size_t get_free_bytes() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return pool_size_ - allocated_bytes_;
    }
    
    double get_utilization() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return pool_size_ > 0 ? static_cast<double>(allocated_bytes_) / static_cast<double>(pool_size_) : 0.0;
    }
    
    std::vector<std::pair<size_t, size_t>> get_size_class_stats() const {
        std::unique_lock<std::mutex> lock(mutex_);
        std::vector<std::pair<size_t, size_t>> stats;
        
        for (const auto& size_class : size_classes_) {
            stats.emplace_back(size_class.size, size_class.allocated_blocks.size());
        }
        
        return stats;
    }
    
private:
    SizeClass& find_size_class(size_t size_bytes) {
        // Find the smallest size class that can accommodate the request
        auto it = std::lower_bound(size_classes_.begin(), size_classes_.end(), size_bytes,
                                   [](const SizeClass& sc, size_t size) { return sc.size < size; });
        
        if (it == size_classes_.end()) {
            // Add a new size class for large allocations
            size_t new_size = 1;
            while (new_size < size_bytes) {
                new_size <<= 1; // Next power of 2
            }
            size_classes_.emplace_back(new_size);
            std::sort(size_classes_.begin(), size_classes_.end(),
                      [](const SizeClass& a, const SizeClass& b) { return a.size < b.size; });
            
            return find_size_class(size_bytes); // Recursive call with new size class
        }
        
        return *it;
    }
    
    size_t pool_size_;
    size_t allocated_bytes_;
    std::vector<SizeClass> size_classes_;
    mutable std::mutex mutex_;
};

/**
 * @brief Allocation pattern tracker for adaptive strategies
 */
class AllocationPatternTracker {
public:
    struct AllocationPattern {
        size_t size;
        size_t alignment;
        size_t frequency{0};
        std::chrono::system_clock::time_point last_request;
        double average_lifetime_ms{0.0};
        size_t total_allocations{0};
        size_t total_deallocations{0};
        
        double get_allocation_rate() const {
            auto now = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::hours>(now - last_request).count();
            return duration > 0 ? static_cast<double>(frequency) / static_cast<double>(duration) : 0.0;
        }
    };
    
    void record_allocation(size_t size, size_t alignment) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto key = std::make_pair(size, alignment);
        auto& pattern = patterns_[key];
        pattern.size = size;
        pattern.alignment = alignment;
        pattern.frequency++;
        pattern.last_request = std::chrono::system_clock::now();
        pattern.total_allocations++;
    }
    
    void record_deallocation(size_t size, size_t alignment, double lifetime_ms) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto key = std::make_pair(size, alignment);
        auto it = patterns_.find(key);
        if (it != patterns_.end()) {
            auto& pattern = it->second;
            pattern.total_deallocations++;
            
            // Update average lifetime using exponential moving average
            if (pattern.average_lifetime_ms == 0.0) {
                pattern.average_lifetime_ms = lifetime_ms;
            } else {
                pattern.average_lifetime_ms = 0.9 * pattern.average_lifetime_ms + 0.1 * lifetime_ms;
            }
        }
    }
    
    std::vector<AllocationPattern> get_hot_patterns(size_t top_k = 10) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<AllocationPattern> patterns;
        patterns.reserve(patterns_.size());
        
        for (const auto& [key, pattern] : patterns_) {
            patterns.push_back(pattern);
        }
        
        // Sort by frequency (descending)
        std::partial_sort(patterns.begin(), 
                         patterns.begin() + std::min(top_k, patterns.size()), 
                         patterns.end(),
                         [](const AllocationPattern& a, const AllocationPattern& b) {
                             return a.frequency > b.frequency;
                         });
        
        if (patterns.size() > top_k) {
            patterns.resize(top_k);
        }
        
        return patterns;
    }
    
    void reset_patterns() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        patterns_.clear();
    }
    
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::pair<size_t, size_t>, AllocationPattern, 
                       std::hash<std::pair<size_t, size_t>>> patterns_;
};

} // anonymous namespace

// ============================================================================
// ADAPTIVE MEMORY POOL IMPLEMENTATION
// ============================================================================

AdaptiveMemoryPoolImpl::AdaptiveMemoryPoolImpl(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config)
    : config_(config) {
    // Initialize adaptive memory pool structures
    (void)this->initialize_adaptive_pool_structures();
}

// ============================================================================
// MEMORY ALLOCATION OPERATIONS
// ============================================================================

core::Result<void*> AdaptiveMemoryPoolImpl::allocate(size_t size_bytes, size_t alignment) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Validate allocation request
    if (size_bytes == 0) {
        return core::Result<void*>::error("Invalid allocation size: cannot be zero");
    }
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        // Record allocation pattern
        if (pattern_tracker_) {
            pattern_tracker_->record_allocation(size_bytes, alignment);
        }
        
        // Perform allocation
        core::Result<void*> result;
        if (size_class_allocator_) {
            result = size_class_allocator_->allocate(size_bytes, alignment);
        } else {
            result = core::Result<void*>::error("Allocator not initialized");
        }
        
        if (result.ok()) {
            // Track allocation
            AllocationEntry entry;
            entry.ptr = result.value();
            entry.size = size_bytes;
            entry.alignment = alignment;
            entry.allocated_at = std::chrono::system_clock::now();
            entry.last_accessed = entry.allocated_at;
            allocations_[result.value()] = entry;
            
            // Update metrics
            const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocations.fetch_add(1);
            const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocated_bytes.fetch_add(size_bytes);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double latency = std::chrono::duration<double, std::milli>(end - start).count();
        (void)this->update_performance_metrics("allocate", latency, result.ok());
        
        return result;
    }
}

core::Result<void> AdaptiveMemoryPoolImpl::deallocate(void* ptr) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!ptr) {
        return core::Result<void>::error("Cannot deallocate null pointer");
    }
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        // Find allocation entry
        auto it = allocations_.find(ptr);
        if (it == allocations_.end()) {
            return core::Result<void>::error("Pointer not found in pool");
        }
        
        const auto& entry = it->second;
        
        // Record deallocation pattern
        if (pattern_tracker_) {
            auto now = std::chrono::system_clock::now();
            double lifetime_ms = std::chrono::duration<double, std::milli>(now - entry.allocated_at).count();
            pattern_tracker_->record_deallocation(entry.size, entry.alignment, lifetime_ms);
        }
        
        // Perform deallocation
        core::Result<void> result;
        if (size_class_allocator_) {
            result = size_class_allocator_->deallocate(ptr);
        } else {
            result = core::Result<void>::error("Allocator not initialized");
        }
        
        if (result.ok()) {
            // Update metrics
            const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_deallocations.fetch_add(1);
            const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocated_bytes.fetch_sub(entry.size);
            
            // Remove allocation entry
            allocations_.erase(it);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double latency = std::chrono::duration<double, std::milli>(end - start).count();
        (void)this->update_performance_metrics("deallocate", latency, result.ok());
        
        return result;
    }
}

core::Result<void*> AdaptiveMemoryPoolImpl::reallocate(void* ptr, size_t new_size_bytes) {
    if (!ptr) {
        return this->allocate(new_size_bytes);
    }
    
    if (new_size_bytes == 0) {
        auto dealloc_result = this->deallocate(ptr);
        if (!dealloc_result.ok()) {
            return core::Result<void*>::error(dealloc_result.error());
        }
        return core::Result<void*>(nullptr);
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // Find current allocation
    auto it = allocations_.find(ptr);
    if (it == allocations_.end()) {
        return core::Result<void*>::error("Pointer not found in pool");
    }
    
    const auto& entry = it->second;
    size_t old_size = entry.size;
    
    lock.unlock();
    
    // Allocate new memory
    auto new_ptr_result = this->allocate(new_size_bytes);
    if (!new_ptr_result.ok()) {
        return new_ptr_result;
    }
    
    void* new_ptr = new_ptr_result.value();
    
    // Copy data
    size_t copy_size = std::min(old_size, new_size_bytes);
    std::memcpy(new_ptr, ptr, copy_size);
    
    // Deallocate old memory
    auto dealloc_result = this->deallocate(ptr);
    if (!dealloc_result.ok()) {
        // Try to clean up new allocation
        (void)this->deallocate(new_ptr);
        return core::Result<void*>::error("Failed to deallocate old memory: " + dealloc_result.error());
    }
    
    return core::Result<void*>(new_ptr);
}

core::Result<void> AdaptiveMemoryPoolImpl::record_access(void* ptr) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // Update access pattern in allocator
    if (size_class_allocator_) {
        auto access_result = size_class_allocator_->record_access(ptr);
        if (!access_result.ok()) {
            return access_result;
        }
    }
    
    // Update allocation entry
    auto it = allocations_.find(ptr);
    if (it != allocations_.end()) {
        auto& entry = const_cast<AllocationEntry&>(it->second);
        entry.last_accessed = std::chrono::system_clock::now();
        entry.access_count++;
    }
    
    return core::Result<void>();
}

// ============================================================================
// MEMORY OPTIMIZATION OPERATIONS
// ============================================================================

core::Result<void> AdaptiveMemoryPoolImpl::defragment() {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    core::Result<void> result;
    if (size_class_allocator_) {
        result = size_class_allocator_->defragment();
        if (result.ok()) {
            const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_defragmentations.fetch_add(1);
        }
    } else {
        result = core::Result<void>::error("Allocator not initialized");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end - start).count();
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_defragmentation_time_ms.store(latency);
    
    return result;
}

core::Result<void> AdaptiveMemoryPoolImpl::compact() {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    core::Result<void> result;
    if (size_class_allocator_) {
        result = size_class_allocator_->compact();
        if (result.ok()) {
            const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_compactions.fetch_add(1);
        }
    } else {
        result = core::Result<void>::error("Allocator not initialized");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end - start).count();
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_compaction_time_ms.store(latency);
    
    return result;
}

core::Result<void> AdaptiveMemoryPoolImpl::optimize_allocation_strategy() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // Analyze allocation patterns and optimize size classes
    if (pattern_tracker_) {
        auto hot_patterns = pattern_tracker_->get_hot_patterns(10);
        
        // In a more sophisticated implementation, this would:
        // 1. Adjust size class distributions based on hot patterns
        // 2. Pre-allocate blocks for frequently requested sizes
        // 3. Adjust alignment strategies
        // 4. Tune defragmentation thresholds
        
        // For now, just record that optimization was performed
        const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_optimizations.fetch_add(1);
    }
    
    return core::Result<void>();
}

// ============================================================================
// MEMORY STATISTICS
// ============================================================================

core::Result<core::semantic_vector::MemoryPoolStats> AdaptiveMemoryPoolImpl::get_pool_stats() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    core::semantic_vector::MemoryPoolStats stats{};
    
    if (size_class_allocator_) {
        stats.total_capacity_bytes = config_.ram_tier_capacity_mb * 1024 * 1024; // Default pool size
        stats.allocated_bytes = size_class_allocator_->get_allocated_bytes();
        stats.free_bytes = size_class_allocator_->get_free_bytes();
        stats.utilization_ratio = size_class_allocator_->get_utilization();
        stats.allocation_count = allocations_.size();
        
        auto size_class_stats = size_class_allocator_->get_size_class_stats();
        stats.size_class_count = size_class_stats.size();
        
        // Calculate fragmentation (simplified)
        stats.fragmentation_ratio = stats.free_bytes > 0 ? 
            static_cast<double>(stats.free_bytes) / static_cast<double>(stats.total_capacity_bytes) : 0.0;
    }
    
    return core::Result<core::semantic_vector::MemoryPoolStats>(stats);
}

core::Result<double> AdaptiveMemoryPoolImpl::get_allocation_efficiency() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (size_class_allocator_) {
        double utilization = size_class_allocator_->get_utilization();
        return core::Result<double>(utilization);
    }
    
    return core::Result<double>(0.0);
}

core::Result<double> AdaptiveMemoryPoolImpl::get_fragmentation_ratio() const {
    auto stats_result = this->get_pool_stats();
    if (stats_result.ok()) {
        return core::Result<double>(stats_result.value().fragmentation_ratio);
    }
    
    return core::Result<double>(0.0);
}

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

core::Result<core::PerformanceMetrics> AdaptiveMemoryPoolImpl::get_performance_metrics() const {
    core::PerformanceMetrics m;
    m.total_memory_usage_bytes = this->performance_monitoring_.total_allocated_bytes.load();
    m.vector_memory_usage_bytes = this->performance_monitoring_.total_allocated_bytes.load();
    m.semantic_memory_usage_bytes = 0;
    m.temporal_memory_usage_bytes = 0;
    m.memory_compression_ratio = 1.0;
    m.average_vector_search_time_ms = this->performance_monitoring_.average_allocation_latency_ms.load();
    m.average_semantic_search_time_ms = 0.0;
    m.average_correlation_time_ms = 0.0;
    m.average_inference_time_ms = 0.0;
    m.vector_search_accuracy = this->performance_monitoring_.allocation_efficiency.load();
    m.semantic_search_accuracy = 1.0;
    m.correlation_accuracy = 1.0;
    m.inference_accuracy = 1.0;
    m.queries_per_second = this->performance_monitoring_.total_allocations.load();
    m.vectors_processed_per_second = this->performance_monitoring_.total_allocations.load();
    m.correlations_computed_per_second = 0;
    m.recorded_at = std::chrono::system_clock::now();
    return core::Result<core::PerformanceMetrics>(m);
}

core::Result<void> AdaptiveMemoryPoolImpl::reset_performance_metrics() {
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_allocation_latency_ms.store(0.0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_deallocation_latency_ms.store(0.0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocations.store(0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_deallocations.store(0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocated_bytes.store(0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).allocation_efficiency.store(1.0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_defragmentations.store(0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_compactions.store(0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_optimizations.store(0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_defragmentation_time_ms.store(0.0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_compaction_time_ms.store(0.0);
    const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).allocation_errors.store(0);
    return core::Result<void>();
}

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

void AdaptiveMemoryPoolImpl::update_config(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    this->config_ = config;
}

core::semantic_vector::SemanticVectorConfig::MemoryConfig AdaptiveMemoryPoolImpl::get_config() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return this->config_;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

core::Result<void> AdaptiveMemoryPoolImpl::initialize_adaptive_pool_structures() {
    try {
        // Initialize size class allocator with default pool size
        size_t pool_size = config_.ram_tier_capacity_mb * 1024 * 1024; // Convert MB to bytes
        if (pool_size == 0) {
            pool_size = 1024 * 1024 * 1024; // Default 1GB
        }
        
        size_class_allocator_ = std::make_unique<SizeClassAllocator>(pool_size);
        pattern_tracker_ = std::make_unique<AllocationPatternTracker>();
        
    } catch (const std::exception& e) {
        return core::Result<void>::error("Failed to initialize adaptive pool structures: " + std::string(e.what()));
    }
    
    return core::Result<void>();
}

core::Result<void> AdaptiveMemoryPoolImpl::update_performance_metrics(const std::string& operation, double latency, bool success) const {
    (void)success;
    
    if (operation == "allocate") {
        // Update average allocation latency using exponential moving average
        size_t n = const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_allocations.load();
        double prev = const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_allocation_latency_ms.load();
        double updated = n > 0 ? prev + (latency - prev) / static_cast<double>(n) : latency;
        const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_allocation_latency_ms.store(updated);
    } else if (operation == "deallocate") {
        // Update average deallocation latency
        size_t n = const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).total_deallocations.load();
        double prev = const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_deallocation_latency_ms.load();
        double updated = n > 0 ? prev + (latency - prev) / static_cast<double>(n) : latency;
        const_cast<AdaptiveMemoryPoolImpl::PerformanceMonitoring&>(this->performance_monitoring_).average_deallocation_latency_ms.store(updated);
    }
    
    return core::Result<void>();
}

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

std::unique_ptr<AdaptiveMemoryPoolImpl> CreateAdaptiveMemoryPool(
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) {
    return std::unique_ptr<AdaptiveMemoryPoolImpl>(new AdaptiveMemoryPoolImpl(config));
}

std::unique_ptr<AdaptiveMemoryPoolImpl> CreateAdaptiveMemoryPoolForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& base_config) {
    
    auto config = base_config;
    
    if (use_case == "high_performance") {
        config.ram_tier_capacity_mb = 2048;  // 2GB pool
        config.enable_memory_monitoring = true;
    } else if (use_case == "memory_efficient") {
        config.ram_tier_capacity_mb = 512;   // 512MB pool
        config.enable_memory_monitoring = true;
        config.enable_delta_compression = true;
        config.enable_dictionary_compression = true;
    } else if (use_case == "high_accuracy") {
        config.ram_tier_capacity_mb = 4096;  // 4GB pool
        config.enable_memory_monitoring = true;
    }
    
    return std::unique_ptr<AdaptiveMemoryPoolImpl>(new AdaptiveMemoryPoolImpl(config));
}

core::Result<core::semantic_vector::ConfigValidationResult> ValidateAdaptiveMemoryPoolConfig(
    const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) {
    
    core::semantic_vector::ConfigValidationResult res{};
    res.is_valid = true;
    
    if (config.ram_tier_capacity_mb == 0) {
        res.warnings.push_back("ram_tier_capacity_mb is 0; pool may use default size");
    }
    if (config.target_memory_reduction < 0.0 || config.target_memory_reduction > 1.0) {
        res.is_valid = false;
        res.errors.push_back("target_memory_reduction must be between 0.0 and 1.0");
    }
    if (config.max_latency_impact < 0.0 || config.max_latency_impact > 1.0) {
        res.is_valid = false;
        res.errors.push_back("max_latency_impact must be between 0.0 and 1.0");
    }
    
    return core::Result<core::semantic_vector::ConfigValidationResult>(res);
}

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb
