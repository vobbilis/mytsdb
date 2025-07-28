#include "tsdb/storage/delta_of_delta_encoder.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <stdexcept>

namespace tsdb {
namespace storage {
namespace internal {

DeltaOfDeltaEncoder::DeltaOfDeltaEncoder(const DeltaOfDeltaConfig& config)
    : config_(config) {
}

std::vector<uint8_t> DeltaOfDeltaEncoder::compress(const std::vector<int64_t>& timestamps) {
    if (timestamps.empty()) {
        return std::vector<uint8_t>();
    }
    
    // Reset stats
    stats_ = CompressionStats{};
    stats_.original_size = timestamps.size() * sizeof(int64_t);
    
    std::vector<uint8_t> result;
    result.reserve(timestamps.size() * sizeof(int64_t) / 2); // Estimate 50% compression
    
    // Write header
    uint32_t total_count = static_cast<uint32_t>(timestamps.size());
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&total_count),
                 reinterpret_cast<uint8_t*>(&total_count) + sizeof(total_count));
    
    // Calculate optimal block size
    uint32_t block_size = calculateOptimalBlockSize(timestamps);
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&block_size),
                 reinterpret_cast<uint8_t*>(&block_size) + sizeof(block_size));
    
    // Compress in blocks
    for (size_t i = 0; i < timestamps.size(); i += block_size) {
        size_t end_idx = std::min(i + block_size, timestamps.size());
        compressBlock(timestamps, i, end_idx, result);
        stats_.blocks_processed++;
    }
    
    stats_.compressed_size = result.size();
    stats_.compression_ratio = static_cast<double>(stats_.original_size) / stats_.compressed_size;
    
    return result;
}

std::vector<int64_t> DeltaOfDeltaEncoder::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<int64_t>();
    }
    
    if (data.size() < sizeof(uint32_t) * 2) {
        return std::vector<int64_t>();
    }
    
    std::vector<int64_t> result;
    
    // Read header
    size_t pos = 0;
    uint32_t total_count = 0;
    std::memcpy(&total_count, &data[pos], sizeof(total_count));
    pos += sizeof(total_count);
    
    uint32_t block_size = 0;
    std::memcpy(&block_size, &data[pos], sizeof(block_size));
    pos += sizeof(block_size);
    
    result.reserve(total_count);
    
    // Decompress blocks
    while (pos < data.size() && result.size() < total_count) {
        if (pos >= data.size()) break;
        
        // Read block header
        uint32_t block_count = 0;
        std::memcpy(&block_count, &data[pos], sizeof(block_count));
        pos += sizeof(block_count);
        
        if (pos >= data.size()) break;
        
        decompressBlock(data, pos, block_count, result);
    }
    
    return result;
}

std::vector<uint8_t> DeltaOfDeltaEncoder::compressWithBlockSize(const std::vector<int64_t>& timestamps, uint32_t block_size) {
    if (timestamps.empty()) {
        return std::vector<uint8_t>();
    }
    
    // Reset stats
    stats_ = CompressionStats{};
    stats_.original_size = timestamps.size() * sizeof(int64_t);
    
    std::vector<uint8_t> result;
    result.reserve(timestamps.size() * sizeof(int64_t) / 2);
    
    // Write header
    uint32_t total_count = static_cast<uint32_t>(timestamps.size());
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&total_count),
                 reinterpret_cast<uint8_t*>(&total_count) + sizeof(total_count));
    
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&block_size),
                 reinterpret_cast<uint8_t*>(&block_size) + sizeof(block_size));
    
    // Compress in blocks
    for (size_t i = 0; i < timestamps.size(); i += block_size) {
        size_t end_idx = std::min(i + block_size, timestamps.size());
        compressBlock(timestamps, i, end_idx, result);
        stats_.blocks_processed++;
    }
    
    stats_.compressed_size = result.size();
    stats_.compression_ratio = static_cast<double>(stats_.original_size) / stats_.compressed_size;
    
    return result;
}

void DeltaOfDeltaEncoder::compressBlock(const std::vector<int64_t>& timestamps, 
                                       size_t start_idx, size_t end_idx, 
                                       std::vector<uint8_t>& result) {
    if (start_idx >= end_idx || end_idx > timestamps.size()) {
        return;
    }
    
    size_t block_size = end_idx - start_idx;
    
    // Write block header
    uint32_t count = static_cast<uint32_t>(block_size);
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&count),
                 reinterpret_cast<uint8_t*>(&count) + sizeof(count));
    
    if (block_size == 0) {
        return;
    }
    
    // Write first timestamp
    int64_t first_timestamp = timestamps[start_idx];
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&first_timestamp),
                 reinterpret_cast<uint8_t*>(&first_timestamp) + sizeof(first_timestamp));
    
    if (block_size == 1) {
        return;
    }
    
    // Calculate deltas and delta-of-deltas
    std::vector<int64_t> deltas;
    std::vector<int64_t> delta_of_deltas;
    deltas.reserve(block_size - 1);
    delta_of_deltas.reserve(block_size - 2);
    
    int64_t prev_timestamp = first_timestamp;
    int64_t prev_delta = 0;
    
    for (size_t i = start_idx + 1; i < end_idx; i++) {
        int64_t delta = timestamps[i] - prev_timestamp;
        deltas.push_back(delta);
        
        if (i > start_idx + 1) {
            int64_t dod = delta - prev_delta;
            delta_of_deltas.push_back(dod);
        }
        
        prev_timestamp = timestamps[i];
        prev_delta = delta;
    }
    
    // Write first delta
    if (!deltas.empty()) {
        int64_t first_delta = deltas[0];
        result.insert(result.end(), 
                     reinterpret_cast<uint8_t*>(&first_delta),
                     reinterpret_cast<uint8_t*>(&first_delta) + sizeof(first_delta));
    }
    
    // Encode delta-of-deltas
    for (int64_t dod : delta_of_deltas) {
        encodeDeltaOfDelta(dod, result);
    }
    
    // Update stats
    if (!deltas.empty()) {
        stats_.average_delta = std::accumulate(deltas.begin(), deltas.end(), 0.0) / deltas.size();
    }
    if (!delta_of_deltas.empty()) {
        stats_.average_delta_of_delta = std::accumulate(delta_of_deltas.begin(), delta_of_deltas.end(), 0.0) / delta_of_deltas.size();
    }
}

void DeltaOfDeltaEncoder::decompressBlock(const std::vector<uint8_t>& data, 
                                         size_t& pos, uint32_t count,
                                         std::vector<int64_t>& result) {
    if (count == 0 || pos >= data.size()) {
        return;
    }
    
    // Read first timestamp
    if (pos + sizeof(int64_t) > data.size()) {
        return;
    }
    
    int64_t timestamp = 0;
    std::memcpy(&timestamp, &data[pos], sizeof(timestamp));
    pos += sizeof(timestamp);
    result.push_back(timestamp);
    
    if (count == 1) {
        return;
    }
    
    // Read first delta
    if (pos + sizeof(int64_t) > data.size()) {
        return;
    }
    
    int64_t delta = 0;
    std::memcpy(&delta, &data[pos], sizeof(delta));
    pos += sizeof(delta);
    
    timestamp += delta;
    result.push_back(timestamp);
    
    // Decode delta-of-deltas
    for (uint32_t i = 2; i < count; i++) {
        if (pos >= data.size()) {
            break;
        }
        
        int64_t dod = decodeDeltaOfDelta(data, pos);
        delta += dod;
        timestamp += delta;
        result.push_back(timestamp);
    }
}

void DeltaOfDeltaEncoder::encodeDeltaOfDelta(int64_t dod, std::vector<uint8_t>& result) {
    if (config_.enable_zigzag_encoding) {
        uint64_t encoded = zigzagEncode(dod);
        writeVarInt(encoded, result);
    } else {
        // Handle signed values without zigzag encoding
        if (dod >= 0) {
            result.push_back(0x00); // Positive flag
            writeVarInt(static_cast<uint64_t>(dod), result);
        } else {
            result.push_back(0x01); // Negative flag
            writeVarInt(static_cast<uint64_t>(-dod), result);
        }
    }
}

int64_t DeltaOfDeltaEncoder::decodeDeltaOfDelta(const std::vector<uint8_t>& data, size_t& pos) {
    if (config_.enable_zigzag_encoding) {
        uint64_t encoded = readVarInt(data, pos);
        return zigzagDecode(encoded);
    } else {
        // Handle signed values without zigzag encoding
        if (pos >= data.size()) {
            return 0;
        }
        
        uint8_t flag = data[pos++];
        uint64_t value = readVarInt(data, pos);
        
        if (flag == 0x00) {
            return static_cast<int64_t>(value);
        } else {
            return -static_cast<int64_t>(value);
        }
    }
}

uint64_t DeltaOfDeltaEncoder::zigzagEncode(int64_t value) const {
    return static_cast<uint64_t>((value << 1) ^ (value >> 63));
}

int64_t DeltaOfDeltaEncoder::zigzagDecode(uint64_t value) const {
    return static_cast<int64_t>((value >> 1) ^ (-(value & 1)));
}

void DeltaOfDeltaEncoder::writeVarInt(uint64_t value, std::vector<uint8_t>& result) {
    while (value >= 0x80) {
        result.push_back(static_cast<uint8_t>(value | 0x80));
        value >>= 7;
    }
    result.push_back(static_cast<uint8_t>(value));
}

uint64_t DeltaOfDeltaEncoder::readVarInt(const std::vector<uint8_t>& data, size_t& pos) {
    uint64_t result = 0;
    int shift = 0;
    
    while (pos < data.size()) {
        uint8_t byte = data[pos++];
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        
        if ((byte & 0x80) == 0) {
            break;
        }
        
        shift += 7;
        if (shift >= 64) {
            break; // Prevent overflow
        }
    }
    
    return result;
}

bool DeltaOfDeltaEncoder::detectRegularIntervals(const std::vector<int64_t>& timestamps,
                                                size_t start_idx, size_t end_idx) const {
    if (end_idx - start_idx < 3) {
        return true; // Consider small blocks as regular
    }
    
    // Calculate intervals
    std::vector<int64_t> intervals;
    intervals.reserve(end_idx - start_idx - 1);
    
    for (size_t i = start_idx + 1; i < end_idx; i++) {
        intervals.push_back(timestamps[i] - timestamps[i-1]);
    }
    
    if (intervals.empty()) {
        return true;
    }
    
    // Check if intervals are consistent (within 1% tolerance)
    int64_t first_interval = intervals[0];
    double tolerance = std::abs(first_interval) * 0.01;
    
    for (int64_t interval : intervals) {
        if (std::abs(interval - first_interval) > tolerance) {
            return false;
        }
    }
    
    return true;
}

uint32_t DeltaOfDeltaEncoder::calculateOptimalBlockSize(const std::vector<int64_t>& timestamps) const {
    if (timestamps.size() <= config_.min_block_size) {
        return static_cast<uint32_t>(timestamps.size());
    }
    
    // Check if timestamps have regular intervals
    bool regular = detectRegularIntervals(timestamps, 0, timestamps.size());
    
    if (regular) {
        // For regular intervals, use larger blocks
        return std::min(config_.max_block_size, static_cast<uint32_t>(timestamps.size()));
    } else {
        // For irregular intervals, use smaller blocks
        return std::min(config_.min_block_size * 2, static_cast<uint32_t>(timestamps.size()));
    }
}

// Factory implementations
std::unique_ptr<DeltaOfDeltaEncoder> DeltaOfDeltaEncoderFactory::create() {
    return std::make_unique<DeltaOfDeltaEncoder>();
}

std::unique_ptr<DeltaOfDeltaEncoder> DeltaOfDeltaEncoderFactory::create(const DeltaOfDeltaConfig& config) {
    return std::make_unique<DeltaOfDeltaEncoder>(config);
}

std::unique_ptr<DeltaOfDeltaEncoder> DeltaOfDeltaEncoderFactory::createForUseCase(const std::string& use_case) {
    DeltaOfDeltaConfig config;
    
    if (use_case == "high_frequency") {
        config.min_block_size = 128;
        config.max_block_size = 2048;
        config.compression_level = 8;
    } else if (use_case == "low_frequency") {
        config.min_block_size = 32;
        config.max_block_size = 512;
        config.compression_level = 4;
    } else if (use_case == "irregular") {
        config.min_block_size = 16;
        config.max_block_size = 256;
        config.enable_irregular_handling = true;
        config.compression_level = 6;
    }
    
    return std::make_unique<DeltaOfDeltaEncoder>(config);
}

} // namespace internal
} // namespace storage
} // namespace tsdb 