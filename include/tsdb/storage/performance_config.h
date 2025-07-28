#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>

namespace tsdb {
namespace storage {
namespace internal {

/**
 * @brief Performance feature flags for controlling optimizations
 */
struct PerformanceFlags {
    // Memory management features
    bool enable_object_pooling = true;
    bool enable_working_set_cache = true;
    bool enable_memory_pooling = true;
    
    // Compression features
    bool enable_type_aware_compression = true;
    bool enable_delta_of_delta_encoding = true;
    bool enable_adaptive_compression = true;
    
    // Concurrency features
    bool enable_sharded_writes = false;
    bool enable_background_processing = false;
    bool enable_lock_free_structures = false;
    
    // Caching features
    bool enable_multi_level_caching = false;
    bool enable_predictive_caching = false;
    
    // SIMD features
    bool enable_simd_compression = false;
    bool enable_simd_decompression = false;
    
    // Query optimization features
    bool enable_parallel_queries = false;
    bool enable_query_planning = false;
    bool enable_index_optimization = false;
    
    // Metrics and monitoring
    bool enable_atomic_metrics = true;
    bool enable_performance_tracking = true;
    bool enable_detailed_logging = false;
    
    // Experimental features
    bool enable_experimental_compression = false;
    bool enable_machine_learning_optimization = false;
};

/**
 * @brief Performance thresholds and limits
 */
struct PerformanceThresholds {
    // Memory thresholds
    size_t max_memory_usage_mb = 8192;  // 8GB default
    size_t cache_size_mb = 1024;        // 1GB default
    size_t object_pool_size = 10000;    // 10K objects default
    
    // Performance thresholds
    double max_write_latency_ms = 10.0;
    double max_read_latency_ms = 5.0;
    double min_compression_ratio = 0.1;
    double max_compression_ratio = 0.8;
    
    // Concurrency limits
    uint32_t max_concurrent_writes = 100;
    uint32_t max_concurrent_reads = 200;
    uint32_t background_threads = 4;
    
    // Cache thresholds
    double min_cache_hit_ratio = 0.7;
    uint32_t cache_eviction_threshold = 80;  // percentage
    
    // Error thresholds
    double max_error_rate = 0.001;  // 0.1%
    uint32_t max_retry_attempts = 3;
};

/**
 * @brief A/B testing configuration
 */
struct ABTestConfig {
    std::string test_name;
    std::string variant_a_name = "control";
    std::string variant_b_name = "treatment";
    
    double variant_a_percentage = 50.0;  // 50% control, 50% treatment
    double variant_b_percentage = 50.0;
    
    std::chrono::seconds test_duration{86400};  // 24 hours default
    std::chrono::system_clock::time_point start_time;
    
    bool enable_gradual_rollout = true;
    double rollout_percentage = 10.0;  // Start with 10%
    std::chrono::minutes rollout_interval{60};  // Increase every hour
    
    // Metrics to track
    std::vector<std::string> metrics_to_track = {
        "write_throughput",
        "read_latency",
        "compression_ratio",
        "memory_usage",
        "error_rate"
    };
    
    // Success criteria
    double min_improvement_percentage = 5.0;  // 5% improvement required
    double confidence_level = 0.95;  // 95% confidence level
};

/**
 * @brief Runtime configuration for performance tuning
 */
struct RuntimeConfig {
    // Sampling and monitoring
    uint32_t metrics_sampling_interval = 1000;  // Sample every 1000 operations
    uint32_t performance_check_interval_ms = 5000;  // Check every 5 seconds
    
    // Adaptive tuning
    bool enable_adaptive_tuning = true;
    uint32_t tuning_check_interval_ms = 30000;  // Tune every 30 seconds
    double tuning_threshold = 0.1;  // 10% performance change triggers tuning
    
    // Rollback settings
    bool enable_automatic_rollback = true;
    uint32_t rollback_check_interval_ms = 10000;  // Check every 10 seconds
    double rollback_threshold = 0.2;  // 20% performance degradation triggers rollback
    
    // Logging and debugging
    bool enable_debug_logging = false;
    uint32_t log_level = 2;  // 0=error, 1=warn, 2=info, 3=debug
    std::string log_file_path = "";
};

/**
 * @brief Configuration validation result
 */
struct ValidationResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    void addError(const std::string& error) {
        is_valid = false;
        errors.push_back(error);
    }
    
    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
    
    std::string getSummary() const {
        std::string summary = "Validation " + std::string(is_valid ? "PASSED" : "FAILED");
        if (!errors.empty()) {
            summary += " - " + std::to_string(errors.size()) + " errors";
        }
        if (!warnings.empty()) {
            summary += " - " + std::to_string(warnings.size()) + " warnings";
        }
        return summary;
    }
};

/**
 * @brief Performance configuration manager
 * 
 * This class provides a centralized configuration system for managing
 * performance features, A/B testing, and runtime tuning.
 */
class PerformanceConfig {
public:
    explicit PerformanceConfig(const std::string& config_name = "default");
    
    /**
     * @brief Get current performance flags
     * @return Reference to current flags
     */
    const PerformanceFlags& getFlags() const { return flags_; }
    
    /**
     * @brief Update performance flags
     * @param flags New flags to apply
     * @return Validation result
     */
    ValidationResult updateFlags(const PerformanceFlags& flags);
    
    /**
     * @brief Get current performance thresholds
     * @return Reference to current thresholds
     */
    const PerformanceThresholds& getThresholds() const { return thresholds_; }
    
    /**
     * @brief Update performance thresholds
     * @param thresholds New thresholds to apply
     * @return Validation result
     */
    ValidationResult updateThresholds(const PerformanceThresholds& thresholds);
    
    /**
     * @brief Get current runtime configuration
     * @return Reference to current runtime config
     */
    const RuntimeConfig& getRuntimeConfig() const { return runtime_config_; }
    
    /**
     * @brief Update runtime configuration
     * @param config New runtime config to apply
     * @return Validation result
     */
    ValidationResult updateRuntimeConfig(const RuntimeConfig& config);
    
    /**
     * @brief Start A/B test
     * @param test_config A/B test configuration
     * @return Validation result
     */
    ValidationResult startABTest(const ABTestConfig& test_config);
    
    /**
     * @brief Stop current A/B test
     */
    void stopABTest();
    
    /**
     * @brief Get current A/B test variant for a user/request
     * @param user_id User identifier for consistent assignment
     * @return Variant name ("control" or "treatment")
     */
    std::string getABTestVariant(const std::string& user_id = "");
    
    /**
     * @brief Check if A/B test is active
     * @return True if A/B test is running
     */
    bool isABTestActive() const { return ab_test_active_.load(); }
    
    /**
     * @brief Get A/B test results
     * @return A/B test configuration with results
     */
    ABTestConfig getABTestResults() const;
    
    /**
     * @brief Register a configuration change callback
     * @param callback Function to call when configuration changes
     */
    void registerChangeCallback(std::function<void(const PerformanceConfig&)> callback);
    
    /**
     * @brief Validate current configuration
     * @return Validation result
     */
    ValidationResult validate() const;
    
    /**
     * @brief Load configuration from file
     * @param file_path Path to configuration file
     * @return Validation result
     */
    ValidationResult loadFromFile(const std::string& file_path);
    
    /**
     * @brief Save configuration to file
     * @param file_path Path to save configuration
     * @return True if successful
     */
    bool saveToFile(const std::string& file_path) const;
    
    /**
     * @brief Get configuration as JSON string
     * @return JSON representation of configuration
     */
    std::string toJson() const;
    
    /**
     * @brief Load configuration from JSON string
     * @param json_str JSON string to parse
     * @return Validation result
     */
    ValidationResult fromJson(const std::string& json_str);
    
    /**
     * @brief Get configuration summary
     * @return Human-readable summary
     */
    std::string getSummary() const;
    
    /**
     * @brief Reset to default configuration
     */
    void resetToDefaults();
    
    /**
     * @brief Check if a specific feature is enabled
     * @param feature_name Name of the feature to check
     * @return True if feature is enabled
     */
    bool isFeatureEnabled(const std::string& feature_name) const;
    
    /**
     * @brief Enable/disable a specific feature
     * @param feature_name Name of the feature
     * @param enabled Whether to enable or disable
     * @return Validation result
     */
    ValidationResult setFeatureEnabled(const std::string& feature_name, bool enabled);
    
    /**
     * @brief Get configuration name
     * @return Configuration name
     */
    const std::string& getConfigName() const { return config_name_; }

private:
    std::string config_name_;
    PerformanceFlags flags_;
    PerformanceThresholds thresholds_;
    RuntimeConfig runtime_config_;
    ABTestConfig ab_test_config_;
    
    std::atomic<bool> ab_test_active_{false};
    std::chrono::system_clock::time_point ab_test_start_time_;
    
    std::vector<std::function<void(const PerformanceConfig&)>> change_callbacks_;
    mutable std::mutex config_mutex_;
    
    // A/B test tracking
    std::atomic<uint64_t> variant_a_requests_{0};
    std::atomic<uint64_t> variant_b_requests_{0};
    std::unordered_map<std::string, double> ab_test_metrics_;
    
    /**
     * @brief Notify all change callbacks
     */
    void notifyChangeCallbacks();
    
    /**
     * @brief Validate performance flags
     * @param flags Flags to validate
     * @return Validation result
     */
    ValidationResult validateFlags(const PerformanceFlags& flags) const;
    
    /**
     * @brief Validate performance thresholds
     * @param thresholds Thresholds to validate
     * @return Validation result
     */
    ValidationResult validateThresholds(const PerformanceThresholds& thresholds) const;
    
    /**
     * @brief Validate A/B test configuration
     * @param test_config Test config to validate
     * @return Validation result
     */
    ValidationResult validateABTestConfig(const ABTestConfig& test_config) const;
    
    /**
     * @brief Hash function for consistent user assignment
     * @param user_id User identifier
     * @return Hash value
     */
    uint32_t hashUserId(const std::string& user_id) const;
    
    /**
     * @brief Calculate current rollout percentage
     * @return Current rollout percentage
     */
    double calculateRolloutPercentage() const;
};

/**
 * @brief Global performance configuration instance
 */
class GlobalPerformanceConfig {
public:
    /**
     * @brief Get global configuration instance
     * @return Reference to global configuration
     */
    static PerformanceConfig& getInstance();
    
    /**
     * @brief Initialize global configuration
     * @param config_name Configuration name
     */
    static void initialize(const std::string& config_name = "global");
    
    /**
     * @brief Load global configuration from file
     * @param file_path Path to configuration file
     * @return Validation result
     */
    static ValidationResult loadFromFile(const std::string& file_path);
    
    /**
     * @brief Save global configuration to file
     * @param file_path Path to save configuration
     * @return True if successful
     */
    static bool saveToFile(const std::string& file_path);
    
    /**
     * @brief Reset global configuration to defaults
     */
    static void resetToDefaults();

private:
    static std::unique_ptr<PerformanceConfig> global_instance_;
    static std::mutex instance_mutex_;
};

// Convenience macros for feature checking
#ifdef TSDB_ENABLE_PERFORMANCE_CONFIG
    #define TSDB_FEATURE_ENABLED(feature) \
        tsdb::storage::internal::GlobalPerformanceConfig::getInstance().isFeatureEnabled(feature)
    
    #define TSDB_AB_TEST_VARIANT(user_id) \
        tsdb::storage::internal::GlobalPerformanceConfig::getInstance().getABTestVariant(user_id)
    
    #define TSDB_AB_TEST_ACTIVE() \
        tsdb::storage::internal::GlobalPerformanceConfig::getInstance().isABTestActive()
#else
    #define TSDB_FEATURE_ENABLED(feature) (true)
    #define TSDB_AB_TEST_VARIANT(user_id) ("control")
    #define TSDB_AB_TEST_ACTIVE() (false)
#endif

} // namespace internal
} // namespace storage
} // namespace tsdb 