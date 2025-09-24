#include "adaptive_memory_integration_new.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace tsdb {
namespace storage {

AdaptiveMemoryIntegration::AdaptiveMemoryIntegration(const core::StorageConfig& config)
    : config_(config)
    , total_allocations_(0)
    , total_deallocations_(0)
    , total_size_allocated_(0)
    , access_count_(0) {
}

AdaptiveMemoryIntegration::~AdaptiveMemoryIntegration() {
    // Clean up any remaining allocations
    std::lock_guard<std::mutex> lock(memory_mutex_);
    for (void* ptr : allocated_blocks_) {
        if (ptr) {
            std::free(ptr);
        }
    }
    allocated_blocks_.clear();
    hot_blocks_.clear();
}

core::Result<void> AdaptiveMemoryIntegration::initialize() {
    try {
        // Initialize simple memory management
        std::lock_guard<std::mutex> lock(memory_mutex_);
        allocated_blocks_.clear();
        hot_blocks_.clear();
        total_allocations_ = 0;
        total_deallocations_ = 0;
        total_size_allocated_ = 0;
        access_count_ = 0;
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Failed to initialize adaptive memory integration: " + std::string(e.what()));
    }
}

core::Result<void*> AdaptiveMemoryIntegration::allocate_optimized(size_t size, size_t alignment) {
    try {
        // Simple memory allocation with alignment
        void* ptr = std::aligned_alloc(alignment, size);
        if (!ptr) {
            return core::Result<void*>::error("Memory allocation failed");
        }
        
        // Track allocation
        {
            std::lock_guard<std::mutex> lock(memory_mutex_);
            allocated_blocks_.push_back(ptr);
            total_allocations_++;
            total_size_allocated_ += size;
        }
        
        return core::Result<void*>(ptr);
    } catch (const std::exception& e) {
        return core::Result<void*>::error("Allocation exception: " + std::string(e.what()));
    }
}

core::Result<void> AdaptiveMemoryIntegration::deallocate_optimized(void* ptr) {
    if (!ptr) {
        return core::Result<void>::error("Invalid pointer for deallocation");
    }
    
    try {
        // Check if pointer is valid
        if (!is_valid_pointer(ptr)) {
            return core::Result<void>::error("Pointer not found in allocated blocks");
        }
        
        // Deallocate memory
        std::free(ptr);
        
        // Remove from tracking
        {
            std::lock_guard<std::mutex> lock(memory_mutex_);
            auto it = std::find(allocated_blocks_.begin(), allocated_blocks_.end(), ptr);
            if (it != allocated_blocks_.end()) {
                allocated_blocks_.erase(it);
            }
            hot_blocks_.erase(ptr);
            total_deallocations_++;
        }
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Deallocation exception: " + std::string(e.what()));
    }
}

core::Result<void> AdaptiveMemoryIntegration::record_access_pattern(void* ptr) {
    if (!ptr) {
        return core::Result<void>::error("Invalid pointer for access pattern recording");
    }
    
    try {
        // Check if pointer is valid
        if (!is_valid_pointer(ptr)) {
            return core::Result<void>::error("Pointer not found in allocated blocks");
        }
        
        // Update access statistics
        update_access_stats(ptr);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Access pattern recording exception: " + std::string(e.what()));
    }
}

std::string AdaptiveMemoryIntegration::get_memory_stats() const {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    
    std::ostringstream oss;
    oss << "Memory Statistics:\n";
    oss << "  Total Allocations: " << total_allocations_ << "\n";
    oss << "  Total Deallocations: " << total_deallocations_ << "\n";
    oss << "  Active Allocations: " << allocated_blocks_.size() << "\n";
    oss << "  Total Size Allocated: " << total_size_allocated_ << " bytes\n";
    oss << "  Hot Blocks: " << hot_blocks_.size() << "\n";
    
    return oss.str();
}

std::string AdaptiveMemoryIntegration::get_access_pattern_stats() const {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    
    std::ostringstream oss;
    oss << "Access Pattern Statistics:\n";
    oss << "  Total Access Count: " << access_count_ << "\n";
    oss << "  Hot Blocks: " << hot_blocks_.size() << "\n";
    oss << "  Active Blocks: " << allocated_blocks_.size() << "\n";
    
    return oss.str();
}

bool AdaptiveMemoryIntegration::is_valid_pointer(void* ptr) const {
    return std::find(allocated_blocks_.begin(), allocated_blocks_.end(), ptr) != allocated_blocks_.end();
}

void AdaptiveMemoryIntegration::update_access_stats(void* ptr) {
    access_count_++;
    
    // Simple hot data detection: if accessed more than 5 times, mark as hot
    // This is a simplified approach for Phase 1
    if (access_count_ % 5 == 0) {
        hot_blocks_.insert(ptr);
    }
}

} // namespace storage
} // namespace tsdb
