#ifndef TSDB_STORAGE_BLOCK_INTERNAL_H_
#define TSDB_STORAGE_BLOCK_INTERNAL_H_

#include <memory>
#include <string>
#include <vector>
#include "tsdb/storage/block.h"

namespace tsdb {
namespace storage {

/**
 * @brief Internal interface for block operations
 */
class BlockInternal : public Block {
public:
    /**
     * @brief Write time series to block
     */
    virtual void write(const core::TimeSeries& series) = 0;
    
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
    virtual std::unique_ptr<BlockInternal> read(const std::string& path) = 0;
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
    virtual void write(const std::string& path, const BlockInternal& block) = 0;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_BLOCK_INTERNAL_H_ 