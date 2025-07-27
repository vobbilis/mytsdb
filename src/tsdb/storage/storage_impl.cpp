#include "tsdb/storage/storage_impl.h"
#include "tsdb/storage/series.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include "tsdb/storage/block_manager.h"
#include "tsdb/storage/block.h"
#include "tsdb/storage/internal/block_impl.h"
#include "tsdb/storage/compression.h"
#include "tsdb/storage/histogram_ops.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include "tsdb/core/interfaces.h"
#include <system_error>
#include <shared_mutex>

namespace tsdb {
namespace storage {

using namespace internal;

} // namespace storage
} // namespace tsdb

// Add error formatting for spdlog
namespace fmt {
template<>
struct formatter<tsdb::core::Error> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) const { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const tsdb::core::Error& err, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}", err.what());
    }
};
}

namespace tsdb {
namespace storage {

Series::Series(
    core::SeriesID id,
    const core::Labels& labels,
    core::MetricType type,
    const struct Granularity& granularity)
    : id_(id)
    , labels_(labels)
    , type_(type)
    , granularity_(granularity) {}

core::Result<void> Series::Write(const std::vector<core::Sample>& samples) {
    std::unique_lock lock(mutex_);
    
    if (!current_block_) {
        // Create new block with default compressors
        tsdb::storage::BlockHeader header;
        header.magic = tsdb::storage::BlockHeader::MAGIC;
        header.version = tsdb::storage::BlockHeader::VERSION;
        header.flags = 0;
        header.crc32 = 0;
        header.start_time = std::numeric_limits<int64_t>::max();
        header.end_time = std::numeric_limits<int64_t>::min();
        header.reserved = 0;

        current_block_ = std::make_shared<BlockImpl>(
            header,
            std::make_unique<SimpleTimestampCompressorImpl>(),
            std::make_unique<SimpleValueCompressorImpl>(),
            std::make_unique<SimpleLabelCompressorImpl>());
        blocks_.push_back(current_block_);
    }
    
    // Create TimeSeries from samples
    core::TimeSeries series(labels_);
    for (const auto& sample : samples) {
        series.add_sample(sample);
    }
    
    // Write to current block
    auto block_impl = std::static_pointer_cast<BlockImpl>(current_block_);
    block_impl->write(series);
    
    return core::Result<void>();
}

core::Result<std::vector<core::Sample>> Series::Read(
    core::Timestamp start, core::Timestamp end) const {
    std::shared_lock lock(mutex_);
    
    std::vector<core::Sample> result;
    
    // Read from all blocks that overlap with the time range
    for (const auto& block : blocks_) {
        if (block->start_time() > end || block->end_time() < start) {
            continue;
        }
        
        auto series = block->read(labels_);
        if (series.samples().empty()) {
            continue;
        }
        
        // Filter samples by time range
        for (const auto& sample : series.samples()) {
            if (sample.timestamp() >= start && sample.timestamp() <= end) {
                result.push_back(sample);
            }
        }
    }
    
    // Sort samples by timestamp
    std::sort(result.begin(), result.end(),
              [](const core::Sample& a, const core::Sample& b) {
                  return a.timestamp() < b.timestamp();
              });
    
    return result;
}

const core::Labels& Series::Labels() const {
    return labels_;
}

core::MetricType Series::Type() const {
    return type_;
}

const struct Granularity& Series::GetGranularity() const {
    return granularity_;
}

core::SeriesID Series::GetID() const {
    return id_;
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

StorageImpl::StorageImpl()
    : initialized_(false) {}

StorageImpl::~StorageImpl() {
    if (initialized_) {
        // Just flush data since BlockManager doesn't have close()
        auto result = flush();
        if (!result.ok()) {
            spdlog::error("Failed to flush storage on shutdown: {}", result.error());
        }
    }
}

core::Result<void> StorageImpl::init(const core::StorageConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (initialized_) {
        return core::Result<void>::error("Storage already initialized");
    }
    
    // Create block manager with data directory from config
    block_manager_ = std::make_shared<BlockManager>(config.data_dir);
    initialized_ = true;
    return core::Result<void>();
}

core::Result<void> StorageImpl::write([[maybe_unused]] const core::TimeSeries& series) {
    if (!initialized_) {
        return core::Result<void>::error("Storage not initialized");
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    // TODO: Implement write
    return core::Result<void>();
}

core::Result<core::TimeSeries> StorageImpl::read(
    [[maybe_unused]] const core::Labels& labels,
    [[maybe_unused]] int64_t start_time,
    [[maybe_unused]] int64_t end_time) {
    if (!initialized_) {
        return core::Result<core::TimeSeries>::error("Storage not initialized");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // TODO: Implement read
    return core::Result<core::TimeSeries>::error("Not implemented");
}

core::Result<std::vector<core::TimeSeries>> StorageImpl::query(
    [[maybe_unused]] const std::vector<std::pair<std::string, std::string>>& matchers,
    [[maybe_unused]] int64_t start_time,
    [[maybe_unused]] int64_t end_time) {
    if (!initialized_) {
        return core::Result<std::vector<core::TimeSeries>>::error("Storage not initialized");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // TODO: Implement query
    return core::Result<std::vector<core::TimeSeries>>::error("Not implemented");
}

core::Result<std::vector<std::string>> StorageImpl::label_names() {
    if (!initialized_) {
        return core::Result<std::vector<std::string>>::error("Storage not initialized");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // TODO: Implement label_names
    return core::Result<std::vector<std::string>>::error("Not implemented");
}

core::Result<std::vector<std::string>> StorageImpl::label_values(
    [[maybe_unused]] const std::string& label_name) {
    if (!initialized_) {
        return core::Result<std::vector<std::string>>::error("Storage not initialized");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // TODO: Implement label_values
    return core::Result<std::vector<std::string>>::error("Not implemented");
}

core::Result<void> StorageImpl::delete_series(
    [[maybe_unused]] const std::vector<std::pair<std::string, std::string>>& matchers) {
    if (!initialized_) {
        return core::Result<void>::error("Storage not initialized");
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    // TODO: Implement delete_series
    return core::Result<void>::error("Not implemented");
}

core::Result<void> StorageImpl::compact() {
    if (!initialized_) {
        return core::Result<void>::error("Storage not initialized");
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return block_manager_->compact();
}

core::Result<void> StorageImpl::flush() {
    if (!initialized_) {
        return core::Result<void>::error("Storage not initialized");
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return block_manager_->flush();
}

core::Result<void> StorageImpl::close() {
    if (!initialized_) {
        return core::Result<void>();
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    // Just flush since BlockManager doesn't have close()
    auto result = block_manager_->flush();
    if (!result.ok()) {
        return result;
    }
    
    initialized_ = false;
    return core::Result<void>();
}

std::string StorageImpl::stats() const {
    if (!initialized_) {
        return "Storage not initialized";
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // TODO: Implement proper stats
    return "Storage statistics not implemented";
}

} // namespace storage
} // namespace tsdb 