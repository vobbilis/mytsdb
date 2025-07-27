#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {
namespace internal {

/**
 * @brief Compression algorithm configuration
 */
struct CompressionConfig {
    enum class Algorithm : uint8_t {
        NONE,           // No compression
        GORILLA,        // Facebook's Gorilla compression
        DELTA_XOR,      // XOR-based delta compression
        DICTIONARY,     // Dictionary-based compression
        RLE            // Run-length encoding
    };
    
    Algorithm algorithm;
    uint32_t level;           // Compression level (0-9)
    size_t dictionary_size;   // Size for dictionary-based compression
    bool enable_simd;         // Use SIMD acceleration if available
};

/**
 * @brief Interface for data compression
 */
class Compressor {
public:
    virtual ~Compressor() = default;

    /**
     * @brief Compress data
     * @param data Data to compress
     * @return Compressed data or error
     */
    virtual core::Result<std::vector<uint8_t>> compress(
        const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Decompress data
     * @param data Compressed data
     * @return Decompressed data or error
     */
    virtual core::Result<std::vector<uint8_t>> decompress(
        const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Compress a chunk of data
     * @param data Data to compress
     * @param size Size of data
     * @return Size of compressed data or error
     */
    virtual core::Result<size_t> compressChunk(
        const uint8_t* data,
        size_t size,
        uint8_t* out,
        size_t out_size) = 0;

    /**
     * @brief Decompress a chunk of data
     * @param data Compressed data
     * @param size Size of compressed data
     * @return Size of decompressed data or error
     */
    virtual core::Result<size_t> decompressChunk(
        const uint8_t* data,
        size_t size,
        uint8_t* out,
        size_t out_size) = 0;

    /**
     * @brief Check if compression is enabled
     */
    virtual bool is_compressed() const = 0;
};

/**
 * @brief Interface for timestamp compression
 */
class TimestampCompressor {
public:
    virtual ~TimestampCompressor() = default;
    
    /**
     * @brief Compress timestamps using XOR delta-of-delta encoding
     */
    virtual std::vector<uint8_t> compress(const std::vector<int64_t>& timestamps) = 0;
    
    /**
     * @brief Decompress timestamps
     */
    virtual std::vector<int64_t> decompress(const std::vector<uint8_t>& data) = 0;
    
    /**
     * @brief Check if compression is enabled
     */
    virtual bool is_compressed() const = 0;
};

/**
 * @brief Interface for value compression
 */
class ValueCompressor {
public:
    virtual ~ValueCompressor() = default;
    
    /**
     * @brief Compress values using XOR-based compression
     */
    virtual std::vector<uint8_t> compress(const std::vector<double>& values) = 0;
    
    /**
     * @brief Decompress values
     */
    virtual std::vector<double> decompress(const std::vector<uint8_t>& data) = 0;
    
    /**
     * @brief Check if compression is enabled
     */
    virtual bool is_compressed() const = 0;
};

/**
 * @brief Interface for label compression
 */
class LabelCompressor {
public:
    virtual ~LabelCompressor() = default;
    
    /**
     * @brief Add a label to the dictionary
     * @return Label ID
     */
    virtual uint32_t add_label(const std::string& label) = 0;
    
    /**
     * @brief Get a label from the dictionary
     * @return Label string
     */
    virtual std::string get_label(uint32_t id) const = 0;
    
    /**
     * @brief Compress labels using dictionary encoding
     */
    virtual std::vector<uint8_t> compress(const core::Labels& labels) = 0;
    
    /**
     * @brief Decompress labels
     */
    virtual core::Labels decompress(const std::vector<uint8_t>& data) = 0;
    
    /**
     * @brief Get dictionary size
     */
    virtual size_t dictionary_size() const = 0;
    
    /**
     * @brief Clear dictionary
     */
    virtual void clear() = 0;
};

/**
 * @brief Factory for creating compressors
 */
class CompressorFactory {
public:
    virtual ~CompressorFactory() = default;
    
    /**
     * @brief Create a timestamp compressor
     */
    virtual std::unique_ptr<TimestampCompressor> create_timestamp_compressor() = 0;
    
    /**
     * @brief Create a value compressor
     */
    virtual std::unique_ptr<ValueCompressor> create_value_compressor() = 0;
    
    /**
     * @brief Create a label compressor
     */
    virtual std::unique_ptr<LabelCompressor> create_label_compressor() = 0;
    
    /**
     * @brief Create a general compressor
     */
    virtual std::unique_ptr<Compressor> create_compressor(CompressionConfig::Algorithm algo) = 0;
};

// Forward declarations
class SimpleTimestampCompressor : public TimestampCompressor {
public:
    std::vector<uint8_t> compress(const std::vector<int64_t>& timestamps) override;
    std::vector<int64_t> decompress(const std::vector<uint8_t>& data) override;
    bool is_compressed() const override;
};

class SimpleValueCompressor : public ValueCompressor {
public:
    std::vector<uint8_t> compress(const std::vector<double>& values) override;
    std::vector<double> decompress(const std::vector<uint8_t>& data) override;
    bool is_compressed() const override;
};

class SimpleLabelCompressor : public LabelCompressor {
public:
    uint32_t add_label(const std::string& label) override;
    std::string get_label(uint32_t id) const override;
    std::vector<uint8_t> compress(const core::Labels& labels) override;
    core::Labels decompress(const std::vector<uint8_t>& data) override;
    size_t dictionary_size() const override;
    void clear() override;
private:
    std::unordered_map<std::string, uint32_t> label_to_id_;
    std::vector<std::string> id_to_label_;
};

class GorillaCompressor : public Compressor {
public:
    core::Result<std::vector<uint8_t>> compress(const std::vector<uint8_t>& data) override;
    core::Result<std::vector<uint8_t>> decompress(const std::vector<uint8_t>& data) override;
    core::Result<size_t> compressChunk(const uint8_t* data, size_t size, uint8_t* out, size_t out_size) override;
    core::Result<size_t> decompressChunk(const uint8_t* data, size_t size, uint8_t* out, size_t out_size) override;
    bool is_compressed() const override;
};

class RLECompressor : public Compressor {
public:
    core::Result<std::vector<uint8_t>> compress(const std::vector<uint8_t>& data) override;
    core::Result<std::vector<uint8_t>> decompress(const std::vector<uint8_t>& data) override;
    core::Result<size_t> compressChunk(const uint8_t* data, size_t size, uint8_t* out, size_t out_size) override;
    core::Result<size_t> decompressChunk(const uint8_t* data, size_t size, uint8_t* out, size_t out_size) override;
    bool is_compressed() const override;
};

class XORCompressor : public Compressor {
public:
    core::Result<std::vector<uint8_t>> compress(const std::vector<uint8_t>& data) override;
    core::Result<std::vector<uint8_t>> decompress(const std::vector<uint8_t>& data) override;
    core::Result<size_t> compressChunk(const uint8_t* data, size_t size, uint8_t* out, size_t out_size) override;
    core::Result<size_t> decompressChunk(const uint8_t* data, size_t size, uint8_t* out, size_t out_size) override;
    bool is_compressed() const override;
};
// Factory functions
std::unique_ptr<TimestampCompressor> create_timestamp_compressor();
std::unique_ptr<ValueCompressor> create_value_compressor();
std::unique_ptr<LabelCompressor> create_label_compressor();
std::unique_ptr<Compressor> create_gorilla_compressor();
std::unique_ptr<Compressor> create_rle_compressor();
std::unique_ptr<Compressor> create_xor_compressor();
std::unique_ptr<CompressorFactory> create_compressor_factory();

} // namespace internal
} // namespace storage
} // namespace tsdb 