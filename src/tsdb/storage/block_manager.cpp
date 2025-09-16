#include "tsdb/storage/block_manager.h"
#include "tsdb/storage/internal/block_types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <mutex>
#include <system_error>
#include <map>
#include <limits>
#include <thread>
#include <chrono>
#include <iostream>

namespace tsdb {
namespace storage {

namespace fs = std::filesystem;

// Helper function to create directory if it doesn't exist
static core::Result<void> ensure_directory(const std::string& path) {
    if (path.empty()) {
        return core::Result<void>::error("Empty path provided");
    }
    
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        return core::Result<void>::error("Failed to create directory: " + ec.message());
    }
    return core::Result<void>();
}

class FileBlockStorage : public BlockStorage {
public:
    explicit FileBlockStorage(const std::string& base_path, internal::BlockTier::Type tier)
        : base_path_(base_path), tier_(tier) {}

    core::Result<void> write(const internal::BlockHeader& header, const std::vector<uint8_t>& data) override {
        std::string path = get_block_path(header);
        try {
            std::ofstream file(path, std::ios::binary);
            if (!file) {
                return core::Result<void>::error("Failed to open file for writing: " + path);
            }
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            return core::Result<void>();
        } catch (const std::exception& e) {
            return core::Result<void>::error(std::string("Write failed: ") + e.what());
        }
    }

    core::Result<std::vector<uint8_t>> read(const internal::BlockHeader& header) override {
        std::string path = get_block_path(header);
        try {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return core::Result<std::vector<uint8_t>>::error("Failed to open file: " + path);
            }

            // Skip header
            file.seekg(sizeof(internal::BlockHeader));

            // Read data
            std::vector<uint8_t> data;
            file.seekg(0, std::ios::end);
            size_t size = static_cast<size_t>(file.tellg()) - sizeof(internal::BlockHeader);
            file.seekg(sizeof(internal::BlockHeader));
            data.resize(size);
            file.read(reinterpret_cast<char*>(data.data()), size);
            return core::Result<std::vector<uint8_t>>(std::move(data));
        } catch (const std::exception& e) {
            return core::Result<std::vector<uint8_t>>::error(std::string("Read failed: ") + e.what());
        }
    }

    core::Result<void> remove(const internal::BlockHeader& header) override {
        std::string path = get_block_path(header);
        try {
            if (fs::remove(path)) {
                return core::Result<void>();
            }
            return core::Result<void>::error("Failed to remove file: " + path);
        } catch (const std::exception& e) {
            return core::Result<void>::error(std::string("Remove failed: ") + e.what());
        }
    }

    core::Result<void> flush() {
        // FileBlockStorage writes directly to disk, so no additional flushing needed
        return core::Result<void>();
    }

private:
    std::string get_block_path(const internal::BlockHeader& header) const {
        std::stringstream ss;
        ss << base_path_ << "/"
           << static_cast<int>(tier_) << "/"
           << std::hex << header.magic << ".block";
        return ss.str();
    }

    std::string base_path_;
    internal::BlockTier::Type tier_;
};

BlockManager::BlockManager(const std::string& data_dir)
    : data_dir_(data_dir)
    , mutex_()
    , block_tiers_() {
    if (data_dir_.empty()) {
        throw std::invalid_argument("Data directory path cannot be empty");
    }

    // Create storage directories
    auto hot_result = ensure_directory(data_dir_ + "/0");  // HOT
    auto warm_result = ensure_directory(data_dir_ + "/1");  // WARM
    auto cold_result = ensure_directory(data_dir_ + "/2");  // COLD

    if (!hot_result.ok() || !warm_result.ok() || !cold_result.ok()) {
        throw std::runtime_error("Failed to create storage directories");
    }

    // Initialize storage backends
    try {
        hot_storage_ = std::make_unique<FileBlockStorage>(
            data_dir_, internal::BlockTier::Type::HOT);
        warm_storage_ = std::make_unique<FileBlockStorage>(
            data_dir_, internal::BlockTier::Type::WARM);
        cold_storage_ = std::make_unique<FileBlockStorage>(
            data_dir_, internal::BlockTier::Type::COLD);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize storage backends: " + std::string(e.what()));
    }
}

core::Result<void> BlockManager::createBlock(int64_t start_time, int64_t end_time) {
    if (start_time > end_time) {
        return core::Result<void>::error("Invalid time range: start_time > end_time");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        internal::BlockHeader header;
        header.magic = internal::BlockHeader::MAGIC;
        header.version = internal::BlockHeader::VERSION;
        header.start_time = start_time;
        header.end_time = end_time;
        header.flags = 0;
        header.crc32 = 0;  // Will be set when block is finalized
        header.reserved = 0;     // Initialize reserved field
        
        // Check if we need to demote blocks
        if (!hot_storage_) {
            return core::Result<void>::error("Hot storage not initialized");
        }

        // Create empty block in hot tier (with retry logic)
        std::vector<uint8_t> empty_data;
        int retry_count = 0;
        const int max_retries = 3;
        
        while (retry_count < max_retries) {
            auto result = hot_storage_->write(header, empty_data);
            if (result.ok()) {
                block_tiers_[header.magic] = internal::BlockTier::Type::HOT;
                return core::Result<void>();
            }
            
            retry_count++;
            if (retry_count < max_retries) {
                // Brief delay before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                return result; // Return the last error
            }
        }
        
        return core::Result<void>::error("Failed to create block after retries");
    } catch (const std::exception& e) {
        return core::Result<void>::error("Block creation exception: " + std::string(e.what()));
    }
}

core::Result<void> BlockManager::finalizeBlock(const internal::BlockHeader& header) {
    if (!header.is_valid()) {
        return core::Result<void>::error("Invalid block header");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.magic);
    if (it == block_tiers_.end()) {
        return core::Result<void>::error("Block not found");
    }
    
    auto storage = getStorageForTier(it->second);
    if (!storage) {
        return core::Result<void>::error("Storage tier not initialized");
    }

    // Mark block as finalized by setting the CHECKSUM flag
    internal::BlockHeader new_header = header;
    new_header.flags |= static_cast<uint32_t>(internal::BlockFlags::CHECKSUM);
    
    // Read existing data
    auto read_result = storage->read(header);
    if (!read_result.ok()) {
        return core::Result<void>::error(read_result.error());
    }
    
    // Write back with new header
    return storage->write(new_header, read_result.value());
}

core::Result<void> BlockManager::deleteBlock(const internal::BlockHeader& header) {
    if (!header.is_valid()) {
        return core::Result<void>::error("Invalid block header");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.magic);
    if (it == block_tiers_.end()) {
        return core::Result<void>::error("Block not found");
    }
    
    auto storage = getStorageForTier(it->second);
    if (!storage) {
        return core::Result<void>::error("Storage tier not initialized");
    }

    auto result = storage->remove(header);
    if (result.ok()) {
        block_tiers_.erase(it);
    }
    return result;
}

core::Result<void> BlockManager::writeData(
    const internal::BlockHeader& header,
    const std::vector<uint8_t>& data) {
    if (!header.is_valid()) {
        return core::Result<void>::error("Invalid block header");
    }
    if (data.empty()) {
        return core::Result<void>::error("Empty data provided");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.magic);
    if (it == block_tiers_.end()) {
        return core::Result<void>::error("Block not found");
    }
    
    auto storage = getStorageForTier(it->second);
    if (!storage) {
        return core::Result<void>::error("Storage tier not initialized");
    }

    return storage->write(header, data);
}

core::Result<std::vector<uint8_t>> BlockManager::readData(
    const internal::BlockHeader& header) {
    if (!header.is_valid()) {
        return core::Result<std::vector<uint8_t>>::error("Invalid block header");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.magic);
    if (it == block_tiers_.end()) {
        return core::Result<std::vector<uint8_t>>::error("Block not found");
    }
    
    auto storage = getStorageForTier(it->second);
    if (!storage) {
        return core::Result<std::vector<uint8_t>>::error("Storage tier not initialized");
    }

    return storage->read(header);
}

core::Result<void> BlockManager::promoteBlock(const internal::BlockHeader& header) {
    if (!header.is_valid()) {
        return core::Result<void>::error("Invalid block header");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.magic);
    if (it == block_tiers_.end()) {
        return core::Result<void>::error("Block not found");
    }
    
    if (it->second == internal::BlockTier::Type::HOT) {
        return core::Result<void>();  // Already in hot tier
    }
    
    internal::BlockTier::Type target_tier;
    if (it->second == internal::BlockTier::Type::COLD) {
        target_tier = internal::BlockTier::Type::WARM;
    } else {
        target_tier = internal::BlockTier::Type::HOT;
    }
    
    return moveBlock(header, it->second, target_tier);
}

core::Result<void> BlockManager::demoteBlock(const internal::BlockHeader& header) {
    if (!header.is_valid()) {
        return core::Result<void>::error("Invalid block header");
    }

    // Note: No lock needed here since this method is called from compact() which already holds the lock
    auto it = block_tiers_.find(header.magic);
    if (it == block_tiers_.end()) {
        return core::Result<void>::error("Block not found");
    }
    
    if (it->second == internal::BlockTier::Type::COLD) {
        return core::Result<void>();  // Already in cold tier
    }
    
    internal::BlockTier::Type target_tier;
    if (it->second == internal::BlockTier::Type::HOT) {
        target_tier = internal::BlockTier::Type::WARM;
    } else {
        target_tier = internal::BlockTier::Type::COLD;
    }
    
    return moveBlock(header, it->second, target_tier);
}

core::Result<void> BlockManager::moveBlock(
    const internal::BlockHeader& header,
    internal::BlockTier::Type from_tier,
    internal::BlockTier::Type to_tier) {
    if (!header.is_valid()) {
        return core::Result<void>::error("Invalid block header");
    }

    auto from_storage = getStorageForTier(from_tier);
    auto to_storage = getStorageForTier(to_tier);
    
    if (!from_storage || !to_storage) {
        return core::Result<void>::error("Invalid storage tier");
    }
    
    // Read existing data
    auto read_result = from_storage->read(header);
    if (!read_result.ok()) {
        return core::Result<void>::error(read_result.error());
    }
    
    // Write back with new header
    auto write_result = to_storage->write(header, read_result.value());
    if (!write_result.ok()) {
        return write_result;
    }
    
    // Remove from source
    auto remove_result = from_storage->remove(header);
    if (!remove_result.ok()) {
        // Try to clean up destination if source removal fails
        to_storage->remove(header);
        return remove_result;
    }
    
    block_tiers_[header.magic] = to_tier;
    return core::Result<void>();
}

core::Result<void> BlockManager::compact() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // For now, just demote all blocks in hot tier to warm tier
    std::vector<internal::BlockHeader> to_demote;
    
    // Find all blocks in hot tier
    for (const auto& [magic, tier] : block_tiers_) {
        if (tier == internal::BlockTier::Type::HOT) {
            internal::BlockHeader header;
            header.magic = magic;
            header.version = internal::BlockHeader::VERSION;
            header.flags = 0;
            header.crc32 = 0;
            header.start_time = 0;
            header.end_time = 0;
            header.reserved = 0;
            to_demote.push_back(header);
        }
    }
    
    // Demote blocks
    for (const auto& header : to_demote) {
        auto result = demoteBlock(header);
        if (!result.ok()) {
            return result;
        }
    }
    
    return core::Result<void>();
}

core::Result<void> BlockManager::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    // No flush needed since FileBlockStorage writes directly to disk
    return core::Result<void>();
}

BlockStorage* BlockManager::getStorageForTier(internal::BlockTier::Type tier) {
    switch (tier) {
        case internal::BlockTier::Type::HOT:
            return hot_storage_.get();
        case internal::BlockTier::Type::WARM:
            return warm_storage_.get();
        case internal::BlockTier::Type::COLD:
            return cold_storage_.get();
        default:
            return nullptr;
    }
}

}  // namespace storage
}  // namespace tsdb 