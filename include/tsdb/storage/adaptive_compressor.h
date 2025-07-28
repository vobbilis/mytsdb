#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include "tsdb/storage/compression.h"

namespace tsdb {
namespace storage {
namespace internal {

/**
 * @brief Data types for adaptive compression
 */
enum class DataType : uint8_t {
    COUNTER,    // Monotonic increasing values
    GAUGE,      // Variable values with no specific pattern
    HISTOGRAM,  // Distribution data (percentiles, buckets)
    CONSTANT,   // Single value repeated
    UNKNOWN     // Type not yet determined
};

/**
 * @brief Configuration for adaptive compression
 */
struct AdaptiveCompressionConfig {
    size_t min_samples_for_detection = 10;  // Minimum samples to detect type
    double counter_threshold = 0.95;        // Ratio of increasing values for counter
    double constant_threshold = 0.99;       // Ratio of same values for constant
    bool enable_ml_detection = false;       // Enable ML-based detection (future)
    size_t max_compression_level = 9;       // Maximum compression level
};

/**
 * @brief Metrics for compression performance
 */
struct CompressionMetrics {
    std::atomic<uint64_t> counter_compressions{0};
    std::atomic<uint64_t> gauge_compressions{0};
    std::atomic<uint64_t> histogram_compressions{0};
    std::atomic<uint64_t> constant_compressions{0};
    
    std::atomic<uint64_t> counter_bytes_saved{0};
    std::atomic<uint64_t> gauge_bytes_saved{0};
    std::atomic<uint64_t> histogram_bytes_saved{0};
    std::atomic<uint64_t> constant_bytes_saved{0};
    
    std::atomic<uint64_t> total_original_bytes{0};
    std::atomic<uint64_t> total_compressed_bytes{0};
    
    double getCompressionRatio() const {
        uint64_t total_orig = total_original_bytes.load();
        uint64_t total_comp = total_compressed_bytes.load();
        return total_orig > 0 ? static_cast<double>(total_comp) / total_orig : 1.0;
    }
    
    double getAverageCompressionRatio() const {
        uint64_t total_compressions = counter_compressions.load() + 
                                     gauge_compressions.load() + 
                                     histogram_compressions.load() + 
                                     constant_compressions.load();
        return total_compressions > 0 ? getCompressionRatio() : 1.0;
    }
};

/**
 * @brief Adaptive compressor that selects optimal compression based on data type
 */
class AdaptiveCompressor : public ValueCompressor {
public:
    explicit AdaptiveCompressor(const AdaptiveCompressionConfig& config = AdaptiveCompressionConfig{});
    ~AdaptiveCompressor() override = default;
    
    /**
     * @brief Compress values with automatic type detection
     */
    std::vector<uint8_t> compress(const std::vector<double>& values) override;
    
    /**
     * @brief Decompress values
     */
    std::vector<double> decompress(const std::vector<uint8_t>& data) override;
    
    /**
     * @brief Check if compression is enabled
     */
    bool is_compressed() const override;
    
    /**
     * @brief Detect data type from values
     */
    DataType detectDataType(const std::vector<double>& values) const;
    
    /**
     * @brief Compress with explicit data type
     */
    std::vector<uint8_t> compressWithType(const std::vector<double>& values, DataType type);
    
    /**
     * @brief Get compression metrics
     */
    const CompressionMetrics& getMetrics() const { return metrics_; }
    
    /**
     * @brief Reset metrics
     */
    void resetMetrics() {
        metrics_.counter_compressions.store(0, std::memory_order_relaxed);
        metrics_.gauge_compressions.store(0, std::memory_order_relaxed);
        metrics_.histogram_compressions.store(0, std::memory_order_relaxed);
        metrics_.constant_compressions.store(0, std::memory_order_relaxed);
        metrics_.counter_bytes_saved.store(0, std::memory_order_relaxed);
        metrics_.gauge_bytes_saved.store(0, std::memory_order_relaxed);
        metrics_.histogram_bytes_saved.store(0, std::memory_order_relaxed);
        metrics_.constant_bytes_saved.store(0, std::memory_order_relaxed);
        metrics_.total_original_bytes.store(0, std::memory_order_relaxed);
        metrics_.total_compressed_bytes.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get configuration
     */
    const AdaptiveCompressionConfig& getConfig() const { return config_; }
    
    /**
     * @brief Update configuration
     */
    void updateConfig(const AdaptiveCompressionConfig& config) { config_ = config; }

private:
    AdaptiveCompressionConfig config_;
    mutable CompressionMetrics metrics_;
    
    // Type-specific compressors
    std::unique_ptr<ValueCompressor> counter_compressor_;
    std::unique_ptr<ValueCompressor> gauge_compressor_;
    std::unique_ptr<ValueCompressor> histogram_compressor_;
    std::unique_ptr<ValueCompressor> constant_compressor_;
    
    /**
     * @brief Compress counter data (monotonic increasing)
     */
    std::vector<uint8_t> compressCounter(const std::vector<double>& values);
    
    /**
     * @brief Compress gauge data (variable values)
     */
    std::vector<uint8_t> compressGauge(const std::vector<double>& values);
    
    /**
     * @brief Compress histogram data (distribution)
     */
    std::vector<uint8_t> compressHistogram(const std::vector<double>& values);
    
    /**
     * @brief Compress constant data (single value)
     */
    std::vector<uint8_t> compressConstant(const std::vector<double>& values);
    
    /**
     * @brief Decompress counter data
     */
    std::vector<double> decompressCounter(const std::vector<uint8_t>& data, uint32_t count);
    
    /**
     * @brief Decompress histogram data
     */
    std::vector<double> decompressHistogram(const std::vector<uint8_t>& data, uint32_t count);
    
    /**
     * @brief Decompress gauge data
     */
    std::vector<double> decompressGauge(const std::vector<uint8_t>& data, uint32_t count);
    
    /**
     * @brief Decompress constant data
     */
    std::vector<double> decompressConstant(const std::vector<uint8_t>& data, uint32_t count);
    
    /**
     * @brief Check if values are monotonic increasing
     */
    bool isMonotonicIncreasing(const std::vector<double>& values) const;
    
    /**
     * @brief Check if values are mostly constant
     */
    bool isMostlyConstant(const std::vector<double>& values) const;
    
    /**
     * @brief Check if values look like histogram data
     */
    bool isHistogramData(const std::vector<double>& values) const;
    
    /**
     * @brief Calculate compression ratio
     */
    double calculateCompressionRatio(size_t original_size, size_t compressed_size) const;
    
    /**
     * @brief Update metrics for a compression operation
     */
    void updateMetrics(DataType type, size_t original_size, size_t compressed_size);
};

/**
 * @brief Factory for creating adaptive compressors
 */
class AdaptiveCompressorFactory {
public:
    static std::unique_ptr<AdaptiveCompressor> create(
        const AdaptiveCompressionConfig& config = AdaptiveCompressionConfig{});
};

} // namespace internal
} // namespace storage
} // namespace tsdb 