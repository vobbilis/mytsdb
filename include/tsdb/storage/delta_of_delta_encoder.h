#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include "tsdb/storage/compression.h"

namespace tsdb {
namespace storage {
namespace internal {

/**
 * @brief Configuration for delta-of-delta encoding
 */
struct DeltaOfDeltaConfig {
    uint32_t min_block_size = 64;      // Minimum block size for compression
    uint32_t max_block_size = 1024;    // Maximum block size for compression
    bool enable_irregular_handling = true;  // Handle irregular intervals
    bool enable_zigzag_encoding = true;     // Use zigzag encoding for signed values
    uint32_t compression_level = 6;    // Compression level (0-9)
};

/**
 * @brief Delta-of-delta encoder for timestamp compression
 * 
 * This encoder implements advanced timestamp compression using delta-of-delta encoding
 * with variable-length encoding for optimal compression ratios. It handles both
 * regular and irregular timestamp intervals gracefully.
 */
class DeltaOfDeltaEncoder {
public:
    explicit DeltaOfDeltaEncoder(const DeltaOfDeltaConfig& config = DeltaOfDeltaConfig{});
    
    /**
     * @brief Compress timestamps using delta-of-delta encoding
     * @param timestamps Vector of timestamps to compress
     * @return Compressed data
     */
    std::vector<uint8_t> compress(const std::vector<int64_t>& timestamps);
    
    /**
     * @brief Decompress timestamps
     * @param data Compressed data
     * @return Decompressed timestamps
     */
    std::vector<int64_t> decompress(const std::vector<uint8_t>& data);
    
    /**
     * @brief Compress timestamps with explicit block size
     * @param timestamps Vector of timestamps to compress
     * @param block_size Size of each compression block
     * @return Compressed data
     */
    std::vector<uint8_t> compressWithBlockSize(const std::vector<int64_t>& timestamps, uint32_t block_size);
    
    /**
     * @brief Get compression statistics
     * @return Compression statistics
     */
    struct CompressionStats {
        size_t original_size = 0;
        size_t compressed_size = 0;
        double compression_ratio = 1.0;
        size_t blocks_processed = 0;
        size_t irregular_intervals = 0;
        double average_delta = 0.0;
        double average_delta_of_delta = 0.0;
    };
    
    CompressionStats getStats() const { return stats_; }
    
    /**
     * @brief Reset compression statistics
     */
    void resetStats() { stats_ = CompressionStats{}; }
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void updateConfig(const DeltaOfDeltaConfig& config) { config_ = config; }
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const DeltaOfDeltaConfig& getConfig() const { return config_; }

private:
    DeltaOfDeltaConfig config_;
    mutable CompressionStats stats_;
    
    /**
     * @brief Encode a single delta-of-delta value using variable-length encoding
     * @param dod Delta-of-delta value to encode
     * @param result Output vector to append encoded data
     */
    void encodeDeltaOfDelta(int64_t dod, std::vector<uint8_t>& result);
    
    /**
     * @brief Decode a single delta-of-delta value
     * @param data Input data
     * @param pos Current position (updated after decoding)
     * @return Decoded delta-of-delta value
     */
    int64_t decodeDeltaOfDelta(const std::vector<uint8_t>& data, size_t& pos);
    
    /**
     * @brief Apply zigzag encoding to signed value
     * @param value Signed value
     * @return Zigzag encoded value
     */
    uint64_t zigzagEncode(int64_t value) const;
    
    /**
     * @brief Apply zigzag decoding to unsigned value
     * @param value Zigzag encoded value
     * @return Decoded signed value
     */
    int64_t zigzagDecode(uint64_t value) const;
    
    /**
     * @brief Write variable-length integer
     * @param value Value to write
     * @param result Output vector
     */
    void writeVarInt(uint64_t value, std::vector<uint8_t>& result);
    
    /**
     * @brief Read variable-length integer
     * @param data Input data
     * @param pos Current position (updated after reading)
     * @return Read value
     */
    uint64_t readVarInt(const std::vector<uint8_t>& data, size_t& pos);
    
    /**
     * @brief Compress a single block of timestamps
     * @param timestamps Timestamps in the block
     * @param start_idx Starting index
     * @param end_idx Ending index (exclusive)
     * @param result Output vector
     */
    void compressBlock(const std::vector<int64_t>& timestamps, 
                      size_t start_idx, size_t end_idx, 
                      std::vector<uint8_t>& result);
    
    /**
     * @brief Decompress a single block of timestamps
     * @param data Compressed data
     * @param pos Current position (updated after decompression)
     * @param count Number of timestamps in block
     * @param result Output vector
     */
    void decompressBlock(const std::vector<uint8_t>& data, 
                        size_t& pos, uint32_t count,
                        std::vector<int64_t>& result);
    
    /**
     * @brief Detect if timestamps have regular intervals
     * @param timestamps Timestamps to analyze
     * @param start_idx Starting index
     * @param end_idx Ending index (exclusive)
     * @return True if intervals are regular
     */
    bool detectRegularIntervals(const std::vector<int64_t>& timestamps,
                               size_t start_idx, size_t end_idx) const;
    
    /**
     * @brief Calculate optimal block size for given timestamps
     * @param timestamps Timestamps to analyze
     * @return Optimal block size
     */
    uint32_t calculateOptimalBlockSize(const std::vector<int64_t>& timestamps) const;
};

/**
 * @brief Factory for creating delta-of-delta encoders
 */
class DeltaOfDeltaEncoderFactory {
public:
    /**
     * @brief Create a delta-of-delta encoder with default configuration
     * @return Unique pointer to encoder
     */
    static std::unique_ptr<DeltaOfDeltaEncoder> create();
    
    /**
     * @brief Create a delta-of-delta encoder with custom configuration
     * @param config Configuration
     * @return Unique pointer to encoder
     */
    static std::unique_ptr<DeltaOfDeltaEncoder> create(const DeltaOfDeltaConfig& config);
    
    /**
     * @brief Create a delta-of-delta encoder optimized for specific use case
     * @param use_case Use case ("high_frequency", "low_frequency", "irregular")
     * @return Unique pointer to encoder
     */
    static std::unique_ptr<DeltaOfDeltaEncoder> createForUseCase(const std::string& use_case);
};

} // namespace internal
} // namespace storage
} // namespace tsdb 