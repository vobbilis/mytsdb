#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <memory>

#include "tsdb/core/types.h"
#include "tsdb/core/semantic_vector_config.h"

namespace tsdb {
namespace core {
using Duration = int64_t;  // Duration in milliseconds
}
}

namespace tsdb {
namespace core {

/**
 * @brief Defines the granularity type for time series data
 */
enum class GranularityType {
    HIGH_FREQUENCY,  // Sub-second
    NORMAL,         // Second-level
    LOW_FREQUENCY   // Minute-level or higher
};

/**
 * @brief Configuration for data granularity and retention
 */
struct Granularity {
    GranularityType type;
    Duration min_interval;     // Minimum interval between samples
    Duration retention;        // How long to retain the data
    
    // Default constructor
    Granularity() : type(GranularityType::NORMAL), min_interval(0), retention(0) {}
    
    static Granularity HighFrequency() {
        Granularity granularity;
        granularity.type = GranularityType::HIGH_FREQUENCY;
        granularity.min_interval = 100'000;           // 100 microseconds
        granularity.retention = 86'400'000;          // 24 hours in milliseconds
        return granularity;
    }
    
    static Granularity Normal() {
        Granularity granularity;
        granularity.type = GranularityType::NORMAL;
        granularity.min_interval = 1'000;            // 1 second
        granularity.retention = 604'800'000;        // 1 week in milliseconds
        return granularity;
    }
    
    static Granularity LowFrequency() {
        Granularity granularity;
        granularity.type = GranularityType::LOW_FREQUENCY;
        granularity.min_interval = 60'000;           // 1 minute
        granularity.retention = 31'536'000'000;     // 1 year in milliseconds
        return granularity;
    }
};

/**
 * @brief Configuration for histogram data
 */
struct HistogramConfig {
    double relative_accuracy;     // Target relative accuracy for DDSketch
    size_t max_num_buckets;      // Maximum number of buckets per histogram
    bool use_fixed_buckets;      // Whether to use fixed-size buckets
    std::vector<double> bounds;  // Bucket bounds for fixed buckets
    
    // Default constructor
    HistogramConfig() : relative_accuracy(0.0), max_num_buckets(0), use_fixed_buckets(false) {}
    
    static HistogramConfig Default() {
        HistogramConfig config;
        config.relative_accuracy = 0.01;    // 1% relative accuracy
        config.max_num_buckets = 2048;      // Maximum buckets
        config.use_fixed_buckets = false;   // Use DDSketch by default
        config.bounds = {};                 // No fixed bounds
        return config;
    }
};

/**
 * @brief Configuration for object pools
 */
struct ObjectPoolConfig {
    size_t time_series_initial_size;    // Initial TimeSeries pool size
    size_t time_series_max_size;        // Maximum TimeSeries pool size
    size_t labels_initial_size;         // Initial Labels pool size
    size_t labels_max_size;             // Maximum Labels pool size
    size_t samples_initial_size;        // Initial Sample pool size
    size_t samples_max_size;            // Maximum Sample pool size
    
    // Default constructor
    ObjectPoolConfig() : time_series_initial_size(100), time_series_max_size(10000),
                        labels_initial_size(200), labels_max_size(20000),
                        samples_initial_size(1000), samples_max_size(100000) {}
    
    static ObjectPoolConfig Default() {
        ObjectPoolConfig config;
        config.time_series_initial_size = 100;
        config.time_series_max_size = 10000;
        config.labels_initial_size = 200;
        config.labels_max_size = 20000;
        config.samples_initial_size = 1000;
        config.samples_max_size = 100000;
        return config;
    }
};

/**
 * @brief Configuration for compression algorithms
 */
struct CompressionConfig {
    enum class Algorithm : uint8_t {
        NONE,           // No compression
        GORILLA,        // Facebook's Gorilla compression
        DELTA_XOR,      // XOR-based delta compression
        DICTIONARY,     // Dictionary-based compression
        RLE            // Run-length encoding
    };
    
    Algorithm timestamp_compression;    // Algorithm for timestamp compression
    Algorithm value_compression;        // Algorithm for value compression
    Algorithm label_compression;        // Algorithm for label compression
    bool adaptive_compression;          // Enable adaptive compression selection
    size_t compression_threshold;       // Minimum size for compression (bytes)
    uint32_t compression_level;         // Compression level (0-9)
    bool enable_simd;                   // Use SIMD acceleration if available
    
    // Default constructor
    CompressionConfig() : timestamp_compression(Algorithm::DELTA_XOR),
                         value_compression(Algorithm::GORILLA),
                         label_compression(Algorithm::DICTIONARY),
                         adaptive_compression(true),
                         compression_threshold(1024),
                         compression_level(6),
                         enable_simd(true) {}
    
    static CompressionConfig Default() {
        CompressionConfig config;
        config.timestamp_compression = Algorithm::DELTA_XOR;
        config.value_compression = Algorithm::GORILLA;
        config.label_compression = Algorithm::DICTIONARY;
        config.adaptive_compression = true;
        config.compression_threshold = 1024;
        config.compression_level = 6;
        config.enable_simd = true;
        return config;
    }
};

/**
 * @brief Configuration for block management
 */
struct BlockConfig {
    size_t max_block_size;              // Maximum size of a block in bytes
    size_t max_block_records;           // Maximum number of records in a block
    Duration block_duration;            // Duration after which to seal a block (milliseconds)
    size_t max_concurrent_compactions;  // Maximum number of concurrent compactions
    bool enable_multi_tier_storage;     // Enable HOT/WARM/COLD tier storage
    
    // Tier-specific configurations
    struct TierConfig {
        uint32_t compression_level;     // Compression level for this tier
        Duration retention_period;      // How long to keep data in this tier (milliseconds)
        bool allow_mmap;                // Allow memory mapping for this tier
        size_t cache_size_bytes;        // Cache size for this tier
    };
    
    TierConfig hot_tier_config;         // Configuration for HOT tier (recent data)
    TierConfig warm_tier_config;        // Configuration for WARM tier (compressed)
    TierConfig cold_tier_config;        // Configuration for COLD tier (archived)
    
    // Block management policies
    Duration promotion_threshold;       // Time before promoting block to next tier
    Duration demotion_threshold;        // Time before demoting block to lower tier
    size_t compaction_threshold_blocks; // Number of blocks to trigger compaction
    double compaction_threshold_ratio;  // Size ratio to trigger compaction
    
    // Default constructor
    BlockConfig() : max_block_size(0), max_block_records(0), block_duration(0),
                   max_concurrent_compactions(0), enable_multi_tier_storage(false),
                   promotion_threshold(0), demotion_threshold(0), 
                   compaction_threshold_blocks(0), compaction_threshold_ratio(0.0) {}
    
    static BlockConfig Default() {
        BlockConfig config;
        config.max_block_size = 64 * 1024 * 1024;          // 64MB blocks
        config.max_block_records = 1000000;                // 1M records per block
        config.block_duration = 3600 * 1000;               // 1 hour blocks
        config.max_concurrent_compactions = 2;             // 2 concurrent compactions
        config.enable_multi_tier_storage = true;           // Enable tiered storage
        
        // HOT tier: Recent data, no compression, fast access
        config.hot_tier_config.compression_level = 0;      // No compression
        config.hot_tier_config.retention_period = 24 * 3600 * 1000;  // 24 hours
        config.hot_tier_config.allow_mmap = true;          // Allow memory mapping
        config.hot_tier_config.cache_size_bytes = 512 * 1024 * 1024;  // 512MB cache
        
        // WARM tier: Compressed data, moderate access
        config.warm_tier_config.compression_level = 6;     // Medium compression
        config.warm_tier_config.retention_period = 7 * 24 * 3600 * 1000;  // 7 days
        config.warm_tier_config.allow_mmap = true;         // Allow memory mapping
        config.warm_tier_config.cache_size_bytes = 256 * 1024 * 1024;  // 256MB cache
        
        // COLD tier: Highly compressed, archived data
        config.cold_tier_config.compression_level = 9;     // High compression
        config.cold_tier_config.retention_period = 365LL * 24 * 3600 * 1000;  // 1 year
        config.cold_tier_config.allow_mmap = false;        // No memory mapping
        config.cold_tier_config.cache_size_bytes = 64 * 1024 * 1024;   // 64MB cache
        
        // Block management policies
        config.promotion_threshold = 6 * 3600 * 1000;      // 6 hours to promote
        config.demotion_threshold = 48 * 3600 * 1000;      // 48 hours to demote
        config.compaction_threshold_blocks = 10;           // Compact after 10 blocks
        config.compaction_threshold_ratio = 0.3;           // Compact if 30% fragmented
        
        return config;
    }
};

/**
 * @brief Configuration for background processing tasks
 */
struct BackgroundConfig {
    bool enable_background_processing;      // Enable background processing
    size_t background_threads;              // Number of background threads
    std::chrono::milliseconds task_interval;    // Interval between background tasks
    std::chrono::milliseconds compaction_interval;  // Interval for compaction tasks
    std::chrono::milliseconds cleanup_interval;     // Interval for cleanup tasks
    std::chrono::milliseconds metrics_interval;     // Interval for metrics collection
    bool enable_auto_compaction;            // Enable automatic compaction
    bool enable_auto_cleanup;               // Enable automatic cleanup
    bool enable_metrics_collection;         // Enable background metrics collection
    
    // Default constructor
    BackgroundConfig() : enable_background_processing(false),  // DISABLED BY DEFAULT FOR TESTING
                        background_threads(2),
                        task_interval(std::chrono::milliseconds(1000)),
                        compaction_interval(std::chrono::milliseconds(60000)),
                        cleanup_interval(std::chrono::milliseconds(300000)),
                        metrics_interval(std::chrono::milliseconds(10000)),
                        enable_auto_compaction(false),  // DISABLED BY DEFAULT FOR TESTING
                        enable_auto_cleanup(false),     // DISABLED BY DEFAULT FOR TESTING
                        enable_metrics_collection(false) {}  // DISABLED BY DEFAULT FOR TESTING
    
    static BackgroundConfig Default() {
        BackgroundConfig config;
        config.enable_background_processing = true;
        config.background_threads = 2;
        config.task_interval = std::chrono::milliseconds(1000);
        config.compaction_interval = std::chrono::milliseconds(10000);    // 10 seconds (was 60s)
        config.cleanup_interval = std::chrono::milliseconds(300000);      // 5 minutes
        config.metrics_interval = std::chrono::milliseconds(10000);       // 10 seconds
        config.enable_auto_compaction = true;
        config.enable_auto_cleanup = true;
        config.enable_metrics_collection = true;
        return config;
    }
};

/**
 * @brief Configuration for storage engine
 */
struct StorageConfig {
    std::string data_dir;           // Directory for data files
    size_t block_size;              // Size of storage blocks in bytes
    size_t max_blocks_per_series;   // Maximum number of blocks per series
    size_t cache_size_bytes;        // Size of block cache in bytes
    Duration block_duration;        // Duration covered by each block
    Duration retention_period;      // How long to retain data
    bool enable_compression;        // Whether to enable compression
    ObjectPoolConfig object_pool_config;  // Object pool configuration
    CompressionConfig compression_config; // Compression configuration
    BlockConfig block_config;       // Block management configuration
    BackgroundConfig background_config;   // Background processing configuration
    
    // Default constructor
    StorageConfig() : block_size(0), max_blocks_per_series(0), cache_size_bytes(0), 
                     block_duration(0), retention_period(0), enable_compression(false),
                     object_pool_config(ObjectPoolConfig::Default()),
                     compression_config(CompressionConfig::Default()),
                     block_config(BlockConfig::Default()),
                     background_config(BackgroundConfig::Default()) {}
    
    static StorageConfig Default() {
        StorageConfig config;
        config.data_dir = "data";
        config.block_size = 64 * 1024 * 1024;     // 64MB blocks
        config.max_blocks_per_series = 1024;       // Max blocks per series
        config.cache_size_bytes = 1024 * 1024 * 1024;   // 1GB cache
        config.block_duration = 3600 * 1000;       // 1 hour blocks
        config.retention_period = 7 * 24 * 3600 * 1000; // 1 week retention
        config.enable_compression = true;          // Enable compression
        config.compression_config = CompressionConfig::Default();
        config.block_config = BlockConfig::Default();
        config.background_config = BackgroundConfig::Default();
        return config;
    }
};

/**
 * @brief Configuration for query engine
 */
struct QueryConfig {
    size_t max_concurrent_queries;    // Maximum number of concurrent queries
    Duration query_timeout;           // Query timeout in milliseconds
    size_t max_samples_per_query;     // Maximum samples returned per query
    size_t max_series_per_query;      // Maximum series returned per query
    
    // Default constructor
    QueryConfig() : max_concurrent_queries(0), query_timeout(0), 
                   max_samples_per_query(0), max_series_per_query(0) {}
    
    static QueryConfig Default() {
        QueryConfig config;
        config.max_concurrent_queries = 100;        // Max concurrent queries
        config.query_timeout = 30 * 1000;          // 30 second timeout
        config.max_samples_per_query = 1000000;    // Max 1M samples
        config.max_series_per_query = 10000;       // Max 10K series
        return config;
    }
};

/**
 * @brief Configuration for semantic vector features
 * 
 * This extends the main configuration with semantic vector capabilities
 * while maintaining backward compatibility with existing configurations.
 */
struct SemanticVectorFeatureConfig {
    bool enable_semantic_vector_features;  // Master switch for semantic vector features
    semantic_vector::SemanticVectorConfig semantic_vector_config;  // Full semantic vector configuration
    
    // Default constructor
    SemanticVectorFeatureConfig() : enable_semantic_vector_features(false) {}
    
    static SemanticVectorFeatureConfig Default() {
        SemanticVectorFeatureConfig config;
        config.enable_semantic_vector_features = false;  // Disabled by default for backward compatibility
        config.semantic_vector_config = semantic_vector::SemanticVectorConfig();  // Default configuration
        return config;
    }
    
    static SemanticVectorFeatureConfig Enabled() {
        SemanticVectorFeatureConfig config;
        config.enable_semantic_vector_features = true;
        config.semantic_vector_config = semantic_vector::SemanticVectorConfig::balanced_config();
        return config;
    }
    
    static SemanticVectorFeatureConfig HighPerformance() {
        SemanticVectorFeatureConfig config;
        config.enable_semantic_vector_features = true;
        config.semantic_vector_config = semantic_vector::SemanticVectorConfig::high_performance_config();
        return config;
    }
    
    static SemanticVectorFeatureConfig MemoryEfficient() {
        SemanticVectorFeatureConfig config;
        config.enable_semantic_vector_features = true;
        config.semantic_vector_config = semantic_vector::SemanticVectorConfig::memory_efficient_config();
        return config;
    }
    
    static SemanticVectorFeatureConfig HighAccuracy() {
        SemanticVectorFeatureConfig config;
        config.enable_semantic_vector_features = true;
        config.semantic_vector_config = semantic_vector::SemanticVectorConfig::high_accuracy_config();
        return config;
    }
    
    // Validation method
    bool is_valid() const {
        if (!enable_semantic_vector_features) {
            return true;  // Valid if features are disabled
        }
        return semantic_vector_config.is_valid();
    }
    
    // Migration utilities for backward compatibility
    static SemanticVectorFeatureConfig migrate_from_legacy_config(const std::string& legacy_config_json);
    std::string to_legacy_config_json() const;
};

/**
 * @brief Global configuration for the TSDB
 */
class Config {
public:
    Config() = default;
    
    // Existing configuration accessors (backward compatibility)
    const StorageConfig& storage() const { return storage_; }
    StorageConfig& mutable_storage() { return storage_; }
    
    const QueryConfig& query() const { return query_; }
    QueryConfig& mutable_query() { return query_; }
    
    const HistogramConfig& histogram() const { return histogram_; }
    HistogramConfig& mutable_histogram() { return histogram_; }
    
    const Granularity& granularity() const { return granularity_; }
    Granularity& mutable_granularity() { return granularity_; }
    
    // New semantic vector configuration accessors
    const SemanticVectorFeatureConfig& semantic_vector() const { return semantic_vector_; }
    SemanticVectorFeatureConfig& mutable_semantic_vector() { return semantic_vector_; }
    
    // Check if semantic vector features are enabled
    bool semantic_vector_enabled() const { return semantic_vector_.enable_semantic_vector_features; }
    
    // Get semantic vector configuration (only if enabled)
    const semantic_vector::SemanticVectorConfig& semantic_vector_config() const {
        return semantic_vector_.semantic_vector_config;
    }
    
    // Enable semantic vector features with specific configuration
    void enable_semantic_vector(const semantic_vector::SemanticVectorConfig& config = semantic_vector::SemanticVectorConfig::balanced_config()) {
        semantic_vector_.enable_semantic_vector_features = true;
        semantic_vector_.semantic_vector_config = config;
    }
    
    // Disable semantic vector features
    void disable_semantic_vector() {
        semantic_vector_.enable_semantic_vector_features = false;
    }
    
    // Configuration validation
    bool is_valid() const {
        // Validate existing configurations
        if (!storage_.data_dir.empty() && storage_.block_size == 0) {
            return false;  // Invalid storage configuration
        }
        if (query_.max_concurrent_queries == 0) {
            return false;  // Invalid query configuration
        }
        
        // Validate semantic vector configuration if enabled
        if (semantic_vector_enabled() && !semantic_vector_.is_valid()) {
            return false;
        }
        
        return true;
    }
    
    // Configuration migration utilities
    static Config migrate_from_legacy_config(const std::string& legacy_config_json);
    std::string to_legacy_config_json() const;
    
    // Configuration examples for different deployment scenarios
    static Config Default() {
        Config config;
        config.storage_ = StorageConfig::Default();
        config.query_ = QueryConfig::Default();
        config.histogram_ = HistogramConfig::Default();
        config.granularity_ = Granularity::Normal();
        config.semantic_vector_ = SemanticVectorFeatureConfig::Default();  // Disabled by default
        return config;
    }
    
    static Config WithSemanticVector() {
        Config config = Default();
        config.enable_semantic_vector();  // Enable with balanced configuration
        return config;
    }
    
    static Config HighPerformance() {
        Config config = Default();
        config.enable_semantic_vector(semantic_vector::SemanticVectorConfig::high_performance_config());
        return config;
    }
    
    static Config MemoryEfficient() {
        Config config = Default();
        config.enable_semantic_vector(semantic_vector::SemanticVectorConfig::memory_efficient_config());
        return config;
    }
    
    static Config HighAccuracy() {
        Config config = Default();
        config.enable_semantic_vector(semantic_vector::SemanticVectorConfig::high_accuracy_config());
        return config;
    }
    
    static Config Production() {
        Config config = Default();
        config.enable_semantic_vector(semantic_vector::SemanticVectorConfig::production_config());
        return config;
    }
    
    static Config Development() {
        Config config = Default();
        config.enable_semantic_vector(semantic_vector::SemanticVectorConfig::development_config());
        return config;
    }

private:
    StorageConfig storage_;
    QueryConfig query_;
    HistogramConfig histogram_;
    Granularity granularity_;
    SemanticVectorFeatureConfig semantic_vector_;  // New semantic vector configuration
};

// ============================================================================
// CONFIGURATION UTILITIES AND DOCUMENTATION
// ============================================================================

/**
 * @brief Configuration utilities for semantic vector integration
 * 
 * Provides utilities for migrating from legacy configurations,
 * validating configurations, and managing configuration updates.
 */
namespace config_utils {

/**
 * @brief Configuration migration utilities
 * 
 * Helps migrate from legacy configurations to the new semantic vector
 * enabled configuration while maintaining backward compatibility.
 */
class ConfigMigration {
public:
    // Migrate from legacy JSON configuration
    static Result<Config> migrate_from_json(const std::string& legacy_json);
    
    // Migrate from legacy configuration file
    static Result<Config> migrate_from_file(const std::string& legacy_file_path);
    
    // Export current configuration to legacy format
    static Result<std::string> export_to_legacy_json(const Config& config);
    
    // Validate migration compatibility
    static bool is_migration_compatible(const std::string& legacy_config);
    
    // Get migration warnings and recommendations
    static std::vector<std::string> get_migration_warnings(const std::string& legacy_config);
};

/**
 * @brief Configuration validation utilities
 * 
 * Provides comprehensive validation for configurations including
 * cross-component validation and performance target validation.
 */
class ConfigValidation {
public:
    // Validate complete configuration
    static semantic_vector::ConfigValidationResult validate_config(const Config& config);
    
    // Validate semantic vector specific configuration
    static semantic_vector::ConfigValidationResult validate_semantic_vector_config(const Config& config);
    
    // Check configuration performance targets
    static bool validate_performance_targets(const Config& config);
    
    // Check resource requirements
    static bool validate_resource_requirements(const Config& config);
    
    // Get configuration recommendations
    static std::vector<std::string> get_configuration_recommendations(const Config& config);
};

/**
 * @brief Configuration optimization utilities
 * 
 * Provides automatic configuration optimization based on
 * system resources and performance requirements.
 */
class ConfigOptimization {
public:
    // Optimize configuration for current system resources
    static Config optimize_for_system(const Config& config);
    
    // Optimize configuration for specific workload
    static Config optimize_for_workload(const Config& config, const std::string& workload_type);
    
    // Optimize configuration for memory constraints
    static Config optimize_for_memory(const Config& config, size_t max_memory_mb);
    
    // Optimize configuration for performance requirements
    static Config optimize_for_performance(const Config& config, double target_latency_ms);
    
    // Get optimization recommendations
    static std::vector<std::string> get_optimization_recommendations(const Config& config);
};

} // namespace config_utils

// ============================================================================
// CONFIGURATION EXAMPLES AND BEST PRACTICES
// ============================================================================

/**
 * @brief Configuration examples for common deployment scenarios
 * 
 * Provides pre-configured configurations for different deployment
 * scenarios and use cases.
 */
namespace config_examples {

/**
 * @brief Development environment configuration
 * 
 * Optimized for development with semantic vector features enabled
 * and comprehensive logging for debugging.
 */
inline Config DevelopmentConfig() {
    Config config = Config::Development();
    
    // Enable debug logging
    config.mutable_storage().data_dir = "./data_dev";
    config.mutable_query().max_concurrent_queries = 10;  // Lower for development
    
    return config;
}

/**
 * @brief Production environment configuration
 * 
 * Optimized for production with semantic vector features enabled
 * and performance-focused settings.
 */
inline Config ProductionConfig() {
    Config config = Config::Production();
    
    // Production-optimized settings
    config.mutable_storage().cache_size_bytes = 2LL * 1024 * 1024 * 1024;  // 2GB cache
    config.mutable_query().max_concurrent_queries = 200;  // Higher for production
    
    return config;
}

/**
 * @brief High-performance configuration
 * 
 * Optimized for maximum performance with semantic vector features
 * and aggressive caching and parallelization.
 */
inline Config HighPerformanceConfig() {
    Config config = Config::HighPerformance();
    
    // Performance-optimized settings
    config.mutable_storage().cache_size_bytes = 4LL * 1024 * 1024 * 1024;  // 4GB cache
    config.mutable_query().max_concurrent_queries = 500;  // High concurrency
    
    return config;
}

/**
 * @brief Memory-efficient configuration
 * 
 * Optimized for memory-constrained environments with semantic vector
 * features and aggressive memory optimization.
 */
inline Config MemoryEfficientConfig() {
    Config config = Config::MemoryEfficient();
    
    // Memory-optimized settings
    config.mutable_storage().cache_size_bytes = 512 * 1024 * 1024;  // 512MB cache
    config.mutable_query().max_concurrent_queries = 50;  // Lower concurrency
    
    return config;
}

/**
 * @brief Balanced configuration
 * 
 * Balanced configuration with semantic vector features enabled
 * and moderate resource usage.
 */
inline Config BalancedConfig() {
    Config config = Config::WithSemanticVector();
    
    // Balanced settings
    config.mutable_storage().cache_size_bytes = 1024 * 1024 * 1024;  // 1GB cache
    config.mutable_query().max_concurrent_queries = 100;  // Moderate concurrency
    
    return config;
}

} // namespace config_examples

} // namespace core
} // namespace tsdb 