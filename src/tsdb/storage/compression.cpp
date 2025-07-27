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

    // For now, just store raw bytes
    std::vector<uint8_t> result;
    result.resize(timestamps.size() * sizeof(int64_t));
    std::memcpy(result.data(), timestamps.data(), result.size());
    return result;
}

std::vector<int64_t> SimpleTimestampCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<int64_t>();
    }

    // Validate that data size is properly aligned
    if (data.size() % sizeof(int64_t) != 0) {
        // Return empty vector for invalid data instead of causing buffer overflow
        return std::vector<int64_t>();
    }

    std::vector<int64_t> result;
    size_t count = data.size() / sizeof(int64_t);
    result.resize(count);
    std::memcpy(result.data(), data.data(), data.size());
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

    // For now, just store raw bytes
    std::vector<uint8_t> result;
    result.resize(values.size() * sizeof(double));
    std::memcpy(result.data(), values.data(), result.size());
    return result;
}

std::vector<double> SimpleValueCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<double>();
    }

    // Validate that data size is properly aligned
    if (data.size() % sizeof(double) != 0) {
        // Return empty vector for invalid data instead of causing buffer overflow
        return std::vector<double>();
    }

    std::vector<double> result;
    size_t count = data.size() / sizeof(double);
    result.resize(count);
    std::memcpy(result.data(), data.data(), data.size());
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
    // TODO: Implement Gorilla compression
    return core::Result<std::vector<uint8_t>>(data);
}

core::Result<std::vector<uint8_t>> GorillaCompressor::decompress(const std::vector<uint8_t>& data) {
    // TODO: Implement Gorilla decompression
    return core::Result<std::vector<uint8_t>>(data);
}

core::Result<size_t> GorillaCompressor::compressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = compress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error().what());
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
        return core::Result<size_t>::error(result.error().what());
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
    // TODO: Implement RLE compression
    return core::Result<std::vector<uint8_t>>(data);
}

core::Result<std::vector<uint8_t>> RLECompressor::decompress(const std::vector<uint8_t>& data) {
    // TODO: Implement RLE decompression
    return core::Result<std::vector<uint8_t>>(data);
}

core::Result<size_t> RLECompressor::compressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = compress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error().what());
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
        return core::Result<size_t>::error(result.error().what());
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
    // TODO: Implement XOR compression
    return core::Result<std::vector<uint8_t>>(data);
}

core::Result<std::vector<uint8_t>> XORCompressor::decompress(const std::vector<uint8_t>& data) {
    // TODO: Implement XOR decompression
    return core::Result<std::vector<uint8_t>>(data);
}

core::Result<size_t> XORCompressor::compressChunk(
    const uint8_t* data,
    size_t size,
    uint8_t* out,
    size_t out_size) {
    auto result = compress(std::vector<uint8_t>(data, data + size));
    if (!result.ok()) {
        return core::Result<size_t>::error(result.error().what());
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
        return core::Result<size_t>::error(result.error().what());
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

} // namespace internal
} // namespace storage
} // namespace tsdb 