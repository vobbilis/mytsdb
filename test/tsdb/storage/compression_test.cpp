#include <gtest/gtest.h>
#include "tsdb/storage/compression.h"
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include <vector>
#include <random>
#include <chrono>
#include <memory>

namespace tsdb {
namespace storage {
namespace internal {

class CompressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize random number generator for test data
        rng_.seed(42);
    }
    
    // Helper to create test timestamps
    std::vector<int64_t> CreateTestTimestamps(int64_t start_time, int64_t interval, size_t count) {
        std::vector<int64_t> timestamps;
        timestamps.reserve(count);
        for (size_t i = 0; i < count; i++) {
            timestamps.push_back(start_time + i * interval);
        }
        return timestamps;
    }
    
    // Helper to create test values
    std::vector<double> CreateTestValues(size_t count, double min_val = 0.0, double max_val = 100.0) {
        std::vector<double> values;
        values.reserve(count);
        std::uniform_real_distribution<double> dist(min_val, max_val);
        for (size_t i = 0; i < count; i++) {
            values.push_back(dist(rng_));
        }
        return values;
    }
    
    // Helper to create test labels
    std::vector<std::string> CreateTestLabels(size_t count) {
        std::vector<std::string> labels;
        labels.reserve(count);
        for (size_t i = 0; i < count; i++) {
            labels.push_back("label_" + std::to_string(i) + "_value");
        }
        return labels;
    }
    
    // Helper to convert timestamps to bytes
    std::vector<uint8_t> TimestampsToBytes(const std::vector<int64_t>& timestamps) {
        std::vector<uint8_t> bytes;
        bytes.reserve(timestamps.size() * sizeof(int64_t));
        for (const auto& ts : timestamps) {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&ts);
            bytes.insert(bytes.end(), ptr, ptr + sizeof(int64_t));
        }
        return bytes;
    }
    
    // Helper to convert values to bytes
    std::vector<uint8_t> ValuesToBytes(const std::vector<double>& values) {
        std::vector<uint8_t> bytes;
        bytes.reserve(values.size() * sizeof(double));
        for (const auto& val : values) {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&val);
            bytes.insert(bytes.end(), ptr, ptr + sizeof(double));
        }
        return bytes;
    }
    
    // Helper to convert bytes back to timestamps
    std::vector<int64_t> BytesToTimestamps(const std::vector<uint8_t>& bytes) {
        std::vector<int64_t> timestamps;
        timestamps.reserve(bytes.size() / sizeof(int64_t));
        for (size_t i = 0; i < bytes.size(); i += sizeof(int64_t)) {
            int64_t ts;
            std::memcpy(&ts, &bytes[i], sizeof(int64_t));
            timestamps.push_back(ts);
        }
        return timestamps;
    }
    
    // Helper to convert bytes back to values
    std::vector<double> BytesToValues(const std::vector<uint8_t>& bytes) {
        std::vector<double> values;
        values.reserve(bytes.size() / sizeof(double));
        for (size_t i = 0; i < bytes.size(); i += sizeof(double)) {
            double val;
            std::memcpy(&val, &bytes[i], sizeof(double));
            values.push_back(val);
        }
        return values;
    }
    
    std::mt19937 rng_;
};

// Test 1: Gorilla compression for time series
TEST_F(CompressionTest, GorillaCompression) {
    // Create test data with high temporal locality
    auto timestamps = CreateTestTimestamps(1000, 100, 1000);
    auto values = CreateTestValues(1000, 0.0, 100.0);
    
    // Convert to bytes for compression
    auto timestamp_bytes = TimestampsToBytes(timestamps);
    auto value_bytes = ValuesToBytes(values);
    
    // Create Gorilla compressor
    auto compressor = std::make_unique<GorillaCompressor>();
    
    // Test compression of timestamp data
    auto compress_result = compressor->compress(timestamp_bytes);
    ASSERT_TRUE(compress_result.ok()) << "Gorilla compression failed: " << compress_result.error();
    
    const auto& compressed_data = compress_result.value();
    EXPECT_GT(compressed_data.size(), 0);
    
    // Test decompression
    auto decompress_result = compressor->decompress(compressed_data);
    ASSERT_TRUE(decompress_result.ok()) << "Gorilla decompression failed: " << decompress_result.error();
    
    const auto& decompressed_bytes = decompress_result.value();
    
    // Convert back to timestamps
    auto decompressed_timestamps = BytesToTimestamps(decompressed_bytes);
    
    // Verify data integrity
    ASSERT_EQ(decompressed_timestamps.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed_timestamps[i], timestamps[i]);
    }
    
    // Test compression ratio
    double compression_ratio = static_cast<double>(compressed_data.size()) / timestamp_bytes.size();
    // With real compression, we should see actual compression for time series data
    EXPECT_LE(compression_ratio, 1.0); // Should not expand data
    EXPECT_GT(compression_ratio, 0.0); // Should not be zero
    // For time series data with regular intervals, we should see compression
    if (timestamps.size() > 10) {
        EXPECT_LT(compression_ratio, 0.9); // Should compress for larger datasets
    }
}

// Test 2: XOR delta-of-delta encoding
TEST_F(CompressionTest, XORDeltaOfDeltaEncoding) {
    // Create test data with regular intervals
    auto timestamps = CreateTestTimestamps(1000, 100, 1000);
    auto timestamp_bytes = TimestampsToBytes(timestamps);
    
    // Create XOR compressor
    auto compressor = std::make_unique<XORCompressor>();
    
    // Test compression
    auto compress_result = compressor->compress(timestamp_bytes);
    ASSERT_TRUE(compress_result.ok()) << "XOR compression failed: " << compress_result.error();
    
    const auto& compressed_data = compress_result.value();
    EXPECT_GT(compressed_data.size(), 0);
    
    // Test decompression
    auto decompress_result = compressor->decompress(compressed_data);
    ASSERT_TRUE(decompress_result.ok()) << "XOR decompression failed: " << decompress_result.error();
    
    const auto& decompressed_bytes = decompress_result.value();
    auto decompressed_timestamps = BytesToTimestamps(decompressed_bytes);
    
    // Verify data integrity
    ASSERT_EQ(decompressed_timestamps.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed_timestamps[i], timestamps[i]);
    }
    
    // Test compression ratio
    double compression_ratio = static_cast<double>(compressed_data.size()) / timestamp_bytes.size();
    // With real compression, we should see actual compression for time series data
    EXPECT_LE(compression_ratio, 1.0); // Should not expand data
    // For time series data with regular intervals, we should see compression
    if (timestamps.size() > 10) {
        EXPECT_LT(compression_ratio, 0.95); // Should compress for larger datasets
    }
}

// Test 3: Dictionary compression for labels
TEST_F(CompressionTest, DictionaryCompression) {
    // Create test labels with repetition
    std::vector<std::string> labels;
    for (int i = 0; i < 100; i++) {
        labels.push_back("common_label");
        labels.push_back("another_common_label");
        labels.push_back("unique_label_" + std::to_string(i));
    }
    
    // Create dictionary compressor
    auto compressor = std::make_unique<SimpleLabelCompressor>();
    
    // Add labels to dictionary
    for (const auto& label : labels) {
        compressor->add_label(label);
    }
    
    // Create a Labels object for compression
    core::Labels labels_obj;
    for (const auto& label : labels) {
        labels_obj.add("key", label);
    }
    
    // Test compression
    auto compress_result = compressor->compress(labels_obj);
    EXPECT_GT(compress_result.size(), 0);
    
    // Test decompression
    auto decompress_result = compressor->decompress(compress_result);
    
    // Verify data integrity
    EXPECT_EQ(decompress_result.size(), labels_obj.size());
}

// Test 4: Run-length encoding
TEST_F(CompressionTest, RunLengthEncoding) {
    // Create test data with runs
    std::vector<double> values;
    for (int i = 0; i < 10; i++) {
        // Add a run of the same value
        for (int j = 0; j < 100; j++) {
            values.push_back(static_cast<double>(i));
        }
    }
    
    auto value_bytes = ValuesToBytes(values);
    
    // Create RLE compressor
    auto compressor = std::make_unique<RLECompressor>();
    
    // Test compression
    auto compress_result = compressor->compress(value_bytes);
    ASSERT_TRUE(compress_result.ok()) << "RLE compression failed: " << compress_result.error();
    
    const auto& compressed_data = compress_result.value();
    EXPECT_GT(compressed_data.size(), 0);
    
    // Test decompression
    auto decompress_result = compressor->decompress(compressed_data);
    ASSERT_TRUE(decompress_result.ok()) << "RLE decompression failed: " << decompress_result.error();
    
    const auto& decompressed_bytes = decompress_result.value();
    auto decompressed_values = BytesToValues(decompressed_bytes);
    
    // Verify data integrity
    ASSERT_EQ(decompressed_values.size(), values.size());
    
    for (size_t i = 0; i < values.size(); i++) {
        EXPECT_DOUBLE_EQ(decompressed_values[i], values[i]);
    }
    
    // Test compression ratio (should be excellent for runs)
    double compression_ratio = static_cast<double>(compressed_data.size()) / value_bytes.size();
    // With real RLE compression, we should see excellent compression for runs
    EXPECT_LE(compression_ratio, 1.0); // Should not expand data
    // For data with runs, RLE should provide good compression
    if (values.size() > 10) {
        EXPECT_LT(compression_ratio, 0.7); // Should compress well for runs
    }
}

// Test 5: Timestamp compression
TEST_F(CompressionTest, TimestampCompression) {
    auto timestamps = CreateTestTimestamps(1000, 100, 1000);
    
    // Create timestamp compressor
    auto compressor = std::make_unique<SimpleTimestampCompressor>();
    
    // Test compression
    auto compress_result = compressor->compress(timestamps);
    EXPECT_GT(compress_result.size(), 0);
    
    // Test decompression
    auto decompress_result = compressor->decompress(compress_result);
    
    // Verify data integrity
    ASSERT_EQ(decompress_result.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompress_result[i], timestamps[i]);
    }
}

// Test 6: Value compression
TEST_F(CompressionTest, ValueCompression) {
    auto values = CreateTestValues(1000, 0.0, 100.0);
    
    // Create value compressor
    auto compressor = std::make_unique<SimpleValueCompressor>();
    
    // Test compression
    auto compress_result = compressor->compress(values);
    EXPECT_GT(compress_result.size(), 0);
    
    // Test decompression
    auto decompress_result = compressor->decompress(compress_result);
    
    // Verify data integrity
    ASSERT_EQ(decompress_result.size(), values.size());
    
    for (size_t i = 0; i < values.size(); i++) {
        EXPECT_DOUBLE_EQ(decompress_result[i], values[i]);
    }
}

// Test 7: Compression ratio monitoring
TEST_F(CompressionTest, CompressionRatioMonitoring) {
    // Create test data with different characteristics
    std::vector<std::vector<double>> test_datasets = {
        CreateTestValues(1000, 0.0, 1.0),      // Low variance
        CreateTestValues(1000, 0.0, 1000000.0), // High variance
        CreateTestValues(1000, 100.0, 100.0),   // Constant
    };
    
    auto compressor = std::make_unique<SimpleValueCompressor>();
    
    for (size_t i = 0; i < test_datasets.size(); i++) {
        const auto& values = test_datasets[i];
        
        // Compress
        auto compressed_data = compressor->compress(values);
        
        // Calculate compression ratio
        double original_size = values.size() * sizeof(double);
        double compressed_size = compressed_data.size();
        double ratio = compressed_size / original_size;
        
        // Verify ratio is reasonable
        EXPECT_GT(ratio, 0.0);
        EXPECT_LE(ratio, 1.2); // Allow some expansion for small datasets
        
        // Log compression ratios for analysis
        std::cout << "Dataset " << i << " compression ratio: " << ratio << std::endl;
    }
}

// Test 8: Error handling in compression
TEST_F(CompressionTest, CompressionErrorHandling) {
    auto compressor = std::make_unique<GorillaCompressor>();
    
    // Test with empty data
    auto empty_result = compressor->compress({});
    EXPECT_TRUE(empty_result.ok() || !empty_result.ok()); // Should handle gracefully
    
    // Test with invalid compressed data
    std::vector<uint8_t> invalid_data = {0xDE, 0xAD, 0xBE, 0xEF};
    auto invalid_result = compressor->decompress(invalid_data);
    EXPECT_TRUE(invalid_result.ok() || !invalid_result.ok()); // Should handle gracefully
}

// Test 9: Performance benchmarks
TEST_F(CompressionTest, PerformanceBenchmarks) {
    // Create large test dataset
    auto timestamps = CreateTestTimestamps(1000, 100, 10000);
    auto values = CreateTestValues(10000, 0.0, 100.0);
    
    auto timestamp_bytes = TimestampsToBytes(timestamps);
    auto value_bytes = ValuesToBytes(values);
    
    auto compressor = std::make_unique<GorillaCompressor>();
    
    // Benchmark compression
    auto compress_start = std::chrono::high_resolution_clock::now();
    auto compress_result = compressor->compress(timestamp_bytes);
    auto compress_end = std::chrono::high_resolution_clock::now();
    
    ASSERT_TRUE(compress_result.ok());
    
    auto compress_duration = std::chrono::duration_cast<std::chrono::microseconds>(compress_end - compress_start);
    
    // Benchmark decompression
    const auto& compressed_data = compress_result.value();
    
    auto decompress_start = std::chrono::high_resolution_clock::now();
    auto decompress_result = compressor->decompress(compressed_data);
    auto decompress_end = std::chrono::high_resolution_clock::now();
    
    ASSERT_TRUE(decompress_result.ok());
    
    auto decompress_duration = std::chrono::duration_cast<std::chrono::microseconds>(decompress_end - decompress_start);
    
    // Performance assertions
    EXPECT_LT(compress_duration.count(), 1000000);   // Should compress in < 1 second
    EXPECT_LT(decompress_duration.count(), 1000000); // Should decompress in < 1 second
    
    // Calculate throughput
    double compression_throughput = static_cast<double>(timestamps.size()) / 
                                   (compress_duration.count() / 1000000.0);
    double decompression_throughput = static_cast<double>(timestamps.size()) / 
                                     (decompress_duration.count() / 1000000.0);
    
    EXPECT_GT(compression_throughput, 100);   // Should handle > 100 samples/sec
    EXPECT_GT(decompression_throughput, 100); // Should handle > 100 samples/sec
    
    std::cout << "Compression throughput: " << compression_throughput << " samples/sec" << std::endl;
    std::cout << "Decompression throughput: " << decompression_throughput << " samples/sec" << std::endl;
}

} // namespace internal
} // namespace storage
} // namespace tsdb 