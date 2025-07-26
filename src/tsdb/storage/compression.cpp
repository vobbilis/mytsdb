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
class SimpleTimestampCompressorImpl : public TimestampCompressor {
public:
    std::vector<uint8_t> compress(const std::vector<int64_t>& timestamps) override {
        if (timestamps.empty()) {
            return std::vector<uint8_t>();
        }

        // For now, just store raw bytes
        std::vector<uint8_t> result;
        result.resize(timestamps.size() * sizeof(int64_t));
        std::memcpy(result.data(), timestamps.data(), result.size());
        return result;
    }

    std::vector<int64_t> decompress(const std::vector<uint8_t>& data) override {
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

    bool is_compressed() const override {
        return false;
    }
};

// SimpleValueCompressor implementation
class SimpleValueCompressorImpl : public ValueCompressor {
public:
    std::vector<uint8_t> compress(const std::vector<double>& values) override {
        if (values.empty()) {
            return std::vector<uint8_t>();
        }

        // For now, just store raw bytes
        std::vector<uint8_t> result;
        result.resize(values.size() * sizeof(double));
        std::memcpy(result.data(), values.data(), result.size());
        return result;
    }

    std::vector<double> decompress(const std::vector<uint8_t>& data) override {
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

    bool is_compressed() const override {
        return false;
    }
};

// SimpleLabelCompressor implementation
class SimpleLabelCompressorImpl : public LabelCompressor {
public:
    uint32_t add_label(const std::string& label) override {
        auto it = label_to_id_.find(label);
        if (it != label_to_id_.end()) {
            return it->second;
        }
        uint32_t id = static_cast<uint32_t>(id_to_label_.size());
        label_to_id_[label] = id;
        id_to_label_.push_back(label);
        return id;
    }

    std::string get_label(uint32_t id) const override {
        if (id >= id_to_label_.size()) {
            return "";
        }
        return id_to_label_[id];
    }

    std::vector<uint8_t> compress(const core::Labels& labels) override {
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

    core::Labels decompress(const std::vector<uint8_t>& data) override {
        core::Labels::Map result_map;
        for (size_t i = 0; i < data.size(); i += 2 * sizeof(uint32_t)) {
            uint32_t name_id, value_id;
            std::memcpy(&name_id, data.data() + i, sizeof(uint32_t));
            std::memcpy(&value_id, data.data() + i + sizeof(uint32_t), sizeof(uint32_t));
            result_map[get_label(name_id)] = get_label(value_id);
        }
        return core::Labels(result_map);
    }

    size_t dictionary_size() const override {
        return id_to_label_.size();
    }

    void clear() override {
        label_to_id_.clear();
        id_to_label_.clear();
    }

private:
    std::unordered_map<std::string, uint32_t> label_to_id_;
    std::vector<std::string> id_to_label_;
};

// GorillaCompressor implementation
class GorillaCompressorImpl : public Compressor {
public:
    core::Result<std::vector<uint8_t>> compress(const std::vector<uint8_t>& data) override {
        // TODO: Implement Gorilla compression
        return core::Result<std::vector<uint8_t>>(data);
    }

    core::Result<std::vector<uint8_t>> decompress(const std::vector<uint8_t>& data) override {
        // TODO: Implement Gorilla decompression
        return core::Result<std::vector<uint8_t>>(data);
    }

    core::Result<size_t> compressChunk(
        const uint8_t* data,
        size_t size,
        uint8_t* out,
        size_t out_size) override {
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

    core::Result<size_t> decompressChunk(
        const uint8_t* data,
        size_t size,
        uint8_t* out,
        size_t out_size) override {
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

    bool is_compressed() const override {
        return true;
    }
};

// RLECompressor implementation
class RLECompressorImpl : public Compressor {
public:
    core::Result<std::vector<uint8_t>> compress(const std::vector<uint8_t>& data) override {
        // TODO: Implement RLE compression
        return core::Result<std::vector<uint8_t>>(data);
    }

    core::Result<std::vector<uint8_t>> decompress(const std::vector<uint8_t>& data) override {
        // TODO: Implement RLE decompression
        return core::Result<std::vector<uint8_t>>(data);
    }

    core::Result<size_t> compressChunk(
        const uint8_t* data,
        size_t size,
        uint8_t* out,
        size_t out_size) override {
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

    core::Result<size_t> decompressChunk(
        const uint8_t* data,
        size_t size,
        uint8_t* out,
        size_t out_size) override {
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

    bool is_compressed() const override {
        return true;
    }
};

// XORCompressor implementation
class XORCompressorImpl : public Compressor {
public:
    core::Result<std::vector<uint8_t>> compress(const std::vector<uint8_t>& data) override {
        // TODO: Implement XOR compression
        return core::Result<std::vector<uint8_t>>(data);
    }

    core::Result<std::vector<uint8_t>> decompress(const std::vector<uint8_t>& data) override {
        // TODO: Implement XOR decompression
        return core::Result<std::vector<uint8_t>>(data);
    }

    core::Result<size_t> compressChunk(
        const uint8_t* data,
        size_t size,
        uint8_t* out,
        size_t out_size) override {
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

    core::Result<size_t> decompressChunk(
        const uint8_t* data,
        size_t size,
        uint8_t* out,
        size_t out_size) override {
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

    bool is_compressed() const override {
        return true;
    }
};

// Factory functions
std::unique_ptr<TimestampCompressor> create_timestamp_compressor() {
    return std::make_unique<SimpleTimestampCompressorImpl>();
}

std::unique_ptr<ValueCompressor> create_value_compressor() {
    return std::make_unique<SimpleValueCompressorImpl>();
}

std::unique_ptr<LabelCompressor> create_label_compressor() {
    return std::make_unique<SimpleLabelCompressorImpl>();
}

std::unique_ptr<Compressor> create_gorilla_compressor() {
    return std::make_unique<GorillaCompressorImpl>();
}

std::unique_ptr<Compressor> create_rle_compressor() {
    return std::make_unique<RLECompressorImpl>();
}

std::unique_ptr<Compressor> create_xor_compressor() {
    return std::make_unique<XORCompressorImpl>();
}

} // namespace internal
} // namespace storage
} // namespace tsdb 