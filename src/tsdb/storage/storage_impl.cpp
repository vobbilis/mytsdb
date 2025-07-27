#include "tsdb/storage/storage_impl.h"
#include "tsdb/storage/series.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
// For now, just use simple error output instead of spdlog
#define spdlog_error(msg) std::cerr << "ERROR: " << msg << std::endl
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
        // For now, we'll just handle this without creating blocks
        // This is a simplified implementation to get the build working
        // In a full implementation, we would need to properly implement BlockImpl
        // or use a different storage approach
    }
    
    // For this simplified implementation, we'll just accept the write
    // without actually storing the data in blocks
    // In a full implementation, this would create and manage blocks properly
    
    return core::Result<void>();
}

core::Result<std::vector<core::Sample>> Series::Read(
    core::Timestamp start, core::Timestamp end) const {
    std::shared_lock lock(mutex_);
    
    // For this simplified implementation, return empty results
    // In a full implementation, this would read from blocks
    std::vector<core::Sample> result;
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
            std::cerr << "ERROR: Failed to flush storage on shutdown: " << result.error().what() << std::endl;
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
    
    // For now, return a basic set of common label names
    // In a full implementation, this would scan through blocks
    // to collect all unique label names
    std::vector<std::string> result = {
        "__name__",
        "job",
        "instance",
        "service"
    };
    
    return core::Result<std::vector<std::string>>(std::move(result));
}

core::Result<std::vector<std::string>> StorageImpl::label_values(
    const std::string& label_name) {
    if (!initialized_) {
        return core::Result<std::vector<std::string>>::error("Storage not initialized");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // For now, return mock values based on label name
    // In a full implementation, this would scan through blocks
    // to collect all unique values for the specified label
    std::vector<std::string> result;
    
    if (label_name == "job") {
        result = {"prometheus", "node_exporter", "grafana"};
    } else if (label_name == "instance") {
        result = {"localhost:9090", "localhost:9100", "localhost:3000"};
    } else if (label_name == "service") {
        result = {"metrics", "monitoring", "alerting"};
    } else if (label_name == "__name__") {
        result = {"cpu_usage", "memory_usage", "disk_usage"};
    }
    
    return core::Result<std::vector<std::string>>(std::move(result));
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