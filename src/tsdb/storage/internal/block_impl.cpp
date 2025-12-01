#include "tsdb/storage/internal/block_impl.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <cstring>
#include "tsdb/storage/read_performance_instrumentation.h"

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
    , dirty_(false)
    , sealed_(false) {}

size_t BlockImpl::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = sizeof(BlockHeader);
    for (const auto& [labels, data] : series_) {
        total += labels.size() * sizeof(std::string); // Labels
        if (data.is_compressed) {
            total += data.timestamps_compressed.size(); // Compressed timestamps
            total += data.values_compressed.size(); // Compressed values
        } else {
            // Uncompressed data
            total += data.timestamps_uncompressed.size() * sizeof(int64_t);
            total += data.values_uncompressed.size() * sizeof(double);
        }
    }
    return total;
}

size_t BlockImpl::num_series() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return series_.size();
}

core::TimeSeries BlockImpl::read(const core::Labels& labels) const {
    ReadPerformanceInstrumentation::ReadMetrics metrics;
    metrics.blocks_accessed = 1;

    std::lock_guard<std::mutex> lock(mutex_);
    // Measure lock wait + map lookup as "block_read" for now (since we hold lock during read)
    // Ideally we'd separate lock wait, but std::lock_guard combines them.
    // We can assume map lookup is fast.
    
    auto it = series_.find(labels);
    if (it == series_.end()) {
        ReadPerformanceInstrumentation::instance().record_read(metrics);
        return core::TimeSeries(labels); // Empty series if not found
    }

    const auto& data = it->second;
    
    // Check if data is compressed or uncompressed
    if (!data.is_compressed) {
        ReadScopedTimer read_timer(metrics.block_read_us);
        // Read from uncompressed buffers
        core::TimeSeries ts(labels);
        size_t min_size = std::min(data.timestamps_uncompressed.size(), data.values_uncompressed.size());
        for (size_t i = 0; i < min_size; i++) {
            ts.add_sample(data.timestamps_uncompressed[i], data.values_uncompressed[i]);
        }
        metrics.samples_scanned = min_size;
        ReadPerformanceInstrumentation::instance().record_read(metrics);
        return ts;
    }
    
    // Data is compressed - decompress it
    // The compressed data is in the new batch format (single compressed block per series)
    std::vector<int64_t> all_timestamps;
    std::vector<double> all_values;
    
    {
        ReadScopedTimer decompress_timer(metrics.decompression_us);
        all_timestamps = ts_compressor_->decompress(data.timestamps_compressed);
        all_values = val_compressor_->decompress(data.values_compressed);
    }
    
    // Match timestamps and values to create samples
    core::TimeSeries ts(labels);
    size_t min_size = std::min(all_timestamps.size(), all_values.size());
    for (size_t i = 0; i < min_size; i++) {
        ts.add_sample(all_timestamps[i], all_values[i]);
    }
    
    metrics.samples_scanned = min_size;
    ReadPerformanceInstrumentation::instance().record_read(metrics);
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
        std::vector<int64_t> timestamps;
        std::vector<double> values;
        
        if (data.is_compressed) {
            timestamps = ts_compressor_->decompress(data.timestamps_compressed);
            values = val_compressor_->decompress(data.values_compressed);
        } else {
            timestamps = data.timestamps_uncompressed;
            values = data.values_uncompressed;
        }
        
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

    // Compress timestamps and values immediately (write() is used for bulk writes)
    auto compressed_timestamps = ts_compressor_->compress(timestamps);
    auto compressed_values = val_compressor_->compress(values);

    // Update or insert series data
    auto& data = series_[series.labels()];
    data.timestamps_compressed = std::move(compressed_timestamps);
    data.values_compressed = std::move(compressed_values);
    data.is_compressed = true;

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
        if (data.is_compressed) {
            for (uint8_t byte : data.timestamps_compressed) {
                crc = (crc << 8) ^ byte;
            }
            for (uint8_t byte : data.values_compressed) {
                crc = (crc << 8) ^ byte;
            }
        } else {
            // Hash uncompressed data
            for (int64_t ts : data.timestamps_uncompressed) {
                crc = (crc << 8) ^ static_cast<uint8_t>(ts & 0xFF);
            }
            for (double val : data.values_uncompressed) {
                uint64_t val_bits;
                std::memcpy(&val_bits, &val, sizeof(double));
                crc = (crc << 8) ^ static_cast<uint8_t>(val_bits & 0xFF);
            }
        }
    }
    return crc;
}

void BlockImpl::append(const core::Labels& labels, const core::Sample& sample) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find or create series data
    auto& series_data = series_[labels];
    
    // Buffer samples uncompressed for better compression when sealed
    series_data.timestamps_uncompressed.push_back(sample.timestamp());
    series_data.values_uncompressed.push_back(sample.value());
    series_data.is_compressed = false;
    
    // Update time range
    update_time_range(sample.timestamp());
    dirty_ = true;
}

void BlockImpl::seal() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Compress all buffered data
    for (auto& [labels, data] : series_) {
        if (!data.is_compressed && !data.timestamps_uncompressed.empty()) {
            // Compress timestamps
            data.timestamps_compressed = ts_compressor_->compress(data.timestamps_uncompressed);
            
            // Compress values
            data.values_compressed = val_compressor_->compress(data.values_uncompressed);
            
            // Clear uncompressed buffers to save memory
            data.timestamps_uncompressed.clear();
            data.timestamps_uncompressed.shrink_to_fit();
            data.values_uncompressed.clear();
            data.values_uncompressed.shrink_to_fit();
            
            data.is_compressed = true;
        }
    }
    
    sealed_ = true;
}

std::vector<uint8_t> BlockImpl::serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<uint8_t> result;
    
    // Serialize header
    const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header_);
    result.insert(result.end(), header_ptr, header_ptr + sizeof(BlockHeader));
    
    // Serialize series data
    for (const auto& [labels, data] : series_) {
        // Serialize labels
        auto labels_str = labels.to_string();
        result.insert(result.end(), labels_str.begin(), labels_str.end());
        
        if (data.is_compressed) {
            // Serialize compressed data
            result.insert(result.end(), data.timestamps_compressed.begin(), data.timestamps_compressed.end());
            result.insert(result.end(), data.values_compressed.begin(), data.values_compressed.end());
        } else {
            // Serialize uncompressed data (shouldn't happen for sealed blocks, but handle it)
            for (int64_t ts : data.timestamps_uncompressed) {
                result.insert(result.end(), 
                            reinterpret_cast<const uint8_t*>(&ts),
                            reinterpret_cast<const uint8_t*>(&ts) + sizeof(ts));
            }
            for (double val : data.values_uncompressed) {
                result.insert(result.end(), 
                            reinterpret_cast<const uint8_t*>(&val),
                            reinterpret_cast<const uint8_t*>(&val) + sizeof(val));
            }
        }
    }
    
    return result;
}

void BlockImpl::update_time_range(int64_t timestamp) {
    // Note: This method assumes the caller already holds the mutex lock
    // (e.g., called from append() which already has the lock)
    // Do NOT lock here to avoid deadlock
    
    if (header_.start_time == 0 || timestamp < header_.start_time) {
        header_.start_time = timestamp;
    }
    if (timestamp > header_.end_time) {
        header_.end_time = timestamp;
    }
}

} // namespace internal
} // namespace storage
} // namespace tsdb 