#include "tiered_memory_integration.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace tsdb {
namespace storage {

// SimpleTieredMemoryManager implementation
core::Result<void> SimpleTieredMemoryManager::add_series(const core::SeriesID& series_id, MemoryTier tier) {
    std::lock_guard<std::mutex> lock(tier_mutex_);
    series_tiers_[series_id] = tier;
    return core::Result<void>();
}

core::Result<void> SimpleTieredMemoryManager::migrate_series(const core::SeriesID& series_id, MemoryTier target_tier) {
    std::lock_guard<std::mutex> lock(tier_mutex_);
    auto it = series_tiers_.find(series_id);
    if (it != series_tiers_.end()) {
        it->second = target_tier;
        return core::Result<void>();
    }
    return core::Result<void>::error("Series not found");
}

core::Result<MemoryTier> SimpleTieredMemoryManager::get_series_tier(const core::SeriesID& series_id) {
    std::lock_guard<std::mutex> lock(tier_mutex_);
    auto it = series_tiers_.find(series_id);
    if (it != series_tiers_.end()) {
        return core::Result<MemoryTier>(it->second);
    }
    return core::Result<MemoryTier>::error("Series not found");
}

TieredMemoryIntegration::TieredMemoryIntegration(const core::StorageConfig& config)
    : config_(config)
    , total_migrations_(0)
    , successful_migrations_(0)
    , failed_migrations_(0)
    , ram_series_count_(0)
    , ssd_series_count_(0)
    , hdd_series_count_(0) {
}

TieredMemoryIntegration::~TieredMemoryIntegration() {
    // Cleanup will be handled by unique_ptr destructors
}

core::Result<void> TieredMemoryIntegration::initialize() {
    try {
        // Initialize tiered memory manager
        // Initialize simple tiered memory management (replacing semantic vector dependencies)
        // SimpleTieredMemoryManager is now a local implementation
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Failed to initialize tiered memory integration: " + std::string(e.what()));
    }
}

core::Result<void> TieredMemoryIntegration::add_series(const core::SeriesID& series_id, 
                                                       MemoryTier tier) {
    try {
        // Simple series addition (replacing semantic vector dependencies)
        // Track series information
        std::lock_guard<std::mutex> lock(series_mutex_);
        SeriesInfo info;
        info.current_tier = tier;
        info.access_count.store(0);
        info.last_access_time = 0;
        info.is_hot = false;
        info.created_at = std::chrono::system_clock::now();
        series_info_[series_id] = std::move(info);
        
        // Update tier statistics
        update_tier_statistics(MemoryTier::RAM, tier);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Add series exception: " + std::string(e.what()));
    }
}

core::Result<void> TieredMemoryIntegration::remove_series(const core::SeriesID& series_id) {
    try {
        // Simple series removal (replacing semantic vector dependencies)
        // Get current tier before removal
        MemoryTier current_tier = MemoryTier::RAM;
        {
            std::lock_guard<std::mutex> lock(series_mutex_);
            auto it = series_info_.find(series_id);
            if (it != series_info_.end()) {
                current_tier = it->second.current_tier;
                series_info_.erase(it);
            }
        }
        
        // Update tier statistics
        update_tier_statistics(current_tier, MemoryTier::RAM);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Remove series exception: " + std::string(e.what()));
    }
}

core::Result<void> TieredMemoryIntegration::promote_series(const core::SeriesID& series_id) {
    return migrate_series(series_id, MemoryTier::RAM);
}

core::Result<void> TieredMemoryIntegration::demote_series(const core::SeriesID& series_id) {
    return migrate_series(series_id, MemoryTier::SSD);
}

core::Result<MemoryTier> TieredMemoryIntegration::get_series_tier(const core::SeriesID& series_id) {
    std::lock_guard<std::mutex> lock(series_mutex_);
    
    auto it = series_info_.find(series_id);
    if (it != series_info_.end()) {
        return core::Result<MemoryTier>(it->second.current_tier);
    }
    
    return core::Result<MemoryTier>::error("Series not found");
}

core::Result<void> TieredMemoryIntegration::optimize_tiered_layout() {
    try {
        // Identify hot and cold series
        auto hot_series = identify_hot_series();
        auto cold_series = identify_cold_series();
        
        // Promote hot series to RAM
        for (const auto& series_id : hot_series) {
            auto result = promote_series(series_id);
            if (result.ok()) {
                successful_migrations_.fetch_add(1);
            } else {
                failed_migrations_.fetch_add(1);
            }
            total_migrations_.fetch_add(1);
        }
        
        // Demote cold series to SSD/HDD
        for (const auto& series_id : cold_series) {
            auto result = demote_series(series_id);
            if (result.ok()) {
                successful_migrations_.fetch_add(1);
            } else {
                failed_migrations_.fetch_add(1);
            }
            total_migrations_.fetch_add(1);
        }
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Tiered layout optimization exception: " + std::string(e.what()));
    }
}

std::string TieredMemoryIntegration::get_tiered_stats() const {
    std::ostringstream oss;
    oss << "Tiered Memory Statistics:\n"
        << "  RAM Series Count: " << ram_series_count_.load() << "\n"
        << "  SSD Series Count: " << ssd_series_count_.load() << "\n"
        << "  HDD Series Count: " << hdd_series_count_.load() << "\n"
        << "  Total Migrations: " << total_migrations_.load() << "\n"
        << "  Successful Migrations: " << successful_migrations_.load() << "\n"
        << "  Failed Migrations: " << failed_migrations_.load();
    
    return oss.str();
}

std::string TieredMemoryIntegration::get_series_tier_stats() const {
    std::lock_guard<std::mutex> lock(series_mutex_);
    
    size_t total_series = series_info_.size();
    size_t hot_series = 0;
    size_t total_accesses = 0;
    
    for (const auto& [series_id, info] : series_info_) {
        if (info.is_hot.load()) {
            hot_series++;
        }
        total_accesses += info.access_count.load();
    }
    
    std::ostringstream oss;
    oss << "Series Tier Statistics:\n"
        << "  Total Series: " << total_series << "\n"
        << "  Hot Series: " << hot_series << "\n"
        << "  Total Accesses: " << total_accesses << "\n"
        << "  Average Accesses per Series: " 
        << (total_series > 0 ? static_cast<double>(total_accesses) / total_series : 0.0);
    
    return oss.str();
}

std::string TieredMemoryIntegration::get_migration_stats() const {
    double success_rate = total_migrations_.load() > 0 ? 
        (static_cast<double>(successful_migrations_.load()) / total_migrations_.load()) * 100.0 : 0.0;
    
    std::ostringstream oss;
    oss << "Migration Statistics:\n"
        << "  Total Migrations: " << total_migrations_.load() << "\n"
        << "  Successful Migrations: " << successful_migrations_.load() << "\n"
        << "  Failed Migrations: " << failed_migrations_.load() << "\n"
        << "  Success Rate: " << std::fixed << std::setprecision(2) << success_rate << "%";
    
    return oss.str();
}

void TieredMemoryIntegration::update_series_access(const core::SeriesID& series_id) {
    std::lock_guard<std::mutex> lock(series_mutex_);
    
    auto it = series_info_.find(series_id);
    if (it != series_info_.end()) {
        it->second.access_count.fetch_add(1);
        it->second.last_access_time.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        
        // Mark as hot if access count exceeds threshold
        if (it->second.access_count.load() > 10) {
            it->second.is_hot.store(true);
        }
    }
}

std::vector<core::SeriesID> TieredMemoryIntegration::identify_hot_series() {
    std::lock_guard<std::mutex> lock(series_mutex_);
    std::vector<core::SeriesID> hot_series;
    
    for (const auto& [series_id, info] : series_info_) {
        if (info.is_hot.load() || info.access_count.load() > 10) {
            hot_series.push_back(series_id);
        }
    }
    
    return hot_series;
}

std::vector<core::SeriesID> TieredMemoryIntegration::identify_cold_series() {
    std::lock_guard<std::mutex> lock(series_mutex_);
    std::vector<core::SeriesID> cold_series;
    
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    for (const auto& [series_id, info] : series_info_) {
        if (!info.is_hot.load() && 
            info.access_count.load() < 5 && 
            (now - info.last_access_time.load()) > 60000) { // 1 minute threshold
            cold_series.push_back(series_id);
        }
    }
    
    return cold_series;
}

core::Result<void> TieredMemoryIntegration::migrate_series(const core::SeriesID& series_id, 
                                                          MemoryTier target_tier) {
    try {
        // Get current tier
        MemoryTier current_tier = MemoryTier::RAM;
        {
            std::lock_guard<std::mutex> lock(series_mutex_);
            auto it = series_info_.find(series_id);
            if (it != series_info_.end()) {
                current_tier = it->second.current_tier;
            }
        }
        
        // Simple migration (replacing semantic vector dependencies)
        // Update series information
        {
            std::lock_guard<std::mutex> lock(series_mutex_);
            auto it = series_info_.find(series_id);
            if (it != series_info_.end()) {
                it->second.current_tier = target_tier;
            }
        }
        
        // Update tier statistics
        update_tier_statistics(current_tier, target_tier);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Series migration exception: " + std::string(e.what()));
    }
}

void TieredMemoryIntegration::update_tier_statistics(MemoryTier old_tier, 
                                                     MemoryTier new_tier) {
    // Decrement old tier count
    switch (old_tier) {
        case MemoryTier::RAM:
            ram_series_count_.fetch_sub(1);
            break;
        case MemoryTier::SSD:
            ssd_series_count_.fetch_sub(1);
            break;
        case MemoryTier::HDD:
            hdd_series_count_.fetch_sub(1);
            break;
    }
    
    // Increment new tier count
    switch (new_tier) {
        case MemoryTier::RAM:
            ram_series_count_.fetch_add(1);
            break;
        case MemoryTier::SSD:
            ssd_series_count_.fetch_add(1);
            break;
        case MemoryTier::HDD:
            hdd_series_count_.fetch_add(1);
            break;
    }
}

} // namespace storage
} // namespace tsdb
