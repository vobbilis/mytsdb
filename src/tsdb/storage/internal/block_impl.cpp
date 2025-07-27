#include "tsdb/storage/internal/block_impl.h"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace tsdb {
namespace storage {
namespace internal {

BlockImpl::BlockImpl(
    const BlockHeader& header,
    std::unique_ptr<TimestampCompressor> ts_compressor,
    std::unique_ptr<ValueCompressor> val_compressor,
    std::unique_ptr<LabelCompressor> label_compressor)
    : header_(header)
    , ts_compressor_(std::move(ts_compressor))
    , val_compressor_(std::move(val_compressor))
    , label_compressor_(std::move(label_compressor))
    , dirty_(false) {}

size_t BlockImpl::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = sizeof(BlockHeader);
    for (const auto& [labels, data] : series_) {
        total += labels.size() * sizeof(std::string); // Labels
        total += data.timestamps.size(); // Timestamps
        total += data.values.size(); // Values
    }
    return total;
}

size_t BlockImpl::num_series() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return series_.size();
}

core::TimeSeries BlockImpl::read(const core::Labels& labels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = series_.find(labels);
    if (it == series_.end()) {
        return core::TimeSeries(labels); // Empty series if not found
    }

    const auto& data = it->second;
    std::vector<core::Sample> samples;
    samples.reserve(data.timestamps.size());

    // Decompress timestamps and values
    auto timestamps = ts_compressor_->decompress(data.timestamps);
    auto values = val_compressor_->decompress(data.values);

    for (size_t i = 0; i < timestamps.size(); i++) {
        samples.emplace_back(timestamps[i], values[i]);
    }

    core::TimeSeries ts(labels);
    for (const auto& s : samples) ts.add_sample(s); // or assign samples if public
    return ts;
}

std::vector<core::TimeSeries> BlockImpl::query(
    const std::vector<std::pair<std::string, std::string>>& matchers,
    int64_t start_time,
    int64_t end_time) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::TimeSeries> result;

    // Simple matcher that checks if labels match all criteria
    auto matches = [&matchers](const core::Labels& labels) {
        for (const auto& [key, value] : matchers) {
            if (labels.map().count(key) == 0 || labels.map().at(key) != value) {
                return false;
            }
        }
        return true;
    };

    for (const auto& [labels, data] : series_) {
        if (!matches(labels)) {
            continue;
        }

        // Decompress timestamps and values
        auto timestamps = ts_compressor_->decompress(data.timestamps);
        auto values = val_compressor_->decompress(data.values);
        std::vector<core::Sample> samples;
        for (size_t i = 0; i < timestamps.size(); i++) {
            samples.emplace_back(timestamps[i], values[i]);
        }
        core::TimeSeries ts(labels);
        for (const auto& s : samples) ts.add_sample(s);
        result.push_back(ts);
    }
    return result;
}

void BlockImpl::write(const core::TimeSeries& series) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Extract timestamps and values
    std::vector<int64_t> timestamps;
    std::vector<double> values;
    timestamps.reserve(series.samples().size());
    values.reserve(series.samples().size());
    
    for (const auto& sample : series.samples()) {
        timestamps.push_back(sample.timestamp());
        values.push_back(sample.value());
    }

    // Compress timestamps and values
    auto compressed_timestamps = ts_compressor_->compress(timestamps);
    auto compressed_values = val_compressor_->compress(values);

    // Update or insert series data
    auto& data = series_[series.labels()];
    data.timestamps = std::move(compressed_timestamps);
    data.values = std::move(compressed_values);

    // Update block header time range
    if (!timestamps.empty()) {
        header_.start_time = std::min(header_.start_time, timestamps.front());
        header_.end_time = std::max(header_.end_time, timestamps.back());
    }

    dirty_ = true;
}

const BlockHeader& BlockImpl::header() const {
    return header_;
}

void BlockImpl::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!dirty_) {
        return;
    }

    // Update header
    update_header();

    // Mark as clean
    dirty_ = false;
}

void BlockImpl::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (dirty_) {
        flush();
    }
    series_.clear();
}

void BlockImpl::update_header() {
    // Update CRC32
    header_.crc32 = calculate_crc();
    
    // Set flags
    header_.flags |= static_cast<uint32_t>(BlockFlags::CHECKSUM);
    if (ts_compressor_->is_compressed()) {
        header_.flags |= static_cast<uint32_t>(BlockFlags::COMPRESSED);
    }
}

uint32_t BlockImpl::calculate_crc() const {
    // Simple CRC32 implementation
    // TODO: Implement proper CRC32 calculation
    uint32_t crc = 0;
    for (const auto& [labels, data] : series_) {
        for (uint8_t byte : data.timestamps) {
            crc = (crc << 8) ^ byte;
        }
        for (uint8_t byte : data.values) {
            crc = (crc << 8) ^ byte;
        }
    }
    return crc;
}

} // namespace internal
} // namespace storage
} // namespace tsdb 