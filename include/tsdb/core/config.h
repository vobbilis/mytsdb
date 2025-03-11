#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <memory>

#include "tsdb/core/types.h"

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
    
    static Granularity HighFrequency() {
        return {
            GranularityType::HIGH_FREQUENCY,
            100'000,           // 100 microseconds
            86'400'000        // 24 hours in milliseconds
        };
    }
    
    static Granularity Normal() {
        return {
            GranularityType::NORMAL,
            1'000,            // 1 second
            604'800'000      // 1 week in milliseconds
        };
    }
    
    static Granularity LowFrequency() {
        return {
            GranularityType::LOW_FREQUENCY,
            60'000,           // 1 minute
            31'536'000'000   // 1 year in milliseconds
        };
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
    
    static HistogramConfig Default() {
        return {
            0.01,    // 1% relative accuracy
            2048,    // Maximum buckets
            false,   // Use DDSketch by default
            {}       // No fixed bounds
        };
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
    
    static StorageConfig Default() {
        return {
            "data",
            64 * 1024 * 1024,     // 64MB blocks
            1024,                  // Max blocks per series
            1024 * 1024 * 1024,   // 1GB cache
            3600 * 1000,          // 1 hour blocks
            7 * 24 * 3600 * 1000, // 1 week retention
            true                   // Enable compression
        };
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
    
    static QueryConfig Default() {
        return {
            100,                    // Max concurrent queries
            30 * 1000,             // 30 second timeout
            1000000,               // Max 1M samples
            10000                  // Max 10K series
        };
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