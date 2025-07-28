#include "tsdb/storage/cache_hierarchy.h"
#include "tsdb/storage/memory_mapped_cache.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <chrono>

namespace tsdb {
namespace storage {

CacheHierarchy::CacheHierarchy(const CacheHierarchyConfig& config)
    : config_(config) {
    // Initialize L1 cache (WorkingSetCache)
    l1_cache_ = std::make_unique<WorkingSetCache>(config_.l1_max_size);
    
    // Initialize L2 cache (MemoryMappedCache) only if L2 is enabled
    if (config_.l2_max_size > 0) {
        l2_cache_ = std::make_unique<MemoryMappedCache>(config_);
        
        // Create storage directories
        try {
            std::filesystem::create_directories(config_.l2_storage_path);
            std::filesystem::create_directories(config_.l3_storage_path);
        } catch (const std::exception& e) {
            // If directory creation fails, we'll continue without L2 cache
            l2_cache_.reset();
        }
    }
    
    // Start background processing if enabled
    if (config_.enable_background_processing) {
        start_background_processing();
    }
}

CacheHierarchy::~CacheHierarchy() {
    stop_background_processing();
}

std::shared_ptr<core::TimeSeries> CacheHierarchy::get(core::SeriesID series_id) {
    // Try L1 cache first (fastest)
    auto result = l1_cache_->get(series_id);
    if (result) {
        l1_hits_.fetch_add(1, std::memory_order_relaxed);
        total_hits_.fetch_add(1, std::memory_order_relaxed);
        update_access_metadata(series_id, 1);
        return result;
    }
    
    // Try L2 cache (medium speed) if available
    if (l2_cache_) {
        result = l2_cache_->get(series_id);
        if (result) {
            l2_hits_.fetch_add(1, std::memory_order_relaxed);
            total_hits_.fetch_add(1, std::memory_order_relaxed);
            update_access_metadata(series_id, 2);
            
            // Consider promoting to L1
            if (should_promote(series_id)) {
                promote(series_id, 1);
            }
            
            return result;
        }
    }
    
    // L3 cache (disk) - this would be handled by the storage system
    // For now, we'll just record a miss
    total_misses_.fetch_add(1, std::memory_order_relaxed);
    update_access_metadata(series_id, 3);
    
    return nullptr;
}

bool CacheHierarchy::put(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series) {
    if (!series) {
        throw core::InvalidArgumentError("Cannot cache null time series");
    }
    
    // Try to put in L1 first
    if (!l1_cache_->is_full()) {
        l1_cache_->put(series_id, series);
        update_access_metadata(series_id, 1);
        return true;
    }
    
    // L1 is full, try L2 if available
    if (l2_cache_ && !l2_cache_->is_full()) {
        l2_cache_->put(series_id, series);
        update_access_metadata(series_id, 2);
        return true;
    }
    
    // Both L1 and L2 are full, implement eviction logic
    // Evict least recently used from L1 and move to L2 if available
    auto [lru_id, lru_series] = l1_cache_->evict_lru_and_get_with_id();
    if (lru_id != 0 && lru_series) {
        // Try to put the evicted series in L2 if available
        if (l2_cache_ && !l2_cache_->is_full()) {
            l2_cache_->put(lru_id, lru_series);
            update_access_metadata(lru_id, 2);
        }
        // If L2 is not available or full, the evicted series is lost (would go to L3 in real implementation)
        
        // Now put the new series in L1
        l1_cache_->put(series_id, series);
        update_access_metadata(series_id, 1);
        return true;
    }
    
    // If we can't evict from L1, try to evict from L2 if available
    if (l2_cache_) {
        auto l2_series_ids = l2_cache_->get_all_series_ids();
        if (!l2_series_ids.empty()) {
            // Get the least recently used series from L2
            core::SeriesID lru_id = l2_series_ids[0]; // Simple approach - take first one
            l2_cache_->remove(lru_id);
            
            // Put the new series in L2
            l2_cache_->put(series_id, series);
            update_access_metadata(series_id, 2);
            return true;
        }
    }
    
    // If both caches are empty (shouldn't happen), just return false
    return false;
}

bool CacheHierarchy::remove(core::SeriesID series_id) {
    bool removed = false;
    
    // Remove from L1
    if (l1_cache_->remove(series_id)) {
        removed = true;
    }
    
    // Remove from L2 if available
    if (l2_cache_ && l2_cache_->remove(series_id)) {
        removed = true;
    }
    
    // L3 removal would be handled by the storage system
    
    return removed;
}

bool CacheHierarchy::promote(core::SeriesID series_id, int target_level) {
    if (target_level < 1 || target_level > 3) {
        return false;
    }
    
    std::shared_ptr<core::TimeSeries> series;
    
    // Get the series from its current location
    if (target_level == 1) {
        // Promote to L1 from L2 if available
        if (l2_cache_) {
            series = l2_cache_->get(series_id);
            if (series) {
                if (!l1_cache_->is_full()) {
                    l1_cache_->put(series_id, series);
                    l2_cache_->remove(series_id);
                    promotions_.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            }
        }
    } else if (target_level == 2) {
        // Promote to L2 from L3 (would be handled by storage system)
        // For now, we'll just return false
        return false;
    }
    
    return false;
}

bool CacheHierarchy::demote(core::SeriesID series_id, int target_level) {
    if (target_level < 1 || target_level > 3) {
        return false;
    }
    
    std::shared_ptr<core::TimeSeries> series;
    
    if (target_level == 2) {
        // Demote from L1 to L2 if available
        if (l2_cache_) {
            series = l1_cache_->get(series_id);
            if (series) {
                if (!l2_cache_->is_full()) {
                    l2_cache_->put(series_id, series);
                    l1_cache_->remove(series_id);
                    demotions_.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            }
        }
    } else if (target_level == 3) {
        // Demote from L2 to L3 (would be handled by storage system)
        // For now, we'll just return false
        return false;
    }
    
    return false;
}

void CacheHierarchy::clear() {
    l1_cache_->clear();
    if (l2_cache_) {
        l2_cache_->clear();
    }
    reset_stats();
}

std::string CacheHierarchy::stats() const {
    std::ostringstream oss;
    oss << "Cache Hierarchy Stats:\n";
    oss << "==========================================\n";
    
    // Overall statistics
    uint64_t total_requests = total_hits_.load(std::memory_order_relaxed) + 
                             total_misses_.load(std::memory_order_relaxed);
    
    oss << "Overall Statistics:\n";
    oss << "  Total requests: " << total_requests << "\n";
    oss << "  Total hits: " << total_hits_.load(std::memory_order_relaxed) << "\n";
    oss << "  Total misses: " << total_misses_.load(std::memory_order_relaxed) << "\n";
    
    if (total_requests > 0) {
        double overall_hit_ratio = static_cast<double>(total_hits_.load(std::memory_order_relaxed)) / 
                                  total_requests * 100.0;
        oss << "  Overall hit ratio: " << std::fixed << std::setprecision(2) << overall_hit_ratio << "%\n";
    }
    
    oss << "  Promotions: " << promotions_.load(std::memory_order_relaxed) << "\n";
    oss << "  Demotions: " << demotions_.load(std::memory_order_relaxed) << "\n";
    
    // L1 cache statistics
    oss << "\nL1 Cache (Memory):\n";
    oss << "  " << l1_cache_->stats();
    oss << "  Hits: " << l1_hits_.load(std::memory_order_relaxed) << "\n";
    
    // L2 cache statistics
    oss << "\nL2 Cache (Memory-mapped):\n";
    if (l2_cache_) {
        oss << "  " << l2_cache_->stats();
    } else {
        oss << "  Status: Disabled\n";
    }
    oss << "  Hits: " << l2_hits_.load(std::memory_order_relaxed) << "\n";
    
    // L3 cache statistics
    oss << "\nL3 Cache (Disk):\n";
    oss << "  Hits: " << l3_hits_.load(std::memory_order_relaxed) << "\n";
    oss << "  Storage path: " << config_.l3_storage_path << "\n";
    
    // Background processing status
    oss << "\nBackground Processing:\n";
    oss << "  Status: " << (is_background_processing_running() ? "Running" : "Stopped") << "\n";
    oss << "  Enabled: " << (config_.enable_background_processing ? "Yes" : "No") << "\n";
    
    return oss.str();
}

double CacheHierarchy::hit_ratio() const {
    uint64_t hits = total_hits_.load(std::memory_order_relaxed);
    uint64_t misses = total_misses_.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;
    
    if (total == 0) {
        return 0.0;
    }
    
    return static_cast<double>(hits) / total * 100.0;
}

void CacheHierarchy::reset_stats() {
    total_hits_.store(0, std::memory_order_relaxed);
    total_misses_.store(0, std::memory_order_relaxed);
    l1_hits_.store(0, std::memory_order_relaxed);
    l2_hits_.store(0, std::memory_order_relaxed);
    l3_hits_.store(0, std::memory_order_relaxed);
    promotions_.store(0, std::memory_order_relaxed);
    demotions_.store(0, std::memory_order_relaxed);
    
    l1_cache_->reset_stats();
    if (l2_cache_) {
        l2_cache_->reset_stats();
    }
}

void CacheHierarchy::start_background_processing() {
    if (background_running_.load(std::memory_order_relaxed)) {
        return; // Already running
    }
    
    background_running_.store(true, std::memory_order_relaxed);
    background_thread_ = std::thread(&CacheHierarchy::background_processing_loop, this);
}

void CacheHierarchy::stop_background_processing() {
    if (!background_running_.load(std::memory_order_relaxed)) {
        return; // Not running
    }
    
    background_running_.store(false, std::memory_order_relaxed);
    
    if (background_thread_.joinable()) {
        background_thread_.join();
    }
}

bool CacheHierarchy::is_background_processing_running() const {
    return background_running_.load(std::memory_order_relaxed);
}

void CacheHierarchy::background_processing_loop() {
    while (background_running_.load(std::memory_order_relaxed)) {
        perform_maintenance();
        
        // Sleep for the configured interval
        std::this_thread::sleep_for(config_.background_interval);
    }
}

void CacheHierarchy::perform_maintenance() {
    // Get all series IDs from L1 and L2
    std::vector<core::SeriesID> l1_series_ids = l1_cache_->get_all_series_ids();
    std::vector<core::SeriesID> l2_series_ids;
    if (l2_cache_) {
        l2_series_ids = l2_cache_->get_all_series_ids();
    }
    
    // Check for promotions from L2 to L1
    for (const auto& series_id : l2_series_ids) {
        if (should_promote(series_id)) {
            promote(series_id, 1);
        }
    }
    
    // Check for demotions from L1 to L2
    for (const auto& series_id : l1_series_ids) {
        if (should_demote(series_id)) {
            demote(series_id, 2);
        }
    }
    
    // Check for demotions from L2 to L3
    for (const auto& series_id : l2_series_ids) {
        if (should_demote(series_id)) {
            demote(series_id, 3);
        }
    }
}

void CacheHierarchy::update_access_metadata(core::SeriesID series_id, int cache_level) {
    // This would update metadata for tracking access patterns
    // For now, we'll leave this as a placeholder
    (void)series_id;
    (void)cache_level;
}

bool CacheHierarchy::should_promote(core::SeriesID series_id) const {
    // Check L2 metadata for promotion criteria
    const auto* metadata = l2_cache_->get_metadata(series_id);
    if (metadata) {
        return metadata->should_promote_to_l1(config_);
    }
    return false;
}

bool CacheHierarchy::should_demote(core::SeriesID series_id) const {
    // Check L1 metadata for demotion criteria
    // For now, we'll use a simple timeout-based approach
    // In a real implementation, we'd need to track metadata for L1 as well
    (void)series_id;
    return false;
}

size_t CacheHierarchy::calculate_series_size(const std::shared_ptr<core::TimeSeries>& series) const {
    if (!series) {
        return 0;
    }
    
    size_t size = 0;
    
    // Size of labels (simplified calculation)
    const auto& labels = series->labels();
    size += labels.size() * 32; // Approximate size per label
    
    // Size of samples
    const auto& samples = series->samples();
    size += samples.size() * sizeof(core::Sample);
    
    // Overhead for shared_ptr and other metadata
    size += sizeof(std::shared_ptr<core::TimeSeries>);
    
    return size;
}

} // namespace storage
} // namespace tsdb 