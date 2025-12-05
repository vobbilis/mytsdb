#include "tsdb/storage/series.h"
#include "tsdb/storage/internal/block_impl.h"
#include <algorithm>

namespace tsdb {
namespace storage {

// Global counter for block IDs (from storage_impl.cpp)
// TODO: This should probably be managed by BlockManager or passed in
static std::atomic<uint64_t> global_block_id_counter{1};

Series::Series(
    core::SeriesID id,
    const core::Labels& labels,
    core::MetricType type,
    const struct Granularity& granularity)
    : metadata_{id, labels, type, granularity} {}

bool Series::append(const core::Sample& sample) {
    std::unique_lock lock(mutex_);

    if (!current_block_) {
        // Create a new block if one doesn't exist.
        tsdb::storage::internal::BlockHeader header;
        // Use global counter to ensure unique block ID
        header.id = global_block_id_counter++;
        header.magic = tsdb::storage::internal::BlockHeader::MAGIC;
        header.version = tsdb::storage::internal::BlockHeader::VERSION;
        header.flags = 0;
        header.crc32 = 0;
        header.start_time = sample.timestamp();
        header.end_time = sample.timestamp();
        header.reserved = 0;

        current_block_ = std::make_shared<internal::BlockImpl>(
            header,
            std::make_unique<internal::SimpleTimestampCompressor>(),
            std::make_unique<internal::SimpleValueCompressor>(),
            std::make_unique<internal::SimpleLabelCompressor>());
    }

    // Append the sample to the current block using the block's append method
    current_block_->append(metadata_.labels, sample);

    // A real implementation would check if the block is full based on size or number of samples.
    // For now, we'll simulate this by sealing the block after a certain number of samples.
    // This logic will be properly implemented in Phase 3.
    return current_block_->num_samples() >= 120; // Placeholder for "is full" logic
}

std::shared_ptr<internal::BlockImpl> Series::seal_block() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (!current_block_) {
        return nullptr;
    }

    // Compress buffered data before sealing
    current_block_->seal();

    auto sealed_block = current_block_;
    blocks_.push_back(sealed_block); // Keep track of historical blocks
    current_block_ = nullptr; // The series is now ready for a new head block

    return sealed_block;
}

std::vector<std::shared_ptr<internal::BlockInternal>> Series::GetBlocks() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return blocks_;
}

void Series::AddBlock(std::shared_ptr<internal::BlockInternal> block) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    blocks_.push_back(block);
}

bool Series::ReplaceBlock(std::shared_ptr<internal::BlockInternal> old_block, std::shared_ptr<internal::BlockInternal> new_block) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    for (auto& block : blocks_) {
        if (block == old_block) {
            block = new_block;
            return true;
        }
    }
    return false;
}

core::Result<std::vector<core::Sample>> Series::Read(
    core::Timestamp start, core::Timestamp end) const {
    std::shared_lock lock(mutex_);
    
    std::vector<core::Sample> result;
    
    // Read from sealed blocks first
    for (const auto& block : blocks_) {
        if (!block) continue;
        
        // Check if block overlaps with time range
        if (block->end_time() < start || block->start_time() > end) {
            continue;  // Block doesn't overlap with requested range
        }
        
        // Read series from block
        auto block_series = block->read(metadata_.labels);
        
        // Add samples within time range
        for (const auto& sample : block_series.samples()) {
            if (sample.timestamp() >= start && sample.timestamp() <= end) {
                result.push_back(sample);
            }
        }
    }
    
    // Read from current active block if it exists
    // Note: We read from current_block_ even if time range check fails,
    // because the block's time range might not be updated yet (e.g., for new blocks)
    if (current_block_) {
        auto block_series = current_block_->read(metadata_.labels);
        
        // Add samples within time range (filter at sample level for accuracy)
        for (const auto& sample : block_series.samples()) {
            if (sample.timestamp() >= start && sample.timestamp() <= end) {
                result.push_back(sample);
            }
        }
    }
    
    // Sort samples by timestamp to ensure chronological order
    std::sort(result.begin(), result.end(), 
              [](const core::Sample& a, const core::Sample& b) {
                  return a.timestamp() < b.timestamp();
              });
    
    // Deduplicate samples to handle overlapping data (e.g., from WAL replay and persisted blocks)
    auto last = std::unique(result.begin(), result.end(),
                           [](const core::Sample& a, const core::Sample& b) {
                               return a.timestamp() == b.timestamp();
                           });
    result.erase(last, result.end());
    
    return core::Result<std::vector<core::Sample>>(std::move(result));
}

const core::Labels& Series::Labels() const {
    return metadata_.labels;
}

core::MetricType Series::Type() const {
    return metadata_.type;
}

const struct Granularity& Series::GetGranularity() const {
    return metadata_.granularity;
}

core::SeriesID Series::GetID() const {
    return metadata_.id;
}

size_t Series::NumSamples() const {
    std::shared_lock lock(mutex_);
    size_t total = 0;
    for (const auto& block : blocks_) {
        total += block->num_samples();
    }
    return total;
}

core::Timestamp Series::MinTimestamp() const {
    std::shared_lock lock(mutex_);
    if (blocks_.empty()) {
        return 0;
    }
    return blocks_.front()->start_time();
}

core::Timestamp Series::MaxTimestamp() const {
    std::shared_lock lock(mutex_);
    if (blocks_.empty()) {
        return 0;
    }
    return blocks_.back()->end_time();
}

} // namespace storage
} // namespace tsdb
