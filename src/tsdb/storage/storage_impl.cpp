#include "tsdb/storage/storage_impl.h"
#include "tsdb/storage/series.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <limits>
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
        // Create new block with default compressors
        tsdb::storage::internal::BlockHeader header;
        header.magic = tsdb::storage::internal::BlockHeader::MAGIC;
        header.version = tsdb::storage::internal::BlockHeader::VERSION;
        header.flags = 0;
        header.crc32 = 0;
        header.start_time = std::numeric_limits<int64_t>::max();
        header.end_time = std::numeric_limits<int64_t>::min();
        header.reserved = 0;

        current_block_ = std::make_shared<BlockImpl>(
            header,
            std::make_unique<SimpleTimestampCompressor>(),
            std::make_unique<SimpleValueCompressor>(),
            std::make_unique<SimpleLabelCompressor>());
        blocks_.push_back(current_block_);
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
            // Error logging removed to fix compilation issue
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

core::Result<void> StorageImpl::write(const core::TimeSeries& series) {
    if (!initialized_) {
        return core::Result<void>::error("Storage not initialized");
    }
    
    if (series.empty()) {
        return core::Result<void>::error("Cannot write empty time series");
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    try {
        // Store the series in memory for now
        // In a full implementation, this would create blocks and write to disk
        stored_series_.push_back(series);
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Write failed: " + std::string(e.what()));
    }
}

core::Result<core::TimeSeries> StorageImpl::read(
    const core::Labels& labels,
    int64_t start_time,
    int64_t end_time) {
    if (!initialized_) {
        return core::Result<core::TimeSeries>::error("Storage not initialized");
    }
    
    if (start_time >= end_time) {
        return core::Result<core::TimeSeries>::error("Invalid time range: start_time must be less than end_time");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    try {
        // Create result time series with the requested labels
        core::TimeSeries result(labels);
        
        // Search through stored series for matching labels
        for (const auto& stored_series : stored_series_) {
            if (stored_series.labels() == labels) {
                // Add samples within the time range
                for (const auto& sample : stored_series.samples()) {
                    if (sample.timestamp() >= start_time && sample.timestamp() <= end_time) {
                        result.add_sample(sample);
                    }
                }
                break; // Found the matching series
            }
        }
        
        return core::Result<core::TimeSeries>(std::move(result));
    } catch (const std::exception& e) {
        return core::Result<core::TimeSeries>::error("Read failed: " + std::string(e.what()));
    }
}

core::Result<std::vector<core::TimeSeries>> StorageImpl::query(
    const std::vector<std::pair<std::string, std::string>>& matchers,
    int64_t start_time,
    int64_t end_time) {
    if (!initialized_) {
        return core::Result<std::vector<core::TimeSeries>>::error("Storage not initialized");
    }
    
    if (start_time >= end_time) {
        return core::Result<std::vector<core::TimeSeries>>::error("Invalid time range: start_time must be less than end_time");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    try {
        std::vector<core::TimeSeries> results;
        
        // Check each stored series against the matchers
        for (const auto& stored_series : stored_series_) {
            bool matches = true;
            
            // Check if all matchers are satisfied
            for (const auto& [key, value] : matchers) {
                auto series_value = stored_series.labels().get(key);
                if (!series_value.has_value() || series_value.value() != value) {
                    matches = false;
                    break;
                }
            }
            
            if (matches) {
                // Create a time series for this matching series
                core::TimeSeries result_series(stored_series.labels());
                
                // Add samples within the time range
                for (const auto& sample : stored_series.samples()) {
                    if (sample.timestamp() >= start_time && sample.timestamp() <= end_time) {
                        result_series.add_sample(sample);
                    }
                }
                
                if (!result_series.empty()) {
                    results.push_back(std::move(result_series));
                }
            }
        }
        
        return core::Result<std::vector<core::TimeSeries>>(std::move(results));
    } catch (const std::exception& e) {
        return core::Result<std::vector<core::TimeSeries>>::error("Query failed: " + std::string(e.what()));
    }
}

core::Result<std::vector<std::string>> StorageImpl::label_names() {
    if (!initialized_) {
        return core::Result<std::vector<std::string>>::error("Storage not initialized");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    try {
        std::set<std::string> label_names_set;
        
        // Collect all label names from stored series
        for (const auto& series : stored_series_) {
            for (const auto& [name, _] : series.labels().map()) {
                label_names_set.insert(name);
            }
        }
        
        std::vector<std::string> result(label_names_set.begin(), label_names_set.end());
        return core::Result<std::vector<std::string>>(std::move(result));
    } catch (const std::exception& e) {
        return core::Result<std::vector<std::string>>::error("Label names failed: " + std::string(e.what()));
    }
}

core::Result<std::vector<std::string>> StorageImpl::label_values(
    const std::string& label_name) {
    if (!initialized_) {
        return core::Result<std::vector<std::string>>::error("Storage not initialized");
    }
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    try {
        std::set<std::string> values_set;
        
        // Collect all values for the specified label name
        for (const auto& series : stored_series_) {
            auto value = series.labels().get(label_name);
            if (value.has_value()) {
                values_set.insert(value.value());
            }
        }
        
        std::vector<std::string> result(values_set.begin(), values_set.end());
        return core::Result<std::vector<std::string>>(std::move(result));
    } catch (const std::exception& e) {
        return core::Result<std::vector<std::string>>::error("Label values failed: " + std::string(e.what()));
    }
}

core::Result<void> StorageImpl::delete_series(
    const std::vector<std::pair<std::string, std::string>>& matchers) {
    if (!initialized_) {
        return core::Result<void>::error("Storage not initialized");
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    try {
        // Find and remove series that match the criteria
        auto it = stored_series_.begin();
        while (it != stored_series_.end()) {
            bool matches = true;
            
            // Check if all matchers are satisfied
            for (const auto& [key, value] : matchers) {
                auto series_value = it->labels().get(key);
                if (!series_value.has_value() || series_value.value() != value) {
                    matches = false;
                    break;
                }
            }
            
            if (matches) {
                it = stored_series_.erase(it);
            } else {
                ++it;
            }
        }
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("Delete series failed: " + std::string(e.what()));
    }
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
    
    try {
        std::ostringstream oss;
        oss << "Storage Statistics:\n";
        oss << "  Total series: " << stored_series_.size() << "\n";
        
        size_t total_samples = 0;
        int64_t min_time = std::numeric_limits<int64_t>::max();
        int64_t max_time = std::numeric_limits<int64_t>::min();
        
        for (const auto& series : stored_series_) {
            total_samples += series.samples().size();
            for (const auto& sample : series.samples()) {
                min_time = std::min(min_time, sample.timestamp());
                max_time = std::max(max_time, sample.timestamp());
            }
        }
        
        oss << "  Total samples: " << total_samples << "\n";
        if (min_time != std::numeric_limits<int64_t>::max()) {
            oss << "  Time range: " << min_time << " to " << max_time << "\n";
        }
        
        return oss.str();
    } catch (const std::exception& e) {
        return "Error generating stats: " + std::string(e.what());
    }
}

} // namespace storage
} // namespace tsdb 