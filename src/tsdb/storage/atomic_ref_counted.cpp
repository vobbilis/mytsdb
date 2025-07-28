#include "tsdb/storage/atomic_ref_counted.h"
#include <iostream>
#include <sstream>

namespace tsdb {
namespace storage {

// Global statistics for all AtomicRefCounted instances
namespace {
    std::atomic<uint64_t> global_total_instances{0};
    std::atomic<uint64_t> global_total_operations{0};
    std::atomic<uint64_t> global_peak_instances{0};
}

/**
 * @brief Get global statistics for all AtomicRefCounted instances
 * @return String containing global statistics
 */
std::string getGlobalAtomicRefCountedStats() {
    std::ostringstream oss;
    oss << "Global AtomicRefCounted Statistics:\n";
    oss << "  Total instances created: " << global_total_instances.load(std::memory_order_relaxed) << "\n";
    oss << "  Total operations performed: " << global_total_operations.load(std::memory_order_relaxed) << "\n";
    oss << "  Peak concurrent instances: " << global_peak_instances.load(std::memory_order_relaxed) << "\n";
    
    uint64_t total_instances = global_total_instances.load(std::memory_order_relaxed);
    uint64_t total_ops = global_total_operations.load(std::memory_order_relaxed);
    
    if (total_instances > 0) {
        double avg_ops_per_instance = static_cast<double>(total_ops) / total_instances;
        oss << "  Average operations per instance: " << std::fixed << std::setprecision(2) << avg_ops_per_instance << "\n";
    }
    
    return oss.str();
}

/**
 * @brief Reset global statistics
 */
void resetGlobalAtomicRefCountedStats() {
    global_total_instances.store(0, std::memory_order_relaxed);
    global_total_operations.store(0, std::memory_order_relaxed);
    global_peak_instances.store(0, std::memory_order_relaxed);
}

/**
 * @brief Increment global instance counter
 */
void incrementGlobalInstanceCount() {
    uint64_t current_count = global_total_instances.fetch_add(1, std::memory_order_relaxed);
    uint64_t new_count = current_count + 1;
    
    // Update peak count
    uint64_t current_peak = global_peak_instances.load(std::memory_order_relaxed);
    while (new_count > current_peak) {
        if (global_peak_instances.compare_exchange_weak(current_peak, new_count, 
                                                       std::memory_order_relaxed)) {
            break;
        }
    }
}

/**
 * @brief Increment global operation counter
 */
void incrementGlobalOperationCount() {
    global_total_operations.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief Validate AtomicRefCounted configuration
 * @param config Configuration to validate
 * @return true if configuration is valid
 */
bool validateAtomicRefCountedConfig(const AtomicRefCountedConfig& config) {
    if (config.max_ref_count == 0) {
        return false;  // Max ref count cannot be zero
    }
    
    if (config.max_ref_count > 10000000) {
        return false;  // Max ref count too high (safety limit)
    }
    
    return true;
}

/**
 * @brief Get default configuration for high-performance scenarios
 * @return Optimized configuration for high-performance use
 */
AtomicRefCountedConfig getHighPerformanceConfig() {
    AtomicRefCountedConfig config;
    config.enable_performance_tracking = true;
    config.use_relaxed_ordering = true;  // Use relaxed ordering for better performance
    config.enable_debug_logging = false;
    config.max_ref_count = 1000000;
    return config;
}

/**
 * @brief Get default configuration for debugging scenarios
 * @return Configuration optimized for debugging
 */
AtomicRefCountedConfig getDebugConfig() {
    AtomicRefCountedConfig config;
    config.enable_performance_tracking = true;
    config.use_relaxed_ordering = false;  // Use strict ordering for debugging
    config.enable_debug_logging = true;
    config.max_ref_count = 100000;
    return config;
}

/**
 * @brief Get default configuration for safety-critical scenarios
 * @return Configuration optimized for safety
 */
AtomicRefCountedConfig getSafetyConfig() {
    AtomicRefCountedConfig config;
    config.enable_performance_tracking = true;
    config.use_relaxed_ordering = false;  // Use strict ordering for safety
    config.enable_debug_logging = true;
    config.max_ref_count = 100000;  // Lower limit for safety
    return config;
}

} // namespace storage
} // namespace tsdb 