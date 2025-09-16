#pragma once

#include "tsdb/core/types.h"
#include <chrono>
#include <string>

namespace tsdb {
namespace storage {

/**
 * @brief Configuration for the hierarchical cache system
 */
struct CacheHierarchyConfig {
    // L1 Cache (Memory) - Fastest, smallest
    size_t l1_max_size = 1000;           // Maximum number of entries in L1
    size_t l1_max_memory_mb = 100;       // Maximum memory usage in MB
    
    // L2 Cache (Memory-mapped) - Medium speed, medium size
    size_t l2_max_size = 10000;          // Maximum number of entries in L2
    size_t l2_max_memory_mb = 1000;      // Maximum memory usage in MB
    std::string l2_storage_path = "./cache/l2";  // Path for L2 storage
    
    // L3 Cache (Disk) - Slowest, largest
    size_t l3_max_size = 100000;         // Maximum number of entries in L3
    size_t l3_max_disk_gb = 10;          // Maximum disk usage in GB
    std::string l3_storage_path = "./cache/l3";  // Path for L3 storage
    
    // Promotion/Demotion policies
    uint64_t l1_promotion_threshold = 5;     // Access count to promote to L1
    uint64_t l2_promotion_threshold = 2;     // Access count to promote to L2
    uint64_t l1_demotion_threshold = 2;      // Access count threshold for L1 demotion
    uint64_t l2_demotion_threshold = 1;      // Access count threshold for L2 demotion  
    std::chrono::seconds l1_demotion_timeout{300};  // Time before demotion from L1
    std::chrono::seconds l2_demotion_timeout{3600}; // Time before demotion from L2
    uint64_t l1_demotion_timeout_seconds = 30;      // Timeout in seconds for L1 demotion
    uint64_t l2_demotion_timeout_seconds = 60;      // Timeout in seconds for L2 demotion
    
    // Background processing
    bool enable_background_processing = true;
    size_t background_threads = 2;
    std::chrono::milliseconds background_interval{1000}; // Background task interval
    
    // Performance monitoring
    bool enable_detailed_metrics = true;
    bool enable_cache_warming = false;
};

/**
 * @brief Cache entry metadata for tracking access patterns
 */
struct CacheEntryMetadata {
    core::SeriesID series_id;
    std::chrono::steady_clock::time_point last_access;
    std::chrono::steady_clock::time_point created_at;
    uint64_t access_count{0};
    uint64_t size_bytes{0};
    bool is_dirty{false};
    
    // Default constructor for unordered_map compatibility
    CacheEntryMetadata() 
        : series_id(0), 
          last_access(std::chrono::steady_clock::now()),
          created_at(std::chrono::steady_clock::now()) {}
    
    CacheEntryMetadata(core::SeriesID id) 
        : series_id(id), 
          last_access(std::chrono::steady_clock::now()),
          created_at(std::chrono::steady_clock::now()) {}
    
    void record_access() {
        last_access = std::chrono::steady_clock::now();
        access_count++;
    }
    
    bool should_promote_to_l1(const CacheHierarchyConfig& config) const {
        return access_count >= config.l1_promotion_threshold;
    }
    
    bool should_promote_to_l2(const CacheHierarchyConfig& config) const {
        return access_count >= config.l2_promotion_threshold;
    }
    
    bool should_demote_from_l1(const CacheHierarchyConfig& config) const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_access) > config.l1_demotion_timeout;
    }
    
    bool should_demote_from_l2(const CacheHierarchyConfig& config) const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_access) > config.l2_demotion_timeout;
    }
};

} // namespace storage
} // namespace tsdb 