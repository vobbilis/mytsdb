#ifndef TSDB_STORAGE_BLOCK_H_
#define TSDB_STORAGE_BLOCK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

/**
 * @brief Block header structure
 */
struct BlockHeader {
    static constexpr uint64_t MAGIC = 0x4253445354534254;  // "TBSTDSB"
    static constexpr uint32_t VERSION = 1;
    
    uint64_t magic;      // Magic number for validation
    uint32_t version;    // Block format version
    uint32_t flags;      // Block flags (compression, etc.)
    uint32_t crc32;      // CRC32 of block data
    int64_t start_time;  // Block start time
    int64_t end_time;    // Block end time
    uint32_t reserved;   // Reserved for future use
    
    bool is_valid() const {
        return magic == MAGIC && version == VERSION;
    }
};

/**
 * @brief Block flags
 */
enum class BlockFlags : uint32_t {
    NONE = 0,
    COMPRESSED = 1 << 0,
    SORTED = 1 << 1,
    CHECKSUM = 1 << 2
};

/**
 * @brief Public interface for time series data blocks
 * 
 * A Block represents an immutable collection of time series data
 * over a specific time range. It provides read-only access to the data
 * and supports querying by labels and time range.
 */
class Block {
public:
    virtual ~Block() = default;
    
    /**
     * @brief Get block size in bytes
     * @return Total size of the block in bytes
     */
    virtual size_t size() const = 0;
    
    /**
     * @brief Get number of series in block
     * @return Number of unique time series in the block
     */
    virtual size_t num_series() const = 0;
    
    /**
     * @brief Get number of samples in block
     * @return Total number of samples across all series
     */
    virtual size_t num_samples() const = 0;
    
    /**
     * @brief Get start time of the block
     * @return Start timestamp in milliseconds since epoch
     */
    virtual int64_t start_time() const = 0;
    
    /**
     * @brief Get end time of the block
     * @return End timestamp in milliseconds since epoch
     */
    virtual int64_t end_time() const = 0;
    
    /**
     * @brief Read time series from block
     * @param labels Labels to identify the series
     * @return Time series data matching the labels
     */
    virtual core::TimeSeries read(const core::Labels& labels) const = 0;
    
    /**
     * @brief Query time series in block
     * @param matchers Label matchers to filter series
     * @param start_time Start time in milliseconds since epoch
     * @param end_time End time in milliseconds since epoch
     * @return Vector of time series matching the query
     */
    virtual std::vector<core::TimeSeries> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time,
        int64_t end_time) const = 0;
    
    /**
     * @brief Flush block to disk
     */
    virtual void flush() = 0;
    
    /**
     * @brief Close block
     */
    virtual void close() = 0;
};

/**
 * @brief Block reader interface
 */
class BlockReader {
public:
    virtual ~BlockReader() = default;
    
    /**
     * @brief Read block from file
     */
    virtual std::unique_ptr<Block> read(const std::string& path) = 0;
};

/**
 * @brief Block writer interface
 */
class BlockWriter {
public:
    virtual ~BlockWriter() = default;
    
    /**
     * @brief Write block to file
     */
    virtual void write(const std::string& path, const Block& block) = 0;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_BLOCK_H_ 