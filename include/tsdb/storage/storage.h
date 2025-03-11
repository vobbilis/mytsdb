#ifndef TSDB_STORAGE_STORAGE_H_
#define TSDB_STORAGE_STORAGE_H_

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/core/error.h"
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {

/**
 * @brief Storage options for configuring the storage engine
 */
struct StorageOptions {
    std::string data_dir;           // Directory for storing data files
    size_t max_block_size;         // Maximum size of a block in bytes
    size_t max_block_records;      // Maximum number of records in a block
    std::chrono::seconds block_duration;  // Duration after which to seal a block
    size_t max_concurrent_compactions;  // Maximum number of concurrent compactions
    
    static StorageOptions Default() {
        return {
            "data",                // Default data directory
            64 * 1024 * 1024,     // 64MB max block size
            1000000,              // 1M records per block
            std::chrono::hours(2), // 2 hour block duration
            2                      // 2 concurrent compactions
        };
    }
};

/**
 * @brief Storage engine interface
 */
class Storage {
public:
    virtual ~Storage() = default;
    
    /**
     * @brief Initialize storage with configuration
     */
    virtual core::Result<void> init(const core::StorageConfig& config) = 0;
    
    /**
     * @brief Write time series data
     */
    virtual core::Result<void> write(const core::TimeSeries& series) = 0;
    
    /**
     * @brief Read time series data
     */
    virtual core::Result<core::TimeSeries> read(
        const core::Labels& labels,
        int64_t start_time,
        int64_t end_time) = 0;
    
    /**
     * @brief Query time series data
     */
    virtual core::Result<std::vector<core::TimeSeries>> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time,
        int64_t end_time) = 0;
    
    /**
     * @brief Get label names
     */
    virtual core::Result<std::vector<std::string>> label_names() = 0;
    
    /**
     * @brief Get label values for a given label name
     */
    virtual core::Result<std::vector<std::string>> label_values(
        const std::string& label_name) = 0;
    
    /**
     * @brief Delete time series data
     */
    virtual core::Result<void> delete_series(
        const std::vector<std::pair<std::string, std::string>>& matchers) = 0;
    
    /**
     * @brief Compact storage
     */
    virtual core::Result<void> compact() = 0;
    
    /**
     * @brief Flush storage
     */
    virtual core::Result<void> flush() = 0;
    
    /**
     * @brief Close storage
     */
    virtual core::Result<void> close() = 0;
    
    /**
     * @brief Get storage statistics
     */
    virtual std::string stats() const = 0;
};

/**
 * @brief Factory for creating storage instances
 */
class StorageFactory {
public:
    virtual ~StorageFactory() = default;
    
    /**
     * @brief Create storage instance
     */
    virtual std::unique_ptr<Storage> create(const core::StorageConfig& config) = 0;
};

/**
 * @brief Create a new storage instance with the given options
 */
std::shared_ptr<Storage> CreateStorage(const StorageOptions& options);

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_STORAGE_H_ 