#include "tsdb/storage/performance_config.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace tsdb {
namespace storage {
namespace internal {

// Global instance and mutex
std::unique_ptr<PerformanceConfig> GlobalPerformanceConfig::global_instance_;
std::mutex GlobalPerformanceConfig::instance_mutex_;

PerformanceConfig::PerformanceConfig(const std::string& config_name)
    : config_name_(config_name) {
    // Initialize with default values
    resetToDefaults();
}

ValidationResult PerformanceConfig::updateFlags(const PerformanceFlags& flags) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    auto validation = validateFlags(flags);
    if (validation.is_valid) {
        flags_ = flags;
        notifyChangeCallbacks();
    }
    
    return validation;
}

ValidationResult PerformanceConfig::updateThresholds(const PerformanceThresholds& thresholds) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    auto validation = validateThresholds(thresholds);
    if (validation.is_valid) {
        thresholds_ = thresholds;
        notifyChangeCallbacks();
    }
    
    return validation;
}

ValidationResult PerformanceConfig::updateRuntimeConfig(const RuntimeConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    runtime_config_ = config;
    notifyChangeCallbacks();
    
    return ValidationResult{};  // Runtime config doesn't need validation
}

ValidationResult PerformanceConfig::startABTest(const ABTestConfig& test_config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    auto validation = validateABTestConfig(test_config);
    if (validation.is_valid) {
        ab_test_config_ = test_config;
        ab_test_config_.start_time = std::chrono::system_clock::now();
        ab_test_active_.store(true);
        
        // Reset counters
        variant_a_requests_.store(0);
        variant_b_requests_.store(0);
        ab_test_metrics_.clear();
        
        notifyChangeCallbacks();
    }
    
    return validation;
}

void PerformanceConfig::stopABTest() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    ab_test_active_.store(false);
    notifyChangeCallbacks();
}

std::string PerformanceConfig::getABTestVariant(const std::string& user_id) {
    if (!ab_test_active_.load()) {
        return "control";
    }
    
    // Check if test duration has expired
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - ab_test_config_.start_time);
    
    if (elapsed > ab_test_config_.test_duration) {
        return "control";  // Test expired, return to control
    }
    
    // Calculate current rollout percentage
    double current_rollout = calculateRolloutPercentage();
    
    // Hash user ID for consistent assignment
    uint32_t hash = hashUserId(user_id);
    double user_percentage = (hash % 10000) / 100.0;  // 0-99.99
    
    if (user_percentage < current_rollout) {
        // User is in the rollout group
        if (user_percentage < (current_rollout * ab_test_config_.variant_a_percentage / 100.0)) {
            variant_a_requests_.fetch_add(1);
            return ab_test_config_.variant_a_name;
        } else {
            variant_b_requests_.fetch_add(1);
            return ab_test_config_.variant_b_name;
        }
    }
    
    // User is not in rollout yet
    return "control";
}

ABTestConfig PerformanceConfig::getABTestResults() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    ABTestConfig results = ab_test_config_;
    
    // Add current metrics
    results.metrics_to_track.clear();
    for (const auto& [metric_name, value] : ab_test_metrics_) {
        results.metrics_to_track.push_back(metric_name + ": " + std::to_string(value));
    }
    
    return results;
}

void PerformanceConfig::registerChangeCallback(std::function<void(const PerformanceConfig&)> callback) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    change_callbacks_.push_back(callback);
}

ValidationResult PerformanceConfig::validate() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    ValidationResult result;
    
    // Validate flags
    auto flags_validation = validateFlags(flags_);
    if (!flags_validation.is_valid) {
        result.errors.insert(result.errors.end(), flags_validation.errors.begin(), flags_validation.errors.end());
        result.is_valid = false;
    }
    
    // Validate thresholds
    auto thresholds_validation = validateThresholds(thresholds_);
    if (!thresholds_validation.is_valid) {
        result.errors.insert(result.errors.end(), thresholds_validation.errors.begin(), thresholds_validation.errors.end());
        result.is_valid = false;
    }
    
    // Validate A/B test if active
    if (ab_test_active_.load()) {
        auto ab_validation = validateABTestConfig(ab_test_config_);
        if (!ab_validation.is_valid) {
            result.errors.insert(result.errors.end(), ab_validation.errors.begin(), ab_validation.errors.end());
            result.is_valid = false;
        }
    }
    
    return result;
}

ValidationResult PerformanceConfig::loadFromFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        ValidationResult result;
        result.addError("Could not open configuration file: " + file_path);
        return result;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return fromJson(buffer.str());
}

bool PerformanceConfig::saveToFile(const std::string& file_path) const {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << toJson();
    return true;
}

std::string PerformanceConfig::toJson() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"config_name\": \"" << config_name_ << "\",\n";
    
    // Performance flags
    oss << "  \"flags\": {\n";
    oss << "    \"enable_object_pooling\": " << (flags_.enable_object_pooling ? "true" : "false") << ",\n";
    oss << "    \"enable_working_set_cache\": " << (flags_.enable_working_set_cache ? "true" : "false") << ",\n";
    oss << "    \"enable_type_aware_compression\": " << (flags_.enable_type_aware_compression ? "true" : "false") << ",\n";
    oss << "    \"enable_delta_of_delta_encoding\": " << (flags_.enable_delta_of_delta_encoding ? "true" : "false") << ",\n";
    oss << "    \"enable_atomic_metrics\": " << (flags_.enable_atomic_metrics ? "true" : "false") << "\n";
    oss << "  },\n";
    
    // Performance thresholds
    oss << "  \"thresholds\": {\n";
    oss << "    \"max_memory_usage_mb\": " << thresholds_.max_memory_usage_mb << ",\n";
    oss << "    \"cache_size_mb\": " << thresholds_.cache_size_mb << ",\n";
    oss << "    \"max_write_latency_ms\": " << thresholds_.max_write_latency_ms << ",\n";
    oss << "    \"max_read_latency_ms\": " << thresholds_.max_read_latency_ms << ",\n";
    oss << "    \"min_compression_ratio\": " << thresholds_.min_compression_ratio << ",\n";
    oss << "    \"max_compression_ratio\": " << thresholds_.max_compression_ratio << "\n";
    oss << "  },\n";
    
    // Runtime config
    oss << "  \"runtime\": {\n";
    oss << "    \"metrics_sampling_interval\": " << runtime_config_.metrics_sampling_interval << ",\n";
    oss << "    \"performance_check_interval_ms\": " << runtime_config_.performance_check_interval_ms << ",\n";
    oss << "    \"enable_adaptive_tuning\": " << (runtime_config_.enable_adaptive_tuning ? "true" : "false") << ",\n";
    oss << "    \"enable_automatic_rollback\": " << (runtime_config_.enable_automatic_rollback ? "true" : "false") << "\n";
    oss << "  }";
    
    // A/B test info if active
    if (ab_test_active_.load()) {
        oss << ",\n";
        oss << "  \"ab_test\": {\n";
        oss << "    \"test_name\": \"" << ab_test_config_.test_name << "\",\n";
        oss << "    \"variant_a_requests\": " << variant_a_requests_.load() << ",\n";
        oss << "    \"variant_b_requests\": " << variant_b_requests_.load() << ",\n";
        oss << "    \"active\": true\n";
        oss << "  }";
    }
    
    oss << "\n}";
    return oss.str();
}

ValidationResult PerformanceConfig::fromJson(const std::string& json_str) {
    // Simple JSON parsing for basic configuration
    // In a production system, use a proper JSON library like nlohmann/json
    
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    ValidationResult result;
    
    // Basic validation - check if it looks like JSON
    if (json_str.empty() || json_str[0] != '{') {
        result.addError("Invalid JSON format");
        return result;
    }
    
    // For now, just validate the structure
    // In a real implementation, parse the JSON and update the configuration
    
    notifyChangeCallbacks();
    return result;
}

std::string PerformanceConfig::getSummary() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    std::ostringstream oss;
    oss << "Performance Configuration: " << config_name_ << "\n";
    oss << "=====================================\n";
    
    // Flags summary
    oss << "Feature Flags:\n";
    oss << "  Object Pooling: " << (flags_.enable_object_pooling ? "✅" : "❌") << "\n";
    oss << "  Working Set Cache: " << (flags_.enable_working_set_cache ? "✅" : "❌") << "\n";
    oss << "  Type-Aware Compression: " << (flags_.enable_type_aware_compression ? "✅" : "❌") << "\n";
    oss << "  Delta-of-Delta Encoding: " << (flags_.enable_delta_of_delta_encoding ? "✅" : "❌") << "\n";
    oss << "  Atomic Metrics: " << (flags_.enable_atomic_metrics ? "✅" : "❌") << "\n";
    
    // Thresholds summary
    oss << "\nPerformance Thresholds:\n";
    oss << "  Max Memory: " << thresholds_.max_memory_usage_mb << " MB\n";
    oss << "  Cache Size: " << thresholds_.cache_size_mb << " MB\n";
    oss << "  Max Write Latency: " << thresholds_.max_write_latency_ms << " ms\n";
    oss << "  Max Read Latency: " << thresholds_.max_read_latency_ms << " ms\n";
    oss << "  Compression Ratio: " << thresholds_.min_compression_ratio << " - " << thresholds_.max_compression_ratio << "\n";
    
    // Runtime config summary
    oss << "\nRuntime Configuration:\n";
    oss << "  Adaptive Tuning: " << (runtime_config_.enable_adaptive_tuning ? "✅" : "❌") << "\n";
    oss << "  Automatic Rollback: " << (runtime_config_.enable_automatic_rollback ? "✅" : "❌") << "\n";
    oss << "  Metrics Sampling: every " << runtime_config_.metrics_sampling_interval << " operations\n";
    
    // A/B test summary
    if (ab_test_active_.load()) {
        oss << "\nA/B Test Active:\n";
        oss << "  Test Name: " << ab_test_config_.test_name << "\n";
        oss << "  Variant A Requests: " << variant_a_requests_.load() << "\n";
        oss << "  Variant B Requests: " << variant_b_requests_.load() << "\n";
        oss << "  Current Rollout: " << std::fixed << std::setprecision(1) << calculateRolloutPercentage() << "%\n";
    }
    
    return oss.str();
}

void PerformanceConfig::resetToDefaults() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Reset all to default values
    flags_ = PerformanceFlags{};
    thresholds_ = PerformanceThresholds{};
    runtime_config_ = RuntimeConfig{};
    ab_test_config_ = ABTestConfig{};
    
    ab_test_active_.store(false);
    variant_a_requests_.store(0);
    variant_b_requests_.store(0);
    ab_test_metrics_.clear();
    
    notifyChangeCallbacks();
}

bool PerformanceConfig::isFeatureEnabled(const std::string& feature_name) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (feature_name == "object_pooling") return flags_.enable_object_pooling;
    if (feature_name == "working_set_cache") return flags_.enable_working_set_cache;
    if (feature_name == "type_aware_compression") return flags_.enable_type_aware_compression;
    if (feature_name == "delta_of_delta_encoding") return flags_.enable_delta_of_delta_encoding;
    if (feature_name == "atomic_metrics") return flags_.enable_atomic_metrics;
    if (feature_name == "sharded_writes") return flags_.enable_sharded_writes;
    if (feature_name == "background_processing") return flags_.enable_background_processing;
    if (feature_name == "simd_compression") return flags_.enable_simd_compression;
    if (feature_name == "parallel_queries") return flags_.enable_parallel_queries;
    
    return false;  // Unknown feature
}

ValidationResult PerformanceConfig::setFeatureEnabled(const std::string& feature_name, bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    ValidationResult result;
    
    if (feature_name == "object_pooling") {
        flags_.enable_object_pooling = enabled;
    } else if (feature_name == "working_set_cache") {
        flags_.enable_working_set_cache = enabled;
    } else if (feature_name == "type_aware_compression") {
        flags_.enable_type_aware_compression = enabled;
    } else if (feature_name == "delta_of_delta_encoding") {
        flags_.enable_delta_of_delta_encoding = enabled;
    } else if (feature_name == "atomic_metrics") {
        flags_.enable_atomic_metrics = enabled;
    } else if (feature_name == "sharded_writes") {
        flags_.enable_sharded_writes = enabled;
    } else if (feature_name == "background_processing") {
        flags_.enable_background_processing = enabled;
    } else if (feature_name == "simd_compression") {
        flags_.enable_simd_compression = enabled;
    } else if (feature_name == "parallel_queries") {
        flags_.enable_parallel_queries = enabled;
    } else {
        result.addError("Unknown feature: " + feature_name);
        return result;
    }
    
    notifyChangeCallbacks();
    return result;
}

void PerformanceConfig::notifyChangeCallbacks() {
    for (const auto& callback : change_callbacks_) {
        try {
            callback(*this);
        } catch (...) {
            // Ignore callback errors
        }
    }
}

ValidationResult PerformanceConfig::validateFlags(const PerformanceFlags& flags) const {
    ValidationResult result;
    
    // Check for conflicting flags
    if (flags.enable_sharded_writes && !flags.enable_background_processing) {
        result.addWarning("Sharded writes work best with background processing enabled");
    }
    
    if (flags.enable_simd_compression && !flags.enable_type_aware_compression) {
        result.addWarning("SIMD compression works best with type-aware compression");
    }
    
    if (flags.enable_machine_learning_optimization && !flags.enable_atomic_metrics) {
        result.addError("Machine learning optimization requires atomic metrics to be enabled");
    }
    
    return result;
}

ValidationResult PerformanceConfig::validateThresholds(const PerformanceThresholds& thresholds) const {
    ValidationResult result;
    
    // Validate memory thresholds
    if (thresholds.max_memory_usage_mb < thresholds.cache_size_mb) {
        result.addError("Max memory usage must be greater than cache size");
    }
    
    if (thresholds.max_memory_usage_mb == 0) {
        result.addError("Max memory usage cannot be zero");
    }
    
    // Validate performance thresholds
    if (thresholds.max_write_latency_ms <= 0) {
        result.addError("Max write latency must be positive");
    }
    
    if (thresholds.max_read_latency_ms <= 0) {
        result.addError("Max read latency must be positive");
    }
    
    if (thresholds.min_compression_ratio >= thresholds.max_compression_ratio) {
        result.addError("Min compression ratio must be less than max compression ratio");
    }
    
    if (thresholds.min_compression_ratio <= 0) {
        result.addError("Min compression ratio must be positive");
    }
    
    // Validate concurrency limits
    if (thresholds.max_concurrent_writes == 0) {
        result.addError("Max concurrent writes cannot be zero");
    }
    
    if (thresholds.max_concurrent_reads == 0) {
        result.addError("Max concurrent reads cannot be zero");
    }
    
    // Validate cache thresholds
    if (thresholds.min_cache_hit_ratio < 0 || thresholds.min_cache_hit_ratio > 1) {
        result.addError("Min cache hit ratio must be between 0 and 1");
    }
    
    if (thresholds.cache_eviction_threshold > 100) {
        result.addError("Cache eviction threshold cannot exceed 100%");
    }
    
    return result;
}

ValidationResult PerformanceConfig::validateABTestConfig(const ABTestConfig& test_config) const {
    ValidationResult result;
    
    if (test_config.test_name.empty()) {
        result.addError("A/B test name cannot be empty");
    }
    
    if (test_config.variant_a_name.empty() || test_config.variant_b_name.empty()) {
        result.addError("Variant names cannot be empty");
    }
    
    if (test_config.variant_a_percentage + test_config.variant_b_percentage != 100.0) {
        result.addError("Variant percentages must sum to 100%");
    }
    
    if (test_config.test_duration.count() <= 0) {
        result.addError("Test duration must be positive");
    }
    
    if (test_config.rollout_percentage < 0 || test_config.rollout_percentage > 100) {
        result.addError("Rollout percentage must be between 0 and 100");
    }
    
    if (test_config.min_improvement_percentage < 0) {
        result.addError("Minimum improvement percentage must be non-negative");
    }
    
    if (test_config.confidence_level < 0.5 || test_config.confidence_level > 0.99) {
        result.addError("Confidence level must be between 0.5 and 0.99");
    }
    
    return result;
}

uint32_t PerformanceConfig::hashUserId(const std::string& user_id) const {
    // Simple hash function for consistent user assignment
    uint32_t hash = 0;
    for (char c : user_id) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

double PerformanceConfig::calculateRolloutPercentage() const {
    if (!ab_test_active_.load()) {
        return 0.0;
    }
    
    if (!ab_test_config_.enable_gradual_rollout) {
        return 100.0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
        now - ab_test_config_.start_time);
    
    double rollout_steps = elapsed.count() / ab_test_config_.rollout_interval.count();
    double current_rollout = ab_test_config_.rollout_percentage + 
                           (rollout_steps * ab_test_config_.rollout_percentage);
    
    return std::min(current_rollout, 100.0);
}

// GlobalPerformanceConfig implementation
PerformanceConfig& GlobalPerformanceConfig::getInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!global_instance_) {
        global_instance_ = std::make_unique<PerformanceConfig>("global");
    }
    return *global_instance_;
}

void GlobalPerformanceConfig::initialize(const std::string& config_name) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    global_instance_ = std::make_unique<PerformanceConfig>(config_name);
}

ValidationResult GlobalPerformanceConfig::loadFromFile(const std::string& file_path) {
    return getInstance().loadFromFile(file_path);
}

bool GlobalPerformanceConfig::saveToFile(const std::string& file_path) {
    return getInstance().saveToFile(file_path);
}

void GlobalPerformanceConfig::resetToDefaults() {
    getInstance().resetToDefaults();
}

} // namespace internal
} // namespace storage
} // namespace tsdb 