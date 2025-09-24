#include "simple_access_pattern_tracker.h"
#include <chrono>
#include <sstream>
#include <algorithm>

namespace tsdb {
namespace storage {
namespace memory_optimization {

SimpleAccessPatternTracker::SimpleAccessPatternTracker() = default;

SimpleAccessPatternTracker::~SimpleAccessPatternTracker() = default;

void SimpleAccessPatternTracker::record_access(void* ptr) {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    auto& info = access_patterns_[ptr];
    info.access_count.fetch_add(1);
    info.last_access_time.store(get_current_time());
    
    total_accesses_.fetch_add(1);
}

void SimpleAccessPatternTracker::record_bulk_access(const std::vector<void*>& addresses) {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    for (void* ptr : addresses) {
        auto& info = access_patterns_[ptr];
        info.access_count.fetch_add(1);
        info.last_access_time.store(get_current_time());
    }
    
    total_accesses_.fetch_add(addresses.size());
}

void SimpleAccessPatternTracker::analyze_patterns() {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    // Update unique addresses count
    unique_addresses_.store(access_patterns_.size());
}

std::vector<void*> SimpleAccessPatternTracker::get_hot_addresses() const {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    std::vector<void*> hot_addresses;
    for (const auto& pair : access_patterns_) {
        if (is_hot_address(pair.first)) {
            hot_addresses.push_back(pair.first);
        }
    }
    
    return hot_addresses;
}

std::vector<void*> SimpleAccessPatternTracker::get_cold_addresses() const {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    std::vector<void*> cold_addresses;
    for (const auto& pair : access_patterns_) {
        if (is_cold_address(pair.first)) {
            cold_addresses.push_back(pair.first);
        }
    }
    
    return cold_addresses;
}

size_t SimpleAccessPatternTracker::get_access_count(void* ptr) const {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    auto it = access_patterns_.find(ptr);
    if (it != access_patterns_.end()) {
        return it->second.access_count.load();
    }
    
    return 0;
}

void SimpleAccessPatternTracker::clear() {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    access_patterns_.clear();
    total_accesses_.store(0);
    unique_addresses_.store(0);
}

std::string SimpleAccessPatternTracker::get_stats() const {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    // Count hot and cold addresses without calling methods that acquire locks
    size_t hot_count = 0;
    size_t cold_count = 0;
    for (const auto& pair : access_patterns_) {
        if (is_hot_address(pair.first)) {
            hot_count++;
        }
        if (is_cold_address(pair.first)) {
            cold_count++;
        }
    }
    
    std::ostringstream oss;
    oss << "Access Pattern Stats:\n";
    oss << "  Total Accesses: " << total_accesses_.load() << "\n";
    oss << "  Unique Addresses: " << unique_addresses_.load() << "\n";
    oss << "  Hot Addresses: " << hot_count << "\n";
    oss << "  Cold Addresses: " << cold_count << "\n";
    
    return oss.str();
}

uint64_t SimpleAccessPatternTracker::get_current_time() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

bool SimpleAccessPatternTracker::is_hot_address(void* ptr) const {
    auto it = access_patterns_.find(ptr);
    if (it != access_patterns_.end()) {
        // Consider an address "hot" if it has been accessed more than 10 times
        return it->second.access_count.load() > 10;
    }
    
    return false;
}

bool SimpleAccessPatternTracker::is_cold_address(void* ptr) const {
    auto it = access_patterns_.find(ptr);
    if (it != access_patterns_.end()) {
        // Consider an address "cold" if it has been accessed less than 3 times
        return it->second.access_count.load() < 3;
    }
    
    return true; // Unknown addresses are considered cold
}

} // namespace memory_optimization
} // namespace storage
} // namespace tsdb
