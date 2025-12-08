#include "tsdb/storage/block_manager.h"
#include "tsdb/storage/internal/block_types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include "tsdb/storage/internal/block_impl.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <shared_mutex>
#include <system_error>
#include <map>
#include <limits>
#include <thread>
#include <chrono>
#include <mutex>
#include <iostream>
#include <numeric>  // For std::iota
#include <algorithm> // For std::sort, std::is_sorted

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
           << std::hex << header.id << ".block";
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

core::Result<internal::BlockHeader> BlockManager::createBlock(int64_t start_time, int64_t end_time) {
    if (start_time > end_time) {
        return core::Result<internal::BlockHeader>::error("Invalid time range: start_time > end_time");
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    try {
        internal::BlockHeader header;
        header.magic = internal::BlockHeader::MAGIC;
        // TODO: Need a way to generate ID here if createBlock is called directly
        // For now, assume this is only called by tests or internal logic that doesn't care about ID uniqueness across restarts?
        // Or use a static counter here too?
        static std::atomic<uint64_t> local_id_counter(1000000); 
        header.id = local_id_counter++;
        header.version = internal::BlockHeader::VERSION;
        header.start_time = start_time;
        header.end_time = end_time;
        header.flags = 0;
        header.crc32 = 0;  // Will be set when block is finalized
        header.reserved = 0;     // Initialize reserved field
        
        // Check if we need to demote blocks
        if (!hot_storage_) {
            return core::Result<internal::BlockHeader>::error("Hot storage not initialized");
        }

        // Create empty block in hot tier (with retry logic)
        std::vector<uint8_t> empty_data;
        int retry_count = 0;
        const int max_retries = 3;
        
        while (retry_count < max_retries) {
            auto result = hot_storage_->write(header, empty_data);
            if (result.ok()) {
                block_tiers_[header.id] = internal::BlockTier::Type::HOT;
                return core::Result<internal::BlockHeader>(header);
            }
            
            retry_count++;
            if (retry_count < max_retries) {
                // Brief delay before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                return core::Result<internal::BlockHeader>::error(result.error()); // Return the last error
            }
        }
        
        return core::Result<internal::BlockHeader>::error("Failed to create block after retries");
    } catch (const std::exception& e) {
        return core::Result<internal::BlockHeader>::error("Block creation exception: " + std::string(e.what()));
    }
}

core::Result<void> BlockManager::finalizeBlock(const internal::BlockHeader& header) {
    if (!header.is_valid()) {
        return core::Result<void>::error("Invalid block header");
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.id);
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

    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.id);
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

    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.id);
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

    std::shared_lock<std::shared_mutex> lock(mutex_);  // Read operation uses shared lock
    
    auto it = block_tiers_.find(header.id);
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

    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = block_tiers_.find(header.id);
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
    auto it = block_tiers_.find(header.id);
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
    
    block_tiers_[header.id] = to_tier;
    return core::Result<void>();
}

core::Result<void> BlockManager::compact() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // For now, just demote all blocks in hot tier to warm tier
    std::vector<internal::BlockHeader> to_demote;
    
    // Find all blocks in hot tier
    for (const auto& [id, tier] : block_tiers_) {
        if (tier == internal::BlockTier::Type::HOT) {
            internal::BlockHeader header;
            header.id = id;
            header.magic = internal::BlockHeader::MAGIC;
            header.version = internal::BlockHeader::VERSION;
            header.flags = 0;
            header.crc32 = 0;
            header.start_time = 0;
            header.end_time = 0;
            header.reserved = 0;
            
            if (!header.is_valid()) {
                // Log warning but continue?
            }
            
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
    std::shared_lock<std::shared_mutex> lock(mutex_);  // Read operation uses shared lock
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

core::Result<void> BlockManager::seal_and_persist_block(std::shared_ptr<internal::BlockImpl> block) {
    if (!block) {
        return core::Result<void>::error("Invalid block provided");
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);

    try {
        internal::BlockHeader header = block->get_header();
        
        // Serialize the block's data using the new serialize method
        std::vector<uint8_t> block_data = block->serialize();

        auto result = hot_storage_->write(header, block_data);
        if (result.ok()) {
            block_tiers_[header.id] = internal::BlockTier::Type::HOT;
        }
        return result;

    } catch (const std::exception& e) {
        return core::Result<void>::error("Seal and persist failed: " + std::string(e.what()));
    }
}

core::Result<std::string> BlockManager::demoteToParquet(const internal::BlockHeader& header) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = block_tiers_.find(header.id);
    if (it == block_tiers_.end()) {
        return core::Result<std::string>::error("Block not found");
    }

    // 1. Read block data from current tier
    auto storage = getStorageForTier(it->second);
    auto read_result = storage->read(header);
    if (!read_result.ok()) {
        return core::Result<std::string>::error("Failed to read block: " + read_result.error());
    }

    // 2. Deserialize to BlockImpl
    auto block = internal::BlockImpl::deserialize(read_result.value());
    if (!block) {
        return core::Result<std::string>::error("Failed to deserialize block");
    }

    // 3. Prepare Parquet Writer
    // Construct path: data_dir/2/<id>.parquet
    std::stringstream ss;
    ss << data_dir_ << "/2/" << std::hex << header.id << ".parquet";
    std::string parquet_path = ss.str();

    parquet::ParquetWriter writer;
    auto open_result = writer.Open(parquet_path, parquet::SchemaMapper::GetArrowSchema());
    if (!open_result.ok()) {
        return core::Result<std::string>::error("Failed to open Parquet writer: " + open_result.error());
    }

    // 4. Iterate over series and write to Parquet
    std::vector<std::pair<std::string, std::string>> empty_matchers;
    auto all_series = block->query(empty_matchers, header.start_time, header.end_time);

    for (const auto& series : all_series) {
        auto batch = parquet::SchemaMapper::ToRecordBatch(series.samples(), series.labels().map());
        if (!batch) {
             return core::Result<std::string>::error("Failed to convert series to RecordBatch");
        }
        auto write_result = writer.WriteBatch(batch);
        if (!write_result.ok()) {
            return core::Result<std::string>::error("Failed to write batch: " + write_result.error());
        }
    }

    auto close_result = writer.Close();
    if (!close_result.ok()) {
        return core::Result<std::string>::error("Failed to close Parquet writer: " + close_result.error());
    }

    // 5. Update state
    // Remove from old storage
    storage->remove(header);
    
    // Update tier map to COLD
    block_tiers_[header.id] = internal::BlockTier::Type::COLD;

    return core::Result<std::string>(parquet_path);
}

core::Result<std::map<uint64_t, std::string>> BlockManager::demoteBlocksToParquet(
    const std::vector<std::pair<core::Labels, std::shared_ptr<internal::BlockInternal>>>& blocks) {
    
    if (blocks.empty()) return std::map<uint64_t, std::string>{};
    
    // 1. Group by Time Partition (Day)
    // For simplicity in this phase, we assume all blocks belong to the same day/partition basically, 
    // or we just pick the first block's time to decide the partition.
    // In a full implementation, we might split into multiple files if they span days.
    // Let's use the first block's start time for the partition.
    int64_t start_time = blocks[0].second->start_time();
    
    std::time_t t = start_time / 1000; // ms to s
    std::tm tm = *std::gmtime(&t);
    
    std::stringstream date_ss;
    date_ss << std::put_time(&tm, "%Y/%m/%d");
    std::string partition_dir = data_dir_ + "/2/" + date_ss.str();
    
    // Create directory
    std::filesystem::create_directories(partition_dir);
    
    // Generate unique file name
    // Use first block ID + count + random/timestamp suffix
    std::string parquet_filename = std::to_string(blocks[0].second->header().id) + "_" + std::to_string(blocks.size()) + ".parquet";
    std::string parquet_path = partition_dir + "/" + parquet_filename;
    
    // 2. Sort blocks by Labels (to optimize read locality)
    auto sorted_blocks = blocks;
    std::sort(sorted_blocks.begin(), sorted_blocks.end(), 
              [](const auto& a, const auto& b) {
                  return a.first < b.first;
              });
              
    // 3. Write to Parquet
    // We use the same ParquetWriter but we need to feed it batches manually?
    // ParquetWriter::WriteBatch takes a RecordBatch.
    // We need to construct a large RecordBatch from multiple blocks.
    
    parquet::ParquetWriter writer;
    // Estimate total rows? 
    // For optimization, we can set max_row_group_length based on total samples.
    // Let's use default 128MB or similar.
    auto open_result = writer.Open(parquet_path, parquet::SchemaMapper::GetArrowSchema());
    if (!open_result.ok()) {
        return core::Result<std::map<uint64_t, std::string>>::error(open_result.error());
    }
    
    // Buffers for batching
    std::vector<int64_t> batch_timestamps;
    std::vector<double> batch_values;
    std::vector<core::Labels> batch_labels;
    std::vector<size_t> batch_counts; // How many samples per series occurrence
    
    // Limit per row group (e.g., 100k samples)
    const size_t MAX_BATCH_SIZE = 100000; 
    
    for (const auto& [labels, block] : sorted_blocks) {
        // Read columns (Zero-Copy!)
        auto columns = block->read_columns(labels);
        auto ts = columns.first;   // Make copies since we may need to sort
        auto vals = columns.second;
        
        if (ts.empty()) continue;
        
        // IMPORTANT: Sort data by timestamp for Parquet optimization
        // Parquet predicate pushdown and row group pruning work best with sorted data
        // This enables min/max statistics to filter out irrelevant row groups
        if (!std::is_sorted(ts.begin(), ts.end())) {
            // Create index array for sorting
            std::vector<size_t> indices(ts.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), 
                      [&ts](size_t i, size_t j) { return ts[i] < ts[j]; });
            
            // Apply sort order to both arrays
            std::vector<int64_t> sorted_ts(ts.size());
            std::vector<double> sorted_vals(vals.size());
            for (size_t i = 0; i < indices.size(); ++i) {
                sorted_ts[i] = ts[indices[i]];
                sorted_vals[i] = vals[indices[i]];
            }
            ts = std::move(sorted_ts);
            vals = std::move(sorted_vals);
        }
        
        // Add series to Bloom filter for fast "definitely not present" checks
        // Build canonical label string for series_id computation
        std::vector<std::pair<std::string, std::string>> sorted_labels;
        for (const auto& [k, v] : labels.map()) {
            sorted_labels.emplace_back(k, v);
        }
        std::sort(sorted_labels.begin(), sorted_labels.end());
        std::string labels_str;
        for (const auto& [k, v] : sorted_labels) {
            if (!labels_str.empty()) labels_str += ",";
            labels_str += k + "=" + v;
        }
        writer.AddSeriesToBloomFilterByLabels(labels_str);
        
        // Write series data - already sorted by Labels at outer level, now sorted by timestamp within
        // Arrow Parquet writer buffers multiple WriteBatch calls until RowGroupSize is reached
        auto batch = parquet::SchemaMapper::ToRecordBatch(ts, vals, labels.map());
        writer.WriteBatch(batch);
    }
    
    writer.Close();
    
    // 4. Return map
    std::map<uint64_t, std::string> result;
    for (const auto& [labels, block] : sorted_blocks) {
        result[block->header().id] = parquet_path;
    }
    
    return core::Result<std::map<uint64_t, std::string>>(result);
}

core::Result<std::shared_ptr<internal::BlockImpl>> BlockManager::readFromParquet(const internal::BlockHeader& header) {
    // 1. Open Parquet Reader
    std::stringstream ss;
    ss << data_dir_ << "/2/" << std::hex << header.id << ".parquet";
    std::string parquet_path = ss.str();

    parquet::ParquetReader reader;
    auto open_result = reader.Open(parquet_path);
    if (!open_result.ok()) {
        return core::Result<std::shared_ptr<internal::BlockImpl>>::error("Failed to open Parquet file: " + open_result.error());
    }

    // 2. Create new BlockImpl
    auto block = std::make_shared<internal::BlockImpl>(
        header,
        std::make_unique<internal::SimpleTimestampCompressor>(),
        std::make_unique<internal::SimpleValueCompressor>(),
        std::make_unique<internal::SimpleLabelCompressor>()
    );

    // 3. Read batches and populate block
    while (true) {
        std::shared_ptr<arrow::RecordBatch> batch;
        auto read_result = reader.ReadBatch(&batch);
        if (!read_result.ok()) {
            return core::Result<std::shared_ptr<internal::BlockImpl>>::error("Failed to read batch: " + read_result.error());
        }
        if (!batch) {
            break; // EOF
        }

        auto samples_result = parquet::SchemaMapper::ToSamples(batch);
        if (!samples_result.ok()) {
             return core::Result<std::shared_ptr<internal::BlockImpl>>::error("Failed to convert batch to samples: " + samples_result.error());
        }
        auto samples = samples_result.value();

        auto tags_result = parquet::SchemaMapper::ExtractTags(batch);
        if (!tags_result.ok()) {
             return core::Result<std::shared_ptr<internal::BlockImpl>>::error("Failed to extract tags: " + tags_result.error());
        }
        core::Labels labels(tags_result.value());

        for (const auto& sample : samples) {
            block->append(labels, sample);
        }
    }

    block->seal();
    return core::Result<std::shared_ptr<internal::BlockImpl>>(block);
}

core::Result<void> BlockManager::compactParquetFiles(const std::vector<std::string>& input_paths, const std::string& output_path) {
    if (input_paths.empty()) {
        return core::Result<void>::error("No input files provided for compaction");
    }

    // 1. Open Writer
    parquet::ParquetWriter writer;
    // We need a schema. We can get it from the first file.
    // Or use the standard schema from SchemaMapper.
    auto schema = parquet::SchemaMapper::GetArrowSchema();
    auto open_res = writer.Open(output_path, schema);
    if (!open_res.ok()) {
        return core::Result<void>::error("Failed to open output file: " + open_res.error());
    }

    // 2. Iterate inputs
    for (const auto& input_path : input_paths) {
        parquet::ParquetReader reader;
        auto open_reader_res = reader.Open(input_path);
        if (!open_reader_res.ok()) {
            // If one fails, we abort.
            // In production, we might want to skip or handle gracefully.
            return core::Result<void>::error("Failed to open input file: " + input_path + " (" + open_reader_res.error() + ")");
        }

        while (true) {
            std::shared_ptr<arrow::RecordBatch> batch;
            auto read_res = reader.ReadBatch(&batch);
            if (!read_res.ok()) {
                return core::Result<void>::error("Failed to read batch from: " + input_path + " (" + read_res.error() + ")");
            }
            if (!batch) break; // EOF

            auto write_res = writer.WriteBatch(batch);
            if (!write_res.ok()) {
                return core::Result<void>::error("Failed to write batch: " + write_res.error());
            }
        }
        reader.Close();
    }

    // 3. Close Writer
    auto close_res = writer.Close();
    if (!close_res.ok()) {
        return core::Result<void>::error("Failed to close output file: " + close_res.error());
    }

    return core::Result<void>();
}

core::Result<std::vector<internal::BlockHeader>> BlockManager::recoverBlocks() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::vector<internal::BlockHeader> headers;
    
    // Helper lambda to scan a tier directory
    auto scan_tier = [&](internal::BlockTier::Type tier) {
        std::string tier_dir = data_dir_ + "/" + std::to_string(static_cast<int>(tier));
        if (!fs::exists(tier_dir)) return;
        
        for (const auto& entry : fs::directory_iterator(tier_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".block") {
                std::ifstream file(entry.path(), std::ios::binary);
                if (file) {
                    internal::BlockHeader header;
                    file.read(reinterpret_cast<char*>(&header), sizeof(header));
                    if (file && header.is_valid()) {
                        // Update internal state
                        block_tiers_[header.id] = tier;
                        headers.push_back(header);
                    }
                }
            }
        }
    };

    // Scan HOT and COLD tiers
    scan_tier(internal::BlockTier::Type::HOT);
    scan_tier(internal::BlockTier::Type::COLD);
    
    return headers;
}

bool BlockManager::persistSeriesToParquet(core::SeriesID series_id, std::shared_ptr<core::TimeSeries> series) {
    if (!series || series->samples().empty()) {
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    try {
        // Construct Parquet path using series_id
        std::stringstream ss;
        ss << data_dir_ << "/2/" << std::hex << series_id << ".parquet";
        std::string parquet_path = ss.str();
        
        // Open Parquet Writer
        parquet::ParquetWriter writer;
        auto open_result = writer.Open(parquet_path, parquet::SchemaMapper::GetArrowSchema());
        if (!open_result.ok()) {
            return false;
        }
        
        // Convert series to RecordBatch and write
        auto batch = parquet::SchemaMapper::ToRecordBatch(series->samples(), series->labels().map());
        if (!batch) {
            return false;
        }
        
        auto write_result = writer.WriteBatch(batch);
        if (!write_result.ok()) {
            return false;
        }
        
        auto close_result = writer.Close();
        if (!close_result.ok()) {
            return false;
        }
        
        // Track this series as being in COLD tier
        block_tiers_[series_id] = internal::BlockTier::Type::COLD;
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

}  // namespace storage
}  // namespace tsdb