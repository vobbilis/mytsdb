#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <memory>

#include "tsdb/core/types.h"

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
    
    // Default constructor
    StorageConfig() : block_size(0), max_blocks_per_series(0), cache_size_bytes(0), 
                     block_duration(0), retention_period(0), enable_compression(false) {}
    
    static StorageConfig Default() {
        StorageConfig config;
        config.data_dir = "data";
        config.block_size = 64 * 1024 * 1024;     // 64MB blocks
        config.max_blocks_per_series = 1024;       // Max blocks per series
        config.cache_size_bytes = 1024 * 1024 * 1024;   // 1GB cache
        config.block_duration = 3600 * 1000;       // 1 hour blocks
        config.retention_period = 7 * 24 * 3600 * 1000; // 1 week retention
        config.enable_compression = true;          // Enable compression
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
 * @brief Global configuration for the TSDB
 */
class Config {
public:
    Config() = default;
    
    const StorageConfig& storage() const { return storage_; }
    StorageConfig& mutable_storage() { return storage_; }
    
    const QueryConfig& query() const { return query_; }
    QueryConfig& mutable_query() { return query_; }
    
    const HistogramConfig& histogram() const { return histogram_; }
    HistogramConfig& mutable_histogram() { return histogram_; }
    
    const Granularity& granularity() const { return granularity_; }
    Granularity& mutable_granularity() { return granularity_; }
    
    static Config Default() {
        Config config;
        config.storage_ = StorageConfig::Default();
        config.query_ = QueryConfig::Default();
        config.histogram_ = HistogramConfig::Default();
        config.granularity_ = Granularity::Normal();
        return config;
    }

private:
    StorageConfig storage_;
    QueryConfig query_;
    HistogramConfig histogram_;
    Granularity granularity_;
};

} // namespace core
} // namespace tsdb 