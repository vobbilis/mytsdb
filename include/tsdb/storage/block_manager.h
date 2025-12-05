#ifndef TSDB_STORAGE_INTERNAL_BLOCK_MANAGER_H_
#define TSDB_STORAGE_INTERNAL_BLOCK_MANAGER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <shared_mutex>
#include <map>

#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include "tsdb/storage/internal/block_types.h"
#include "tsdb/storage/internal/block_impl.h"
#include "tsdb/storage/parquet/writer.hpp"
#include "tsdb/storage/parquet/reader.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"

namespace tsdb {
namespace storage {

/**
 * @brief Interface for block storage operations
 */
class BlockStorage {
public:
    virtual ~BlockStorage() = default;

    /**
     * @brief Write a block to storage
     */
    virtual core::Result<void> write(
        const internal::BlockHeader& header,
        const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Read a block from storage
     */
    virtual core::Result<std::vector<uint8_t>> read(
        const internal::BlockHeader& header) = 0;

    /**
     * @brief Remove a block from storage
     */
    virtual core::Result<void> remove(
        const internal::BlockHeader& header) = 0;
};

/**
 * @brief Manages block lifecycle and storage operations
 */
class BlockManager {
public:
    explicit BlockManager(const std::string& data_dir);
    ~BlockManager() = default;

    /**
     * @brief Create a new block
     */
    /**
     * @brief Create a new block
     */
    core::Result<internal::BlockHeader> createBlock(int64_t start_time, int64_t end_time);

    /**
     * @brief Finalize a block for reading
     */
    core::Result<void> finalizeBlock(const internal::BlockHeader& header);

    /**
     * @brief Delete a block
     */
    core::Result<void> deleteBlock(const internal::BlockHeader& header);

    /**
     * @brief Write data to a block
     */
    core::Result<void> writeData(
        const internal::BlockHeader& header,
        const std::vector<uint8_t>& data);

    /**
     * @brief Read data from a block
     */
    core::Result<std::vector<uint8_t>> readData(
        const internal::BlockHeader& header);

    /**
     * @brief Move block to hot storage
     */
    core::Result<void> promoteBlock(const internal::BlockHeader& header);

    /**
     * @brief Move block to cold storage
     */
    core::Result<void> demoteBlock(const internal::BlockHeader& header);

    /**
     * @brief Compact blocks to optimize storage
     */
    core::Result<void> compact();

    /**
     * @brief Recover blocks from disk and populate internal state
     * @return Vector of recovered block headers
     */
    core::Result<std::vector<internal::BlockHeader>> recoverBlocks();

    /**
     * @brief Flush pending writes to disk
     */
    core::Result<void> flush();

    /**
     * @brief Move block between storage tiers
     */
    core::Result<void> moveBlock(
        const internal::BlockHeader& header,
        BlockStorage* source,
        BlockStorage* target);

    /**
     * @brief Seals and persists a block from memory to storage.
     *
     * @param block The in-memory block implementation to persist.
     * @return Result indicating success or failure.
     */
    core::Result<void> seal_and_persist_block(std::shared_ptr<internal::BlockImpl> block);

    // Demote block to Parquet (Cold Tier)
    // Returns the path to the created Parquet file
    core::Result<std::string> demoteToParquet(const internal::BlockHeader& header);
    
    // Read block from Parquet (Cold Tier)
    core::Result<std::shared_ptr<internal::BlockImpl>> readFromParquet(const internal::BlockHeader& header);

    // Compact multiple Parquet files into one
    core::Result<void> compactParquetFiles(const std::vector<std::string>& input_paths, const std::string& output_path);

private:
    std::string data_dir_;
    mutable std::shared_mutex mutex_;
    std::map<uint64_t, internal::BlockTier::Type> block_tiers_;
    
    // Storage backends for different tiers
    std::unique_ptr<BlockStorage> hot_storage_;
    std::unique_ptr<BlockStorage> warm_storage_;
    std::unique_ptr<BlockStorage> cold_storage_;

    BlockStorage* getStorageForTier(internal::BlockTier::Type tier);
    core::Result<void> moveBlock(
        const internal::BlockHeader& header,
        internal::BlockTier::Type from_tier,
        internal::BlockTier::Type to_tier);


};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_INTERNAL_BLOCK_MANAGER_H_