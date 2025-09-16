#include "tsdb/storage/adaptive_compressor.h"
#include "tsdb/storage/compression.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace tsdb {
namespace storage {
namespace internal {

AdaptiveCompressor::AdaptiveCompressor(const AdaptiveCompressionConfig& config)
    : config_(config) {
    // Initialize type-specific compressors
    counter_compressor_ = std::make_unique<SimpleValueCompressor>();
    gauge_compressor_ = std::make_unique<SimpleValueCompressor>();
    histogram_compressor_ = std::make_unique<SimpleValueCompressor>();
    constant_compressor_ = std::make_unique<SimpleValueCompressor>();
}

std::vector<uint8_t> AdaptiveCompressor::compress(const std::vector<double>& values) {
    if (values.empty()) {
        return std::vector<uint8_t>();
    }
    
    DataType detected_type = detectDataType(values);
    return compressWithType(values, detected_type);
}

std::vector<double> AdaptiveCompressor::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<double>();
    }
    
    if (data.size() < sizeof(uint32_t) + sizeof(uint8_t)) {
        return std::vector<double>();
    }
    
    // Read header: count and data type
    uint32_t count = 0;
    std::memcpy(&count, data.data(), sizeof(count));
    
    uint8_t data_type_byte = data[sizeof(count)];
    DataType data_type = static_cast<DataType>(data_type_byte);
    
    // Extract the actual compressed data (skip header)
    std::vector<uint8_t> compressed_data(data.begin() + sizeof(count) + sizeof(uint8_t), data.end());
    
    // Decompress based on type
    switch (data_type) {
        case DataType::COUNTER:
            return decompressCounter(compressed_data, count);
        case DataType::GAUGE:
            return decompressGauge(compressed_data, count);
        case DataType::HISTOGRAM:
            return decompressHistogram(compressed_data, count);
        case DataType::CONSTANT:
            return decompressConstant(compressed_data, count);
        default:
            // Fallback to gauge compression
            return gauge_compressor_->decompress(compressed_data);
    }
}

bool AdaptiveCompressor::is_compressed() const {
    return true;
}

DataType AdaptiveCompressor::detectDataType(const std::vector<double>& values) const {
    if (values.size() < config_.min_samples_for_detection) {
        return DataType::GAUGE; // Default to gauge for small datasets
    }
    
    // Check for constant values first (most restrictive)
    if (isMostlyConstant(values)) {
        return DataType::CONSTANT;
    }
    
    // Check for counter (monotonic increasing)
    if (isMonotonicIncreasing(values)) {
        return DataType::COUNTER;
    }
    
    // Check for histogram data
    if (isHistogramData(values)) {
        return DataType::HISTOGRAM;
    }
    
    // Default to gauge
    return DataType::GAUGE;
}

std::vector<uint8_t> AdaptiveCompressor::compressWithType(const std::vector<double>& values, DataType type) {
    if (values.empty()) {
        return std::vector<uint8_t>();
    }
    
    std::vector<uint8_t> compressed_data;
    size_t original_size = values.size() * sizeof(double);
    
    // Compress based on type
    switch (type) {
        case DataType::COUNTER:
            compressed_data = compressCounter(values);
            break;
        case DataType::GAUGE:
            compressed_data = compressGauge(values);
            break;
        case DataType::HISTOGRAM:
            compressed_data = compressHistogram(values);
            break;
        case DataType::CONSTANT:
            compressed_data = compressConstant(values);
            break;
        default:
            compressed_data = compressGauge(values);
            break;
    }
    
    // Create final result with header
    std::vector<uint8_t> result;
    result.reserve(sizeof(uint32_t) + sizeof(uint8_t) + compressed_data.size());
    
    // Write header: count and data type
    uint32_t count = static_cast<uint32_t>(values.size());
    result.insert(result.end(), 
                 reinterpret_cast<uint8_t*>(&count),
                 reinterpret_cast<uint8_t*>(&count) + sizeof(count));
    
    uint8_t type_byte = static_cast<uint8_t>(type);
    result.push_back(type_byte);
    
    // Append compressed data
    result.insert(result.end(), compressed_data.begin(), compressed_data.end());
    
    // Update metrics
    updateMetrics(type, original_size, result.size());
    
    return result;
}

std::vector<uint8_t> AdaptiveCompressor::compressCounter(const std::vector<double>& values) {
    // For counters, we can use delta compression since values are monotonic
    std::vector<uint8_t> result;
    result.reserve(values.size() * sizeof(double));
    
    // Store first value as-is
    result.insert(result.end(), 
                 reinterpret_cast<const uint8_t*>(&values[0]),
                 reinterpret_cast<const uint8_t*>(&values[0]) + sizeof(double));
    
    // Delta compression for remaining values
    for (size_t i = 1; i < values.size(); i++) {
        double delta = values[i] - values[i-1];
        
        // Variable-length encoding for deltas
        if (delta == 0.0) {
            result.push_back(0x00); // 1 byte for zero delta
        } else if (std::abs(delta) <= 127.0) {
            result.push_back(0x01); // 1 byte flag
            result.push_back(static_cast<uint8_t>(static_cast<int8_t>(delta)));
        } else if (std::abs(delta) <= 32767.0) {
            result.push_back(0x02); // 2 byte flag
            int16_t encoded = static_cast<int16_t>(delta);
            result.push_back(static_cast<uint8_t>(encoded & 0xFF));
            result.push_back(static_cast<uint8_t>((encoded >> 8) & 0xFF));
        } else {
            result.push_back(0x03); // 8 byte flag
            result.insert(result.end(),
                        reinterpret_cast<const uint8_t*>(&delta),
                        reinterpret_cast<const uint8_t*>(&delta) + sizeof(delta));
        }
    }
    
    return result;
}

std::vector<double> AdaptiveCompressor::decompressCounter(const std::vector<uint8_t>& data, uint32_t count) {
    if (data.empty() || count == 0) {
        return std::vector<double>();
    }
    
    std::vector<double> result;
    result.reserve(count);
    
    size_t pos = 0;
    
    // Read first value
    if (pos + sizeof(double) <= data.size()) {
        double value = 0.0;
        std::memcpy(&value, &data[pos], sizeof(double));
        result.push_back(value);
        pos += sizeof(double);
        
        // Decompress remaining values
        while (pos < data.size() && result.size() < count) {
            if (pos >= data.size()) break;
            
            uint8_t flag = data[pos++];
            
            double delta = 0.0;
            switch (flag) {
                case 0x00: // Zero delta
                    delta = 0.0;
                    break;
                case 0x01: // 1 byte delta
                    if (pos < data.size()) {
                        delta = static_cast<double>(static_cast<int8_t>(data[pos++]));
                    }
                    break;
                case 0x02: // 2 byte delta
                    if (pos + 1 < data.size()) {
                        int16_t encoded = static_cast<int16_t>(data[pos]) | 
                                        (static_cast<int16_t>(data[pos + 1]) << 8);
                        delta = static_cast<double>(encoded);
                        pos += 2;
                    }
                    break;
                case 0x03: // 8 byte delta
                    if (pos + sizeof(double) <= data.size()) {
                        std::memcpy(&delta, &data[pos], sizeof(double));
                        pos += sizeof(double);
                    }
                    break;
                default:
                    // Invalid flag, skip
                    break;
            }
            
            result.push_back(result.back() + delta);
        }
    }
    
    return result;
}

std::vector<uint8_t> AdaptiveCompressor::compressGauge(const std::vector<double>& values) {
    // For gauge data, use delta encoding with variable-length encoding
    std::vector<uint8_t> result;
    
    if (values.empty()) {
        return result;
    }
    
    // Write first value as-is
    double first_value = values[0];
    result.insert(result.end(), 
                 reinterpret_cast<const uint8_t*>(&first_value),
                 reinterpret_cast<const uint8_t*>(&first_value) + sizeof(double));
    
    // Encode deltas for remaining values with better compression
    for (size_t i = 1; i < values.size(); i++) {
        double delta = values[i] - values[i-1];
        
        // Use more aggressive variable-length encoding for deltas
        if (std::abs(delta) < 1e-5) {
            // Very small delta: encode as 0 (1 byte)
            result.push_back(0);
        } else if (std::abs(delta) < 1e-2) {
            // Small delta: encode as 1 + 1-byte scaled
            result.push_back(1);
            // Scale to 1-byte precision (-128 to 127)
            int8_t scaled = static_cast<int8_t>(delta * 100);
            result.push_back(static_cast<uint8_t>(scaled));
        } else if (std::abs(delta) < 1e0) {
            // Medium delta: encode as 2 + 2-byte half-precision
            result.push_back(2);
            // Convert to half-precision (2 bytes)
            uint16_t half_precision = static_cast<uint16_t>(delta * 1000); // Scale for 2-byte precision
            result.push_back(static_cast<uint8_t>(half_precision & 0xFF));
            result.push_back(static_cast<uint8_t>((half_precision >> 8) & 0xFF));
        } else if (std::abs(delta) < 1e2) {
            // Large delta: encode as 3 + 4-byte float
            result.push_back(3);
            float delta_float = static_cast<float>(delta);
            result.insert(result.end(),
                        reinterpret_cast<const uint8_t*>(&delta_float),
                        reinterpret_cast<const uint8_t*>(&delta_float) + sizeof(float));
        } else {
            // Very large delta: encode as 4 + 8-byte double
            result.push_back(4);
            result.insert(result.end(),
                        reinterpret_cast<const uint8_t*>(&delta),
                        reinterpret_cast<const uint8_t*>(&delta) + sizeof(double));
        }
    }
    
    return result;
}

std::vector<double> AdaptiveCompressor::decompressGauge(const std::vector<uint8_t>& data, uint32_t count) {
    if (data.empty() || count == 0) {
        return std::vector<double>();
    }
    
    std::vector<double> result;
    result.reserve(count);
    
    size_t pos = 0;
    
    // Read first value
    if (pos + sizeof(double) <= data.size()) {
        double value = 0.0;
        std::memcpy(&value, &data[pos], sizeof(double));
        result.push_back(value);
        pos += sizeof(double);
        
        // Decompress remaining values
        while (pos < data.size() && result.size() < count) {
            if (pos >= data.size()) break;
            
            uint8_t flag = data[pos++];
            
            double delta = 0.0;
            switch (flag) {
                case 0: // Very small delta
                    delta = 0.0;
                    break;
                case 1: // Small delta (1-byte scaled)
                    if (pos < data.size()) {
                        int8_t scaled = static_cast<int8_t>(data[pos++]);
                        delta = static_cast<double>(scaled) / 100.0; // Less aggressive scaling
                    }
                    break;
                case 2: // Medium delta (2-byte half-precision)
                    if (pos + 1 < data.size()) {
                        uint16_t half_precision = static_cast<uint16_t>(data[pos]) | 
                                                (static_cast<uint16_t>(data[pos + 1]) << 8);
                        delta = static_cast<double>(half_precision) / 1000.0; // Less aggressive scaling
                        pos += 2;
                    }
                    break;
                case 3: // Large delta (4-byte float)
                    if (pos + sizeof(float) <= data.size()) {
                        float delta_float = 0.0f;
                        std::memcpy(&delta_float, &data[pos], sizeof(float));
                        delta = static_cast<double>(delta_float);
                        pos += sizeof(float);
                    }
                    break;
                case 4: // Very large delta (8-byte double)
                    if (pos + sizeof(double) <= data.size()) {
                        std::memcpy(&delta, &data[pos], sizeof(double));
                        pos += sizeof(double);
                    }
                    break;
                default:
                    // Unknown flag, skip
                    break;
            }
            
            // Add delta to previous value
            double new_value = result.back() + delta;
            result.push_back(new_value);
        }
    }
    
    return result;
}

std::vector<uint8_t> AdaptiveCompressor::compressHistogram(const std::vector<double>& values) {
    // For histogram data, we can use specialized compression
    // This is a simplified version - in practice, histogram compression
    // would be more sophisticated based on the distribution characteristics
    
    std::vector<uint8_t> result;
    result.reserve(values.size() * 2 + sizeof(double) * 2);
    
    // Calculate statistics for histogram compression
    double min_val = *std::min_element(values.begin(), values.end());
    double max_val = *std::max_element(values.begin(), values.end());
    double range = max_val - min_val;
    
    // Store range information
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&min_val),
                 reinterpret_cast<const uint8_t*>(&min_val) + sizeof(min_val));
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&range),
                 reinterpret_cast<const uint8_t*>(&range) + sizeof(range));
    
    // Normalize values to [0,1] range and compress
    for (const double& value : values) {
        double normalized = range > 0 ? (value - min_val) / range : 0.0;
        
        // Quantize to 16-bit precision for histogram data
        uint16_t quantized = static_cast<uint16_t>(normalized * 65535.0);
        result.push_back(static_cast<uint8_t>(quantized & 0xFF));
        result.push_back(static_cast<uint8_t>((quantized >> 8) & 0xFF));
    }
    
    return result;
}

std::vector<double> AdaptiveCompressor::decompressHistogram(const std::vector<uint8_t>& data, uint32_t count) {
    if (data.empty() || count == 0) {
        return std::vector<double>();
    }
    
    std::vector<double> result;
    result.reserve(count);
    
    size_t pos = 0;
    
    // Read range information
    if (pos + sizeof(double) * 2 <= data.size()) {
        double min_val = 0.0;
        double range = 0.0;
        
        std::memcpy(&min_val, &data[pos], sizeof(double));
        pos += sizeof(double);
        std::memcpy(&range, &data[pos], sizeof(double));
        pos += sizeof(double);
        
        // Decompress quantized values
        for (uint32_t i = 0; i < count && pos + 1 < data.size(); i++) {
            uint16_t quantized = static_cast<uint16_t>(data[pos]) | 
                               (static_cast<uint16_t>(data[pos + 1]) << 8);
            pos += 2;
            
            // Dequantize
            double normalized = static_cast<double>(quantized) / 65535.0;
            double value = min_val + normalized * range;
            result.push_back(value);
        }
    }
    
    return result;
}

std::vector<uint8_t> AdaptiveCompressor::compressConstant(const std::vector<double>& values) {
    // For constant data, we only need to store the value once
    std::vector<uint8_t> result;
    result.reserve(sizeof(double));
    
    // Store the constant value once
    result.insert(result.end(), 
                 reinterpret_cast<const uint8_t*>(&values[0]),
                 reinterpret_cast<const uint8_t*>(&values[0]) + sizeof(double));
    
    return result;
}

std::vector<double> AdaptiveCompressor::decompressConstant(const std::vector<uint8_t>& data, uint32_t count) {
    if (data.empty() || count == 0) {
        return std::vector<double>();
    }
    
    std::vector<double> result;
    result.reserve(count);
    
    // Read the constant value
    if (data.size() >= sizeof(double)) {
        double constant_value = 0.0;
        std::memcpy(&constant_value, data.data(), sizeof(double));
        
        // Replicate the constant value
        result.resize(count, constant_value);
    }
    
    return result;
}

bool AdaptiveCompressor::isMonotonicIncreasing(const std::vector<double>& values) const {
    if (values.size() < 2) {
        return false;
    }
    
    size_t increasing_count = 0;
    for (size_t i = 1; i < values.size(); i++) {
        if (values[i] >= values[i-1]) {
            increasing_count++;
        }
    }
    
    double ratio = static_cast<double>(increasing_count) / (values.size() - 1);
    return ratio >= config_.counter_threshold;
}

bool AdaptiveCompressor::isMostlyConstant(const std::vector<double>& values) const {
    if (values.size() < 2) {
        return true;
    }
    
    double first_value = values[0];
    size_t constant_count = 0;
    
    for (const double& value : values) {
        if (std::abs(value - first_value) < 1e-10) {
            constant_count++;
        }
    }
    
    double ratio = static_cast<double>(constant_count) / values.size();
    return ratio >= config_.constant_threshold;
}

bool AdaptiveCompressor::isHistogramData(const std::vector<double>& values) const {
    // More sophisticated histogram detection
    if (values.size() < 5) {
        return false;
    }
    
    // Check if all values are non-negative (typical for histogram data)
    bool all_non_negative = std::all_of(values.begin(), values.end(),
                                       [](double v) { return v >= 0.0; });
    
    if (!all_non_negative) {
        return false;
    }
    
    // Check for histogram-like characteristics:
    // 1. Values should be non-negative
    // 2. Should have some variation but not too much
    // 3. Should not be monotonic (histograms are not counters)
    // 4. Should have a more skewed distribution typical of histograms
    
    // Calculate basic statistics
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();
    
    if (mean <= 0) {
        return false;
    }
    
    double variance = 0.0;
    for (const double& value : values) {
        double diff = value - mean;
        variance += diff * diff;
    }
    variance /= values.size();
    double std_dev = std::sqrt(variance);
    
    // Coefficient of variation should be reasonable for histogram data
    double cv = std_dev / mean;
    
    // Histogram data typically has CV between 0.5 and 1.5
    // This is more restrictive to avoid overlap with gauge data
    bool reasonable_cv = cv >= 0.5 && cv <= 1.5;
    
    // Check that it's not monotonic (histograms are not counters)
    bool not_monotonic = !isMonotonicIncreasing(values);
    
    // Check for skewness (histogram data is often right-skewed)
    // Calculate skewness: E[(X-μ)³]/σ³
    double skewness = 0.0;
    for (const double& value : values) {
        double diff = value - mean;
        skewness += (diff * diff * diff);
    }
    skewness /= (values.size() * std_dev * std_dev * std_dev);
    
    // Histogram data is often right-skewed (positive skewness)
    bool has_skewness = skewness > 0.2;
    
    return all_non_negative && reasonable_cv && not_monotonic && has_skewness;
}

double AdaptiveCompressor::calculateCompressionRatio(size_t original_size, size_t compressed_size) const {
    if (original_size == 0) return 0.0;
    return static_cast<double>(compressed_size) / original_size;
}

void AdaptiveCompressor::updateMetrics(DataType type, size_t original_size, size_t compressed_size) {
    size_t bytes_saved = original_size - compressed_size;
    
    switch (type) {
        case DataType::COUNTER:
            metrics_.counter_compressions.fetch_add(1, std::memory_order_relaxed);
            metrics_.counter_bytes_saved.fetch_add(bytes_saved, std::memory_order_relaxed);
            break;
        case DataType::GAUGE:
            metrics_.gauge_compressions.fetch_add(1, std::memory_order_relaxed);
            metrics_.gauge_bytes_saved.fetch_add(bytes_saved, std::memory_order_relaxed);
            break;
        case DataType::HISTOGRAM:
            metrics_.histogram_compressions.fetch_add(1, std::memory_order_relaxed);
            metrics_.histogram_bytes_saved.fetch_add(bytes_saved, std::memory_order_relaxed);
            break;
        case DataType::CONSTANT:
            metrics_.constant_compressions.fetch_add(1, std::memory_order_relaxed);
            metrics_.constant_bytes_saved.fetch_add(bytes_saved, std::memory_order_relaxed);
            break;
        default:
            break;
    }
    
    metrics_.total_original_bytes.fetch_add(original_size, std::memory_order_relaxed);
    metrics_.total_compressed_bytes.fetch_add(compressed_size, std::memory_order_relaxed);
}

std::unique_ptr<AdaptiveCompressor> AdaptiveCompressorFactory::create(const AdaptiveCompressionConfig& config) {
    return std::make_unique<AdaptiveCompressor>(config);
}

} // namespace internal
} // namespace storage
} // namespace tsdb 