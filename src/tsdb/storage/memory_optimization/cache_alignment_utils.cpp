#include "cache_alignment_utils.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace tsdb {
namespace storage {

CacheAlignmentUtils::CacheAlignmentUtils(const core::StorageConfig& config)
    : config_(config)
    , total_prefetches_(0)
    , successful_prefetches_(0)
    , failed_prefetches_(0) {
    
    cache_info_.aligned_allocations = 0;
    cache_info_.alignment_operations = 0;
    cache_info_.prefetch_operations = 0;
    cache_info_.promotion_operations = 0;
    cache_info_.demotion_operations = 0;
}

CacheAlignmentUtils::~CacheAlignmentUtils() {
    // Cleanup will be handled automatically
}

core::Result<void> CacheAlignmentUtils::initialize() {
    try {
        // Initialize cache alignment tracking
        cache_info_.aligned_allocations = 0;
        cache_info_.alignment_operations = 0;
        cache_info_.prefetch_operations = 0;
        cache_info_.promotion_operations = 0;
        cache_info_.demotion_operations = 0;
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Failed to initialize cache alignment utils: " + std::string(e.what()));
    }
}

void* CacheAlignmentUtils::align_to_cache_line(void* ptr, size_t alignment) {
    if (!ptr) {
        return nullptr;
    }
    
    cache_info_.alignment_operations.fetch_add(1);
    
    return calculate_aligned_pointer(ptr, alignment);
}

core::Result<void*> CacheAlignmentUtils::allocate_aligned(size_t size, size_t alignment) {
    try {
        // Allocate memory with extra space for alignment
        size_t total_size = size + alignment - 1;
        void* raw_ptr = std::aligned_alloc(alignment, total_size);
        
        if (!raw_ptr) {
            return core::Result<void*>::error("Failed to allocate aligned memory");
        }
        
        // Align the pointer
        void* aligned_ptr = calculate_aligned_pointer(raw_ptr, alignment);
        
        // Track the allocation
        track_memory_allocation(aligned_ptr, size, alignment);
        
        cache_info_.aligned_allocations.fetch_add(1);
        
        return core::Result<void*>(aligned_ptr);
    } catch (const std::exception& e) {
        return core::Result<void*>::error("Aligned allocation exception: " + std::string(e.what()));
    }
}

core::Result<void> CacheAlignmentUtils::deallocate_aligned(void* ptr) {
    if (!ptr) {
        return core::Result<void>::error("Invalid pointer for deallocation");
    }
    
    try {
        // Untrack the allocation
        untrack_memory_allocation(ptr);
        
        // Deallocate memory
        std::free(ptr);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Aligned deallocation exception: " + std::string(e.what()));
    }
}

core::Result<void> CacheAlignmentUtils::optimize_data_layout(std::vector<void*>& data) {
    try {
        // Sort data by access frequency for better cache performance
        std::sort(data.begin(), data.end(), [this](void* a, void* b) {
            std::lock_guard<std::mutex> lock(memory_mutex_);
            
            auto it_a = memory_tracking_.find(a);
            auto it_b = memory_tracking_.find(b);
            
            if (it_a != memory_tracking_.end() && it_b != memory_tracking_.end()) {
                return it_a->second.access_count.load() > it_b->second.access_count.load();
            }
            
            return false;
        });
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Data layout optimization exception: " + std::string(e.what()));
    }
}

core::Result<void> CacheAlignmentUtils::prefetch_data(void* ptr, size_t size) {
    if (!ptr) {
        return core::Result<void>::error("Invalid pointer for prefetch");
    }
    
    try {
        // Simplified prefetch implementation
        // In a real implementation, this would use actual prefetch instructions
        
        // Update access statistics
        update_access_statistics(ptr);
        
        // Simulate prefetch operation
        volatile char* data = static_cast<char*>(ptr);
        volatile char dummy = data[0];
        (void)dummy; // Suppress unused variable warning
        
        total_prefetches_.fetch_add(1);
        successful_prefetches_.fetch_add(1);
        cache_info_.prefetch_operations.fetch_add(1);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        failed_prefetches_.fetch_add(1);
        return core::Result<void>::error("Data prefetch exception: " + std::string(e.what()));
    }
}

core::Result<void> CacheAlignmentUtils::promote_hot_data(void* ptr) {
    if (!ptr) {
        return core::Result<void>::error("Invalid pointer for promotion");
    }
    
    try {
        // Update access statistics
        update_access_statistics(ptr);
        
        // Mark as hot data
        std::lock_guard<std::mutex> lock(memory_mutex_);
        auto it = memory_tracking_.find(ptr);
        if (it != memory_tracking_.end()) {
            it->second.is_hot.store(true);
        }
        
        cache_info_.promotion_operations.fetch_add(1);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Hot data promotion exception: " + std::string(e.what()));
    }
}

core::Result<void> CacheAlignmentUtils::demote_cold_data(void* ptr) {
    if (!ptr) {
        return core::Result<void>::error("Invalid pointer for demotion");
    }
    
    try {
        // Update access statistics
        update_access_statistics(ptr);
        
        // Mark as cold data
        std::lock_guard<std::mutex> lock(memory_mutex_);
        auto it = memory_tracking_.find(ptr);
        if (it != memory_tracking_.end()) {
            it->second.is_hot.store(false);
        }
        
        cache_info_.demotion_operations.fetch_add(1);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Cold data demotion exception: " + std::string(e.what()));
    }
}

std::string CacheAlignmentUtils::get_cache_stats() const {
    std::ostringstream oss;
    oss << "Cache Alignment Statistics:\n"
        << "  Aligned Allocations: " << cache_info_.aligned_allocations.load() << "\n"
        << "  Alignment Operations: " << cache_info_.alignment_operations.load() << "\n"
        << "  Prefetch Operations: " << cache_info_.prefetch_operations.load() << "\n"
        << "  Promotion Operations: " << cache_info_.promotion_operations.load() << "\n"
        << "  Demotion Operations: " << cache_info_.demotion_operations.load();
    
    return oss.str();
}

std::string CacheAlignmentUtils::get_memory_stats() const {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    
    size_t total_allocations = memory_tracking_.size();
    size_t hot_allocations = 0;
    size_t total_accesses = 0;
    
    for (const auto& [ptr, info] : memory_tracking_) {
        if (info.is_hot.load()) {
            hot_allocations++;
        }
        total_accesses += info.access_count.load();
    }
    
    std::ostringstream oss;
    oss << "Memory Usage Statistics:\n"
        << "  Total Allocations: " << total_allocations << "\n"
        << "  Hot Allocations: " << hot_allocations << "\n"
        << "  Total Accesses: " << total_accesses << "\n"
        << "  Average Accesses per Allocation: " 
        << (total_allocations > 0 ? static_cast<double>(total_accesses) / total_allocations : 0.0);
    
    return oss.str();
}

std::string CacheAlignmentUtils::get_prefetch_stats() const {
    double success_rate = total_prefetches_.load() > 0 ? 
        (static_cast<double>(successful_prefetches_.load()) / total_prefetches_.load()) * 100.0 : 0.0;
    
    std::ostringstream oss;
    oss << "Prefetch Statistics:\n"
        << "  Total Prefetches: " << total_prefetches_.load() << "\n"
        << "  Successful Prefetches: " << successful_prefetches_.load() << "\n"
        << "  Failed Prefetches: " << failed_prefetches_.load() << "\n"
        << "  Success Rate: " << std::fixed << std::setprecision(2) << success_rate << "%";
    
    return oss.str();
}

void CacheAlignmentUtils::track_memory_allocation(void* ptr, size_t size, size_t alignment) {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    
    MemoryInfo info;
    info.ptr = ptr;
    info.size = size;
    info.alignment = alignment;
    info.allocated_at = std::chrono::system_clock::now();
    info.access_count.store(0);
    info.is_hot.store(false);
    memory_tracking_[ptr] = std::move(info);
}

void CacheAlignmentUtils::untrack_memory_allocation(void* ptr) {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    memory_tracking_.erase(ptr);
}

void CacheAlignmentUtils::update_access_statistics(void* ptr) {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    
    auto it = memory_tracking_.find(ptr);
    if (it != memory_tracking_.end()) {
        it->second.access_count.fetch_add(1);
    }
}

std::vector<void*> CacheAlignmentUtils::identify_hot_data() {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    std::vector<void*> hot_data;
    
    for (const auto& [ptr, info] : memory_tracking_) {
        if (info.is_hot.load() || info.access_count.load() > 10) {
            hot_data.push_back(ptr);
        }
    }
    
    return hot_data;
}

std::vector<void*> CacheAlignmentUtils::identify_cold_data() {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    std::vector<void*> cold_data;
    
    for (const auto& [ptr, info] : memory_tracking_) {
        if (!info.is_hot.load() && info.access_count.load() < 5) {
            cold_data.push_back(ptr);
        }
    }
    
    return cold_data;
}

void* CacheAlignmentUtils::calculate_aligned_pointer(void* ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned_addr);
}

bool CacheAlignmentUtils::is_cache_aligned(void* ptr, size_t alignment) {
    if (!ptr) {
        return false;
    }
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return (addr % alignment) == 0;
}

} // namespace storage
} // namespace tsdb
