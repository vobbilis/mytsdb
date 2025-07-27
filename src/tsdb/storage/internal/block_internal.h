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



} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_BLOCK_INTERNAL_H_ 