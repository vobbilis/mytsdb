#include "tsdb/storage/compression.h"
#include "tsdb/core/result.h"
#include <cstring>
#include <algorithm>
#include <unordered_map>

namespace tsdb {
namespace storage {
namespace internal {

namespace {

// Portable bit manipulation functions for C++17
inline int count_leading_zeros(uint64_t x) {
    if (x == 0) return 64;
    int count = 0;
    while ((x & (1ULL << 63)) == 0) {
        count++;
        x <<= 1;
    }
    return count;
}

inline int count_trailing_zeros(uint64_t x) {
    if (x == 0) return 64;
    int count = 0;
    while ((x & 1) == 0) {
        count++;
        x >>= 1;
    }
    return count;
}

} // namespace

// SimpleTimestampCompressor implementation
std::vector<uint8_t> SimpleTimestampCompressor::compress(const std::vector<int64_t>& timestamps) {
    if (timestamps.empty()) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> result;
    result.reserve(timestamps.size() * sizeof(int64_t) + sizeof(uint32_t));
    
    // Write header: number of timestamps
    uint32_t count = static_cast<uint32_t>(timestamps.size());
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&count),
                 reinterpret_cast<uint8_t*>(&count) + sizeof(count));
    
    // Delta compression for timestamps
    int64_t prev_timestamp = timestamps[0];
    result.insert(result.end(), 
                 reinterpret_cast<const uint8_t*>(&prev_timestamp),
                 reinterpret_cast<const uint8_t*>(&prev_timestamp) + sizeof(prev_timestamp));
    
    for (size_t i = 1; i < timestamps.size(); i++) {
        int64_t delta = timestamps[i] - prev_timestamp;
        
        // Variable-length encoding for deltas
        if (delta == 0) {
            result.push_back(0x00); // 1 byte for zero delta
        } else if (delta >= -127 && delta <= 127) {
            result.push_back(0x01); // 1 byte flag
            result.push_back(static_cast<uint8_t>(delta));
        } else if (delta >= -32767 && delta <= 32767) {
            result.push_back(0x02); // 2 byte flag
            result.push_back(static_cast<uint8_t>(delta & 0xFF));
            result.push_back(static_cast<uint8_t>((delta >> 8) & 0xFF));
        } else {
            result.push_back(0x03); // 8 byte flag
            result.insert(result.end(),
                        reinterpret_cast<uint8_t*>(&delta),
                        reinterpret_cast<uint8_t*>(&delta) + sizeof(delta));
        }
        
        prev_timestamp = timestamps[i];
    }
    
    return result;
}

std::vector<int64_t> SimpleTimestampCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<int64_t>();
    }

    if (data.size() < sizeof(uint32_t)) {
        return std::vector<int64_t>();
    }

    std::vector<int64_t> result;
    
    // Read header: number of timestamps
    uint32_t count = 0;
    std::memcpy(&count, data.data(), sizeof(count));
    result.reserve(count);
    
    size_t pos = sizeof(count);
    
    // Read first timestamp
    if (pos + sizeof(int64_t) <= data.size()) {
        int64_t timestamp = 0;
        std::memcpy(&timestamp, &data[pos], sizeof(timestamp));
        result.push_back(timestamp);
        pos += sizeof(int64_t);
        
        // Decompress remaining timestamps
        while (pos < data.size() && result.size() < count) {
            if (pos >= data.size()) break;
            
            uint8_t flag = data[pos++];
            
            int64_t delta = 0;
            switch (flag) {
                case 0x00: // Zero delta
                    delta = 0;
                    break;
                case 0x01: // 1 byte delta
                    if (pos >= data.size()) break;
                    delta = static_cast<int8_t>(data[pos++]);
                    break;
                case 0x02: // 2 byte delta
                    if (pos + 1 >= data.size()) break;
                    delta = static_cast<int16_t>(data[pos] | (data[pos + 1] << 8));
                    pos += 2;
                    break;
                case 0x03: // 8 byte delta
                    if (pos + sizeof(int64_t) > data.size()) break;
                    std::memcpy(&delta, &data[pos], sizeof(delta));
                    pos += sizeof(delta);
                    break;
                default:
                    break;
            }
            
            timestamp += delta;
            result.push_back(timestamp);
        }
    }
    
    return result;
}

bool SimpleTimestampCompressor::is_compressed() const {
    return false;
}

// SimpleValueCompressor implementation
std::vector<uint8_t> SimpleValueCompressor::compress(const std::vector<double>& values) {
    if (values.empty()) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> result;
    result.reserve(values.size() * sizeof(double) + sizeof(uint32_t));
    
    // Write header: number of values
    uint32_t count = static_cast<uint32_t>(values.size());
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&count),
                 reinterpret_cast<uint8_t*>(&count) + sizeof(count));
    
    // XOR compression for double values
    uint64_t prev_value = 0;
    std::memcpy(&prev_value, &values[0], sizeof(double));
    result.insert(result.end(), 
                 reinterpret_cast<const uint8_t*>(&values[0]),
                 reinterpret_cast<const uint8_t*>(&values[0]) + sizeof(double));
    
    for (size_t i = 1; i < values.size(); i++) {
        uint64_t current_value = 0;
        std::memcpy(&current_value, &values[i], sizeof(double));
        
        uint64_t xor_value = current_value ^ prev_value;
        
        // Encode XOR value using variable-length encoding
        if (xor_value == 0) {
            result.push_back(0x00); // 1 byte for zero XOR
        } else {
            // Count leading zeros
            int leading_zeros = count_leading_zeros(xor_value);
            int trailing_zeros = count_trailing_zeros(xor_value);
            int significant_bits = 64 - leading_zeros - trailing_zeros;
            
            if (significant_bits <= 8) {
                result.push_back(0x01); // 1 byte flag
                result.push_back(static_cast<uint8_t>(xor_value >> trailing_zeros));
            } else if (significant_bits <= 16) {
                result.push_back(0x02); // 2 byte flag
                uint16_t encoded = static_cast<uint16_t>(xor_value >> trailing_zeros);
                result.push_back(static_cast<uint8_t>(encoded & 0xFF));
                result.push_back(static_cast<uint8_t>((encoded >> 8) & 0xFF));
            } else if (significant_bits <= 32) {
                result.push_back(0x03); // 4 byte flag
                uint32_t encoded = static_cast<uint32_t>(xor_value >> trailing_zeros);
                result.insert(result.end(),
                            reinterpret_cast<uint8_t*>(&encoded),
                            reinterpret_cast<uint8_t*>(&encoded) + sizeof(encoded));
            } else {
                result.push_back(0x04); // 8 byte flag
                result.insert(result.end(),
                            reinterpret_cast<uint8_t*>(&xor_value),
                            reinterpret_cast<uint8_t*>(&xor_value) + sizeof(xor_value));
            }
        }
        
        prev_value = current_value;
    }
    
    return result;
}

std::vector<double> SimpleValueCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<double>();
    }

    if (data.size() < sizeof(uint32_t)) {
        return std::vector<double>();
    }

    std::vector<double> result;
    
    // Read header: number of values
    uint32_t count = 0;
    std::memcpy(&count, data.data(), sizeof(count));
    result.reserve(count);
    
    size_t pos = sizeof(count);
    
    // Read first value
    if (pos + sizeof(double) <= data.size()) {
        double value = 0.0;
        std::memcpy(&value, &data[pos], sizeof(double));
        result.push_back(value);
        pos += sizeof(double);
        
        uint64_t prev_value = 0;
        std::memcpy(&prev_value, &value, sizeof(double));
        
        // Decompress remaining values
        while (pos < data.size() && result.size() < count) {
            if (pos >= data.size()) break;
            
            uint8_t flag = data[pos++];
            
            uint64_t xor_value = 0;
            switch (flag) {
                case 0x00: // Zero XOR
                    xor_value = 0;
                    break;
                case 0x01: // 1 byte XOR
                    if (pos >= data.size()) break;
                    xor_value = static_cast<uint8_t>(data[pos++]);
                    break;
                case 0x02: // 2 byte XOR
                    if (pos + 1 >= data.size()) break;
                    xor_value = static_cast<uint16_t>(data[pos] | (data[pos + 1] << 8));
                    pos += 2;
                    break;
                case 0x03: // 4 byte XOR
                    if (pos + sizeof(uint32_t) > data.size()) break;
                    std::memcpy(&xor_value, &data[pos], sizeof(uint32_t));
                    pos += sizeof(uint32_t);
                    break;
                case 0x04: // 8 byte XOR
                    if (pos + sizeof(uint64_t) > data.size()) break;
                    std::memcpy(&xor_value, &data[pos], sizeof(uint64_t));
                    pos += sizeof(uint64_t);
                    break;
                default:
                    break;
            }
            
            uint64_t current_value = prev_value ^ xor_value;
            double current_double = 0.0;
            std::memcpy(&current_double, &current_value, sizeof(double));
            result.push_back(current_double);
            prev_value = current_value;
        }
    }
    
    return result;
}

bool SimpleValueCompressor::is_compressed() const {
    return false;
}

// SimpleLabelCompressor implementation
uint32_t SimpleLabelCompressor::add_label(const std::string& label) {
    auto it = label_to_id_.find(label);
    if (it != label_to_id_.end()) {
        return it->second;
    }
    uint32_t id = static_cast<uint32_t>(id_to_label_.size());
    label_to_id_[label] = id;
    id_to_label_.push_back(label);
    return id;
}

std::string SimpleLabelCompressor::get_label(uint32_t id) const {
    if (id >= id_to_label_.size()) {
        return "";
    }
    return id_to_label_[id];
}

std::vector<uint8_t> SimpleLabelCompressor::compress(const core::Labels& labels) {
    std::vector<uint8_t> result;
    for (const auto& [name, value] : labels.map()) {
        uint32_t name_id = add_label(name);
        uint32_t value_id = add_label(value);
        result.resize(result.size() + 2 * sizeof(uint32_t));
        std::memcpy(result.data() + result.size() - 2 * sizeof(uint32_t), &name_id, sizeof(uint32_t));
        std::memcpy(result.data() + result.size() - sizeof(uint32_t), &value_id, sizeof(uint32_t));
    }
    return result;
}

core::Labels SimpleLabelCompressor::decompress(const std::vector<uint8_t>& data) {
    core::Labels::Map result_map;
    for (size_t i = 0; i < data.size(); i += 2 * sizeof(uint32_t)) {
        uint32_t name_id, value_id;
        std::memcpy(&name_id, data.data() + i, sizeof(uint32_t));
        std::memcpy(&value_id, data.data() + i + sizeof(uint32_t), sizeof(uint32_t));
        result_map[get_label(name_id)] = get_label(value_id);
    }
    return core::Labels(result_map);
}

size_t SimpleLabelCompressor::dictionary_size() const {
    return id_to_label_.size();
}

void SimpleLabelCompressor::clear() {
    label_to_id_.clear();
    id_to_label_.clear();
}

// GorillaCompressor implementation
core::Result<std::vector<uint8_t>> GorillaCompressor::compress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return core::Result<std::vector<uint8_t>>(std::vector<uint8_t>());
    }

    // For Gorilla compression, we expect the data to be a sequence of timestamps and values
    // We'll implement a simplified version that compresses timestamps using delta encoding
    // and values using XOR compression
    
    std::vector<uint8_t> compressed;
    compressed.reserve(data.size()); // Reserve space, will be smaller
    
    // Write header: original size
    uint32_t original_size = static_cast<uint32_t>(data.size());
    compressed.insert(compressed.end(), 
                     reinterpret_cast<uint8_t*>(&original_size),
                     reinterpret_cast<uint8_t*>(&original_size) + sizeof(original_size));
    
    // Simple delta compression for timestamps
    // Assume data contains 64-bit timestamps
    if (data.size() >= sizeof(int64_t)) {
        int64_t prev_timestamp = 0;
        std::memcpy(&prev_timestamp, data.data(), sizeof(int64_t));
        
        // Write first timestamp as-is
        compressed.insert(compressed.end(), data.begin(), data.begin() + sizeof(int64_t));
        
        // Compress remaining timestamps using delta encoding
        for (size_t i = sizeof(int64_t); i < data.size(); i += sizeof(int64_t)) {
            if (i + sizeof(int64_t) <= data.size()) {
                int64_t current_timestamp = 0;
                std::memcpy(&current_timestamp, &data[i], sizeof(int64_t));
                
                int64_t delta = current_timestamp - prev_timestamp;
                
                // Encode delta using variable-length encoding
                if (delta == 0) {
                    compressed.push_back(0x00); // 1 byte for zero delta
                } else if (delta >= -127 && delta <= 127) {
                    compressed.push_back(0x01); // 1 byte flag
                    compressed.push_back(static_cast<uint8_t>(delta));
                } else if (delta >= -32767 && delta <= 32767) {
                    compressed.push_back(0x02); // 2 byte flag
                    compressed.push_back(static_cast<uint8_t>(delta & 0xFF));
                    compressed.push_back(static_cast<uint8_t>((delta >> 8) & 0xFF));
                } else {
                    compressed.push_back(0x03); // 8 byte flag
                    compressed.insert(compressed.end(),
                                    reinterpret_cast<uint8_t*>(&delta),
                                    reinterpret_cast<uint8_t*>(&delta) + sizeof(delta));
                }
                
                prev_timestamp = current_timestamp;
            }
        }
    }
    
    return core::Result<std::vector<uint8_t>>(std::move(compressed));
}

core::Result<std::vector<uint8_t>> GorillaCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return core::Result<std::vector<uint8_t>>(std::vector<uint8_t>());
    }
    
    if (data.size() < sizeof(uint32_t)) {
        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: too small");
    }
    
    std::vector<uint8_t> decompressed;
    
    // Read header: original size
    uint32_t original_size = 0;
    std::memcpy(&original_size, data.data(), sizeof(original_size));
    decompressed.reserve(original_size);
    
    size_t pos = sizeof(original_size);
    
    // Read first timestamp
    if (pos + sizeof(int64_t) <= data.size()) {
        decompressed.insert(decompressed.end(), 
                           data.begin() + pos, 
                           data.begin() + pos + sizeof(int64_t));
        pos += sizeof(int64_t);
        
        int64_t prev_timestamp = 0;
        std::memcpy(&prev_timestamp, decompressed.data(), sizeof(int64_t));
        
        // Decompress remaining timestamps
        while (pos < data.size() && decompressed.size() < original_size) {
            if (pos >= data.size()) break;
            
            uint8_t flag = data[pos++];
            
            int64_t delta = 0;
            switch (flag) {
                case 0x00: // Zero delta
                    delta = 0;
                    break;
                case 0x01: // 1 byte delta
                    if (pos >= data.size()) {
                        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: truncated");
                    }
                    delta = static_cast<int8_t>(data[pos++]);
                    break;
                case 0x02: // 2 byte delta
                    if (pos + 1 >= data.size()) {
                        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: truncated");
                    }
                    delta = static_cast<int16_t>(data[pos] | (data[pos + 1] << 8));
                    pos += 2;
                    break;
                case 0x03: // 8 byte delta
                    if (pos + sizeof(int64_t) > data.size()) {
                        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: truncated");
                    }
                    std::memcpy(&delta, &data[pos], sizeof(delta));
                    pos += sizeof(delta);
                    break;
                default:
                    return core::Result<std::vector<uint8_t>>::error("Invalid compression flag");
            }
            
            int64_t current_timestamp = prev_timestamp + delta;
            decompressed.insert(decompressed.end(),
                              reinterpret_cast<uint8_t*>(&current_timestamp),
                              reinterpret_cast<uint8_t*>(&current_timestamp) + sizeof(current_timestamp));
            prev_timestamp = current_timestamp;
        }
    }
    
    return core::Result<std::vector<uint8_t>>(std::move(decompressed));
}

core::Result<size_t> GorillaCompressor::compressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = compress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error());
    }
    
    const auto& compressed = result.value();
    if (compressed.size() > out_size) {
        return core::Result<size_t>::error("Output buffer too small");
    }
    
    std::memcpy(out, compressed.data(), compressed.size());
    return core::Result<size_t>(compressed.size());
}

core::Result<size_t> GorillaCompressor::decompressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = decompress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error());
    }
    
    const auto& decompressed = result.value();
    if (decompressed.size() > out_size) {
        return core::Result<size_t>::error("Output buffer too small");
    }
    
    std::memcpy(out, decompressed.data(), decompressed.size());
    return core::Result<size_t>(decompressed.size());
}

bool GorillaCompressor::is_compressed() const {
    return true;
}

// RLECompressor implementation
core::Result<std::vector<uint8_t>> RLECompressor::compress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return core::Result<std::vector<uint8_t>>(std::vector<uint8_t>());
    }

    std::vector<uint8_t> compressed;
    compressed.reserve(data.size() + sizeof(uint32_t)); // Reserve space for header
    
    // Write header: original size
    uint32_t original_size = static_cast<uint32_t>(data.size());
    compressed.insert(compressed.end(), 
                     reinterpret_cast<uint8_t*>(&original_size),
                     reinterpret_cast<uint8_t*>(&original_size) + sizeof(original_size));
    
    // Run-Length Encoding
    uint8_t current_byte = data[0];
    uint32_t run_length = 1;
    
    for (size_t i = 1; i < data.size(); i++) {
        if (data[i] == current_byte && run_length < 65535) {
            run_length++;
        } else {
            // Write run - handle runs longer than 255
            while (run_length > 0) {
                uint16_t chunk_size = (run_length > 255) ? 255 : static_cast<uint16_t>(run_length);
                compressed.push_back(current_byte);
                compressed.push_back(static_cast<uint8_t>(chunk_size));
                run_length -= chunk_size;
            }
            
            // Start new run
            current_byte = data[i];
            run_length = 1;
        }
    }
    
    // Write final run
    while (run_length > 0) {
        uint16_t chunk_size = (run_length > 255) ? 255 : static_cast<uint16_t>(run_length);
        compressed.push_back(current_byte);
        compressed.push_back(static_cast<uint8_t>(chunk_size));
        run_length -= chunk_size;
    }
    
    return core::Result<std::vector<uint8_t>>(std::move(compressed));
}

core::Result<std::vector<uint8_t>> RLECompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return core::Result<std::vector<uint8_t>>(std::vector<uint8_t>());
    }
    
    if (data.size() < sizeof(uint32_t)) {
        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: too small");
    }
    
    std::vector<uint8_t> decompressed;
    
    // Read header: original size
    uint32_t original_size = 0;
    std::memcpy(&original_size, data.data(), sizeof(original_size));
    decompressed.reserve(original_size);
    
    size_t pos = sizeof(original_size);
    
    // Decompress RLE data
    while (pos + 1 < data.size()) {
        uint8_t value = data[pos++];
        uint8_t count = data[pos++];
        
        for (uint8_t i = 0; i < count; i++) {
            decompressed.push_back(value);
        }
    }
    
    return core::Result<std::vector<uint8_t>>(std::move(decompressed));
}

core::Result<size_t> RLECompressor::compressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = compress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error());
    }
    
    const auto& compressed = result.value();
    if (compressed.size() > out_size) {
        return core::Result<size_t>::error("Output buffer too small");
    }
    
    std::memcpy(out, compressed.data(), compressed.size());
    return core::Result<size_t>(compressed.size());
}

core::Result<size_t> RLECompressor::decompressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = decompress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error());
    }
    
    const auto& decompressed = result.value();
    if (decompressed.size() > out_size) {
        return core::Result<size_t>::error("Output buffer too small");
    }
    
    std::memcpy(out, decompressed.data(), decompressed.size());
    return core::Result<size_t>(decompressed.size());
}

bool RLECompressor::is_compressed() const {
    return true;
}

// XORCompressor implementation
core::Result<std::vector<uint8_t>> XORCompressor::compress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return core::Result<std::vector<uint8_t>>(std::vector<uint8_t>());
    }

    std::vector<uint8_t> compressed;
    compressed.reserve(data.size() + sizeof(uint32_t)); // Reserve space for header
    
    // Write header: original size
    uint32_t original_size = static_cast<uint32_t>(data.size());
    compressed.insert(compressed.end(), 
                     reinterpret_cast<uint8_t*>(&original_size),
                     reinterpret_cast<uint8_t*>(&original_size) + sizeof(original_size));
    
    // For timestamp data, use delta encoding instead of XOR
    // Assume data contains 64-bit timestamps
    if (data.size() >= sizeof(int64_t)) {
        int64_t prev_timestamp = 0;
        std::memcpy(&prev_timestamp, data.data(), sizeof(int64_t));
        
        // Write first timestamp as-is
        compressed.insert(compressed.end(), data.begin(), data.begin() + sizeof(int64_t));
        
        // Compress remaining timestamps using delta encoding
        for (size_t i = sizeof(int64_t); i < data.size(); i += sizeof(int64_t)) {
            if (i + sizeof(int64_t) <= data.size()) {
                int64_t current_timestamp = 0;
                std::memcpy(&current_timestamp, &data[i], sizeof(int64_t));
                
                int64_t delta = current_timestamp - prev_timestamp;
                
                // Encode delta using variable-length encoding
                if (delta == 0) {
                    compressed.push_back(0x00); // 1 byte for zero delta
                } else if (delta >= -127 && delta <= 127) {
                    compressed.push_back(0x01); // 1 byte flag
                    compressed.push_back(static_cast<uint8_t>(delta));
                } else if (delta >= -32767 && delta <= 32767) {
                    compressed.push_back(0x02); // 2 byte flag
                    compressed.push_back(static_cast<uint8_t>(delta & 0xFF));
                    compressed.push_back(static_cast<uint8_t>((delta >> 8) & 0xFF));
                } else {
                    compressed.push_back(0x03); // 8 byte flag
                    compressed.insert(compressed.end(),
                                    reinterpret_cast<uint8_t*>(&delta),
                                    reinterpret_cast<uint8_t*>(&delta) + sizeof(delta));
                }
                
                prev_timestamp = current_timestamp;
            }
        }
    }
    
    return core::Result<std::vector<uint8_t>>(std::move(compressed));
}

core::Result<std::vector<uint8_t>> XORCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return core::Result<std::vector<uint8_t>>(std::vector<uint8_t>());
    }
    
    if (data.size() < sizeof(uint32_t)) {
        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: too small");
    }
    
    std::vector<uint8_t> decompressed;
    
    // Read header: original size
    uint32_t original_size = 0;
    std::memcpy(&original_size, data.data(), sizeof(original_size));
    decompressed.reserve(original_size);
    
    size_t pos = sizeof(original_size);
    
    // Read first timestamp
    if (pos + sizeof(int64_t) <= data.size()) {
        decompressed.insert(decompressed.end(), 
                           data.begin() + pos, 
                           data.begin() + pos + sizeof(int64_t));
        pos += sizeof(int64_t);
        
        int64_t prev_timestamp = 0;
        std::memcpy(&prev_timestamp, decompressed.data(), sizeof(int64_t));
        
        // Decompress remaining timestamps
        while (pos < data.size() && decompressed.size() < original_size) {
            if (pos >= data.size()) break;
            
            uint8_t flag = data[pos++];
            
            int64_t delta = 0;
            switch (flag) {
                case 0x00: // Zero delta
                    delta = 0;
                    break;
                case 0x01: // 1 byte delta
                    if (pos >= data.size()) {
                        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: truncated");
                    }
                    delta = static_cast<int8_t>(data[pos++]);
                    break;
                case 0x02: // 2 byte delta
                    if (pos + 1 >= data.size()) {
                        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: truncated");
                    }
                    delta = static_cast<int16_t>(data[pos] | (data[pos + 1] << 8));
                    pos += 2;
                    break;
                case 0x03: // 8 byte delta
                    if (pos + sizeof(int64_t) > data.size()) {
                        return core::Result<std::vector<uint8_t>>::error("Invalid compressed data: truncated");
                    }
                    std::memcpy(&delta, &data[pos], sizeof(delta));
                    pos += sizeof(delta);
                    break;
                default:
                    return core::Result<std::vector<uint8_t>>::error("Invalid compression flag");
            }
            
            int64_t current_timestamp = prev_timestamp + delta;
            decompressed.insert(decompressed.end(),
                              reinterpret_cast<uint8_t*>(&current_timestamp),
                              reinterpret_cast<uint8_t*>(&current_timestamp) + sizeof(current_timestamp));
            prev_timestamp = current_timestamp;
        }
    }
    
    return core::Result<std::vector<uint8_t>>(std::move(decompressed));
}

core::Result<size_t> XORCompressor::compressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = compress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error());
    }
    
    const auto& compressed = result.value();
    if (compressed.size() > out_size) {
        return core::Result<size_t>::error("Output buffer too small");
    }
    
    std::memcpy(out, compressed.data(), compressed.size());
    return core::Result<size_t>(compressed.size());
}

core::Result<size_t> XORCompressor::decompressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = decompress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error());
    }
    
    const auto& decompressed = result.value();
    if (decompressed.size() > out_size) {
        return core::Result<size_t>::error("Output buffer too small");
    }
    
    std::memcpy(out, decompressed.data(), decompressed.size());
    return core::Result<size_t>(decompressed.size());
}

bool XORCompressor::is_compressed() const {
    return true;
}

// Factory function implementations
std::unique_ptr<TimestampCompressor> create_timestamp_compressor() {
    return std::make_unique<SimpleTimestampCompressor>();
}

std::unique_ptr<ValueCompressor> create_value_compressor() {
    return std::make_unique<SimpleValueCompressor>();
}

std::unique_ptr<LabelCompressor> create_label_compressor() {
    return std::make_unique<SimpleLabelCompressor>();
}

std::unique_ptr<Compressor> create_gorilla_compressor() {
    return std::make_unique<GorillaCompressor>();
}

std::unique_ptr<Compressor> create_rle_compressor() {
    return std::make_unique<RLECompressor>();
}

std::unique_ptr<Compressor> create_xor_compressor() {
    return std::make_unique<XORCompressor>();
}

// Concrete CompressorFactory implementation
class CompressorFactoryImpl : public CompressorFactory {
public:
    std::unique_ptr<TimestampCompressor> create_timestamp_compressor() override {
        return std::make_unique<SimpleTimestampCompressor>();
    }
    
    std::unique_ptr<ValueCompressor> create_value_compressor() override {
        return std::make_unique<SimpleValueCompressor>();
    }
    
    std::unique_ptr<LabelCompressor> create_label_compressor() override {
        return std::make_unique<SimpleLabelCompressor>();
    }
    
    std::unique_ptr<Compressor> create_compressor(CompressionConfig::Algorithm algo) override {
        switch (algo) {
            case CompressionConfig::Algorithm::GORILLA:
                return std::make_unique<GorillaCompressor>();
            case CompressionConfig::Algorithm::DELTA_XOR:
                return std::make_unique<XORCompressor>();
            case CompressionConfig::Algorithm::RLE:
                return std::make_unique<RLECompressor>();
            case CompressionConfig::Algorithm::DICTIONARY:
                // For now, use RLE as a placeholder for dictionary compression
                return std::make_unique<RLECompressor>();
            case CompressionConfig::Algorithm::NONE:
            default:
                // Return a no-op compressor that doesn't actually compress
                return std::make_unique<XORCompressor>(); // Placeholder
        }
    }
};

std::unique_ptr<CompressorFactory> create_compressor_factory() {
    return std::make_unique<CompressorFactoryImpl>();
}

// Adapter class implementations

// GorillaTimestampCompressor implementation
std::vector<uint8_t> GorillaTimestampCompressor::compress(const std::vector<int64_t>& timestamps) {
    if (timestamps.empty()) {
        return std::vector<uint8_t>();
    }
    
    // Convert timestamps to byte vector
    std::vector<uint8_t> data;
    data.reserve(timestamps.size() * sizeof(int64_t));
    for (const auto& timestamp : timestamps) {
        data.insert(data.end(), 
                   reinterpret_cast<const uint8_t*>(&timestamp),
                   reinterpret_cast<const uint8_t*>(&timestamp) + sizeof(timestamp));
    }
    
    // Compress using Gorilla compressor
    auto result = gorilla_compressor_->compress(data);
    if (!result.ok()) {
        return std::vector<uint8_t>();
    }
    
    return result.value();
}

std::vector<int64_t> GorillaTimestampCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<int64_t>();
    }
    
    // Decompress using Gorilla compressor
    auto result = gorilla_compressor_->decompress(data);
    if (!result.ok()) {
        return std::vector<int64_t>();
    }
    
    // Convert byte vector back to timestamps
    const auto& decompressed = result.value();
    std::vector<int64_t> timestamps;
    timestamps.reserve(decompressed.size() / sizeof(int64_t));
    
    for (size_t i = 0; i < decompressed.size(); i += sizeof(int64_t)) {
        int64_t timestamp;
        std::memcpy(&timestamp, &decompressed[i], sizeof(timestamp));
        timestamps.push_back(timestamp);
    }
    
    return timestamps;
}

// XORTimestampCompressor implementation
std::vector<uint8_t> XORTimestampCompressor::compress(const std::vector<int64_t>& timestamps) {
    if (timestamps.empty()) {
        return std::vector<uint8_t>();
    }
    
    // Convert timestamps to byte vector
    std::vector<uint8_t> data;
    data.reserve(timestamps.size() * sizeof(int64_t));
    for (const auto& timestamp : timestamps) {
        data.insert(data.end(), 
                   reinterpret_cast<const uint8_t*>(&timestamp),
                   reinterpret_cast<const uint8_t*>(&timestamp) + sizeof(timestamp));
    }
    
    // Compress using XOR compressor
    auto result = xor_compressor_->compress(data);
    if (!result.ok()) {
        return std::vector<uint8_t>();
    }
    
    return result.value();
}

std::vector<int64_t> XORTimestampCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<int64_t>();
    }
    
    // Decompress using XOR compressor
    auto result = xor_compressor_->decompress(data);
    if (!result.ok()) {
        return std::vector<int64_t>();
    }
    
    // Convert byte vector back to timestamps
    const auto& decompressed = result.value();
    std::vector<int64_t> timestamps;
    timestamps.reserve(decompressed.size() / sizeof(int64_t));
    
    for (size_t i = 0; i < decompressed.size(); i += sizeof(int64_t)) {
        int64_t timestamp;
        std::memcpy(&timestamp, &decompressed[i], sizeof(timestamp));
        timestamps.push_back(timestamp);
    }
    
    return timestamps;
}

// RLETimestampCompressor implementation
std::vector<uint8_t> RLETimestampCompressor::compress(const std::vector<int64_t>& timestamps) {
    if (timestamps.empty()) {
        return std::vector<uint8_t>();
    }
    
    // Convert timestamps to byte vector
    std::vector<uint8_t> data;
    data.reserve(timestamps.size() * sizeof(int64_t));
    for (const auto& timestamp : timestamps) {
        data.insert(data.end(), 
                   reinterpret_cast<const uint8_t*>(&timestamp),
                   reinterpret_cast<const uint8_t*>(&timestamp) + sizeof(timestamp));
    }
    
    // Compress using RLE compressor
    auto result = rle_compressor_->compress(data);
    if (!result.ok()) {
        return std::vector<uint8_t>();
    }
    
    return result.value();
}

std::vector<int64_t> RLETimestampCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<int64_t>();
    }
    
    // Decompress using RLE compressor
    auto result = rle_compressor_->decompress(data);
    if (!result.ok()) {
        return std::vector<int64_t>();
    }
    
    // Convert byte vector back to timestamps
    const auto& decompressed = result.value();
    std::vector<int64_t> timestamps;
    timestamps.reserve(decompressed.size() / sizeof(int64_t));
    
    for (size_t i = 0; i < decompressed.size(); i += sizeof(int64_t)) {
        int64_t timestamp;
        std::memcpy(&timestamp, &decompressed[i], sizeof(timestamp));
        timestamps.push_back(timestamp);
    }
    
    return timestamps;
}

// GorillaValueCompressor implementation
std::vector<uint8_t> GorillaValueCompressor::compress(const std::vector<double>& values) {
    if (values.empty()) {
        return std::vector<uint8_t>();
    }
    
    // Convert values to byte vector
    std::vector<uint8_t> data;
    data.reserve(values.size() * sizeof(double));
    for (const auto& value : values) {
        data.insert(data.end(), 
                   reinterpret_cast<const uint8_t*>(&value),
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
    }
    
    // Compress using Gorilla compressor
    auto result = gorilla_compressor_->compress(data);
    if (!result.ok()) {
        return std::vector<uint8_t>();
    }
    
    return result.value();
}

std::vector<double> GorillaValueCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<double>();
    }
    
    // Decompress using Gorilla compressor
    auto result = gorilla_compressor_->decompress(data);
    if (!result.ok()) {
        return std::vector<double>();
    }
    
    // Convert byte vector back to values
    const auto& decompressed = result.value();
    std::vector<double> values;
    values.reserve(decompressed.size() / sizeof(double));
    
    for (size_t i = 0; i < decompressed.size(); i += sizeof(double)) {
        double value;
        std::memcpy(&value, &decompressed[i], sizeof(value));
        values.push_back(value);
    }
    
    return values;
}

// XORValueCompressor implementation
std::vector<uint8_t> XORValueCompressor::compress(const std::vector<double>& values) {
    if (values.empty()) {
        return std::vector<uint8_t>();
    }
    
    // Convert values to byte vector
    std::vector<uint8_t> data;
    data.reserve(values.size() * sizeof(double));
    for (const auto& value : values) {
        data.insert(data.end(), 
                   reinterpret_cast<const uint8_t*>(&value),
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
    }
    
    // Compress using XOR compressor
    auto result = xor_compressor_->compress(data);
    if (!result.ok()) {
        return std::vector<uint8_t>();
    }
    
    return result.value();
}

std::vector<double> XORValueCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<double>();
    }
    
    // Decompress using XOR compressor
    auto result = xor_compressor_->decompress(data);
    if (!result.ok()) {
        return std::vector<double>();
    }
    
    // Convert byte vector back to values
    const auto& decompressed = result.value();
    std::vector<double> values;
    values.reserve(decompressed.size() / sizeof(double));
    
    for (size_t i = 0; i < decompressed.size(); i += sizeof(double)) {
        double value;
        std::memcpy(&value, &decompressed[i], sizeof(value));
        values.push_back(value);
    }
    
    return values;
}

// RLEValueCompressor implementation
std::vector<uint8_t> RLEValueCompressor::compress(const std::vector<double>& values) {
    if (values.empty()) {
        return std::vector<uint8_t>();
    }
    
    // Convert values to byte vector
    std::vector<uint8_t> data;
    data.reserve(values.size() * sizeof(double));
    for (const auto& value : values) {
        data.insert(data.end(), 
                   reinterpret_cast<const uint8_t*>(&value),
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
    }
    
    // Compress using RLE compressor
    auto result = rle_compressor_->compress(data);
    if (!result.ok()) {
        return std::vector<uint8_t>();
    }
    
    return result.value();
}

std::vector<double> RLEValueCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<double>();
    }
    
    // Decompress using RLE compressor
    auto result = rle_compressor_->decompress(data);
    if (!result.ok()) {
        return std::vector<double>();
    }
    
    // Convert byte vector back to values
    const auto& decompressed = result.value();
    std::vector<double> values;
    values.reserve(decompressed.size() / sizeof(double));
    
    for (size_t i = 0; i < decompressed.size(); i += sizeof(double)) {
        double value;
        std::memcpy(&value, &decompressed[i], sizeof(value));
        values.push_back(value);
    }
    
    return values;
}

// RLELabelCompressor implementation
std::vector<uint8_t> RLELabelCompressor::compress(const core::Labels& labels) {
    // Convert labels to byte vector
    std::string label_string = labels.to_string();
    std::vector<uint8_t> data(label_string.begin(), label_string.end());
    
    // Compress using RLE compressor
    auto result = rle_compressor_->compress(data);
    if (!result.ok()) {
        return std::vector<uint8_t>();
    }
    
    return result.value();
}

core::Labels RLELabelCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return core::Labels();
    }
    
    // Decompress using RLE compressor
    auto result = rle_compressor_->decompress(data);
    if (!result.ok()) {
        return core::Labels();
    }
    
    // Convert byte vector back to labels
    const auto& decompressed = result.value();
    std::string label_string(decompressed.begin(), decompressed.end());
    
    // Parse label string back to Labels object
    // This is a simplified implementation - in practice you'd want proper parsing
    return core::Labels(); // Placeholder
}

// DictionaryLabelCompressor implementation
std::vector<uint8_t> DictionaryLabelCompressor::compress(const core::Labels& labels) {
    std::vector<uint8_t> result;
    for (const auto& [name, value] : labels.map()) {
        uint32_t name_id = add_label(name);
        uint32_t value_id = add_label(value);
        result.resize(result.size() + 2 * sizeof(uint32_t));
        std::memcpy(result.data() + result.size() - 2 * sizeof(uint32_t), &name_id, sizeof(uint32_t));
        std::memcpy(result.data() + result.size() - sizeof(uint32_t), &value_id, sizeof(uint32_t));
    }
    return result;
}

core::Labels DictionaryLabelCompressor::decompress(const std::vector<uint8_t>& data) {
    core::Labels::Map result_map;
    for (size_t i = 0; i < data.size(); i += 2 * sizeof(uint32_t)) {
        uint32_t name_id, value_id;
        std::memcpy(&name_id, data.data() + i, sizeof(uint32_t));
        std::memcpy(&value_id, data.data() + i + sizeof(uint32_t), sizeof(uint32_t));
        result_map[get_label(name_id)] = get_label(value_id);
    }
    return core::Labels(result_map);
}

uint32_t DictionaryLabelCompressor::add_label(const std::string& label) {
    auto it = label_to_id_.find(label);
    if (it != label_to_id_.end()) {
        return it->second;
    }
    uint32_t id = static_cast<uint32_t>(id_to_label_.size());
    label_to_id_[label] = id;
    id_to_label_.push_back(label);
    return id;
}

std::string DictionaryLabelCompressor::get_label(uint32_t id) const {
    if (id >= id_to_label_.size()) {
        return "";
    }
    return id_to_label_[id];
}

size_t DictionaryLabelCompressor::dictionary_size() const {
    return id_to_label_.size();
}

void DictionaryLabelCompressor::clear() {
    label_to_id_.clear();
    id_to_label_.clear();
}

} // namespace internal
} // namespace storage
} // namespace tsdb 