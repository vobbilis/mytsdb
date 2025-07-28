#include "tsdb/storage/adaptive_compressor.h"
#include <gtest/gtest.h>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>

namespace tsdb {
namespace storage {
namespace internal {

class AdaptiveCompressorTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.min_samples_for_detection = 5;
        config_.counter_threshold = 0.95;
        config_.constant_threshold = 0.99;
        compressor_ = std::make_unique<AdaptiveCompressor>(config_);
    }
    
    AdaptiveCompressionConfig config_;
    std::unique_ptr<AdaptiveCompressor> compressor_;
    
    // Helper functions to generate test data
    std::vector<double> generateCounterData(size_t count) {
        std::vector<double> data;
        data.reserve(count);
        
        double value = 1000.0;
        for (size_t i = 0; i < count; i++) {
            data.push_back(value);
            value += 1.0 + (i % 10); // Some variation in increment
        }
        return data;
    }
    
    std::vector<double> generateGaugeData(size_t count) {
        std::vector<double> data;
        data.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> dist(100.0, 20.0);
        
        for (size_t i = 0; i < count; i++) {
            data.push_back(dist(gen));
        }
        return data;
    }
    
    std::vector<double> generateHistogramData(size_t count) {
        std::vector<double> data;
        data.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::gamma_distribution<double> dist(2.0, 1.0); // Non-negative distribution
        
        for (size_t i = 0; i < count; i++) {
            data.push_back(dist(gen));
        }
        return data;
    }
    
    std::vector<double> generateConstantData(size_t count) {
        std::vector<double> data(count, 42.0);
        return data;
    }
};

TEST_F(AdaptiveCompressorTest, TypeDetectionCounter) {
    auto data = generateCounterData(20);
    DataType detected = compressor_->detectDataType(data);
    EXPECT_EQ(detected, DataType::COUNTER);
}

TEST_F(AdaptiveCompressorTest, TypeDetectionGauge) {
    auto data = generateGaugeData(20);
    DataType detected = compressor_->detectDataType(data);
    EXPECT_EQ(detected, DataType::GAUGE);
}

TEST_F(AdaptiveCompressorTest, TypeDetectionHistogram) {
    auto data = generateHistogramData(20);
    DataType detected = compressor_->detectDataType(data);
    EXPECT_EQ(detected, DataType::HISTOGRAM);
}

TEST_F(AdaptiveCompressorTest, TypeDetectionConstant) {
    auto data = generateConstantData(20);
    DataType detected = compressor_->detectDataType(data);
    EXPECT_EQ(detected, DataType::CONSTANT);
}

TEST_F(AdaptiveCompressorTest, TypeDetectionSmallDataset) {
    auto data = generateCounterData(3); // Below min_samples_for_detection
    DataType detected = compressor_->detectDataType(data);
    EXPECT_EQ(detected, DataType::GAUGE); // Should default to gauge
}

TEST_F(AdaptiveCompressorTest, CompressionDecompressionCounter) {
    auto original_data = generateCounterData(50);
    
    // Compress
    auto compressed = compressor_->compress(original_data);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress
    auto decompressed = compressor_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), original_data.size());
    
    // Verify data integrity
    for (size_t i = 0; i < original_data.size(); i++) {
        EXPECT_NEAR(decompressed[i], original_data[i], 1e-10);
    }
}

TEST_F(AdaptiveCompressorTest, CompressionDecompressionGauge) {
    auto original_data = generateGaugeData(50);
    
    // Compress
    auto compressed = compressor_->compress(original_data);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress
    auto decompressed = compressor_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), original_data.size());
    
    // Verify data integrity with appropriate tolerance for lossy compression
    for (size_t i = 0; i < original_data.size(); i++) {
        // Use higher tolerance for gauge data due to lossy compression
        double tolerance = 1e-3; // 0.001 tolerance for gauge compression
        EXPECT_NEAR(decompressed[i], original_data[i], tolerance);
    }
}

TEST_F(AdaptiveCompressorTest, CompressionDecompressionHistogram) {
    auto original_data = generateHistogramData(50);
    
    // Compress
    auto compressed = compressor_->compress(original_data);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress
    auto decompressed = compressor_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), original_data.size());
    
    // Verify data integrity (with some tolerance for quantization)
    for (size_t i = 0; i < original_data.size(); i++) {
        EXPECT_NEAR(decompressed[i], original_data[i], 1e-3);
    }
}

TEST_F(AdaptiveCompressorTest, CompressionDecompressionConstant) {
    auto original_data = generateConstantData(50);
    
    // Compress
    auto compressed = compressor_->compress(original_data);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress
    auto decompressed = compressor_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), original_data.size());
    
    // Verify data integrity
    for (size_t i = 0; i < original_data.size(); i++) {
        EXPECT_NEAR(decompressed[i], original_data[i], 1e-10);
    }
}

TEST_F(AdaptiveCompressorTest, CompressionRatio) {
    auto counter_data = generateCounterData(100);
    auto gauge_data = generateGaugeData(100);
    auto histogram_data = generateHistogramData(100);
    auto constant_data = generateConstantData(100);
    
    // Compress each type
    auto counter_compressed = compressor_->compress(counter_data);
    auto gauge_compressed = compressor_->compress(gauge_data);
    auto histogram_compressed = compressor_->compress(histogram_data);
    auto constant_compressed = compressor_->compress(constant_data);
    
    // Calculate compression ratios
    double counter_ratio = static_cast<double>(counter_compressed.size()) / (counter_data.size() * sizeof(double));
    double gauge_ratio = static_cast<double>(gauge_compressed.size()) / (gauge_data.size() * sizeof(double));
    double histogram_ratio = static_cast<double>(histogram_compressed.size()) / (histogram_data.size() * sizeof(double));
    double constant_ratio = static_cast<double>(constant_compressed.size()) / (constant_data.size() * sizeof(double));
    
    // Verify compression ratios are reasonable
    EXPECT_LT(counter_ratio, 1.0);
    EXPECT_LT(gauge_ratio, 1.0);
    EXPECT_LT(histogram_ratio, 1.0);
    EXPECT_LT(constant_ratio, 1.0);
    
    // Constant data should have the best compression
    EXPECT_LT(constant_ratio, counter_ratio);
    EXPECT_LT(constant_ratio, gauge_ratio);
    EXPECT_LT(constant_ratio, histogram_ratio);
}

TEST_F(AdaptiveCompressorTest, MetricsTracking) {
    auto counter_data = generateCounterData(50);
    auto gauge_data = generateGaugeData(50);
    auto constant_data = generateConstantData(50);
    
    // Reset metrics
    compressor_->resetMetrics();
    
    // Compress different types
    compressor_->compress(counter_data);
    compressor_->compress(gauge_data);
    compressor_->compress(constant_data);
    
    // Check metrics
    const auto& metrics = compressor_->getMetrics();
    EXPECT_EQ(metrics.counter_compressions.load(), 1);
    EXPECT_EQ(metrics.gauge_compressions.load(), 1);
    EXPECT_EQ(metrics.constant_compressions.load(), 1);
    EXPECT_EQ(metrics.histogram_compressions.load(), 0);
    
    // Check that we have some compression
    EXPECT_GT(metrics.total_original_bytes.load(), 0);
    EXPECT_GT(metrics.total_compressed_bytes.load(), 0);
    EXPECT_LT(metrics.getCompressionRatio(), 1.0);
}

TEST_F(AdaptiveCompressorTest, ExplicitTypeCompression) {
    auto data = generateGaugeData(50);
    
    // Compress with explicit gauge type (more appropriate for gauge data)
    auto compressed = compressor_->compressWithType(data, DataType::GAUGE);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress
    auto decompressed = compressor_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), data.size());
    
    // Verify data integrity with appropriate tolerance for lossy compression
    for (size_t i = 0; i < data.size(); i++) {
        // Use higher tolerance for gauge data due to lossy compression
        EXPECT_NEAR(decompressed[i], data[i], 1e-1); // 0.1 tolerance for lossy compression
    }
}

TEST_F(AdaptiveCompressorTest, EmptyData) {
    std::vector<double> empty_data;
    
    // Compress empty data
    auto compressed = compressor_->compress(empty_data);
    EXPECT_TRUE(compressed.empty());
    
    // Decompress empty data
    auto decompressed = compressor_->decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
}

TEST_F(AdaptiveCompressorTest, SingleValue) {
    std::vector<double> single_value = {42.0};
    
    // Compress single value
    auto compressed = compressor_->compress(single_value);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress
    auto decompressed = compressor_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), 1);
    EXPECT_NEAR(decompressed[0], 42.0, 1e-10);
}

TEST_F(AdaptiveCompressorTest, ConfigurationUpdate) {
    auto data = generateCounterData(20);
    
    // Test with default config
    DataType detected1 = compressor_->detectDataType(data);
    
    // Update config to be more strict
    AdaptiveCompressionConfig strict_config = config_;
    strict_config.counter_threshold = 0.99; // More strict
    compressor_->updateConfig(strict_config);
    
    // Test with strict config
    DataType detected2 = compressor_->detectDataType(data);
    
    // The more strict config might not detect it as counter
    // This is expected behavior
    EXPECT_TRUE(detected1 == DataType::COUNTER || detected2 == DataType::COUNTER);
}

TEST_F(AdaptiveCompressorTest, FactoryCreation) {
    auto factory_compressor = AdaptiveCompressorFactory::create(config_);
    EXPECT_NE(factory_compressor, nullptr);
    
    auto data = generateCounterData(20);
    auto compressed = factory_compressor->compress(data);
    EXPECT_FALSE(compressed.empty());
}

TEST_F(AdaptiveCompressorTest, EdgeCaseMonotonicWithResets) {
    // Create data that's mostly increasing but has occasional resets
    std::vector<double> data = {100, 101, 102, 103, 104, 50, 51, 52, 53, 54};
    
    DataType detected = compressor_->detectDataType(data);
    // Should not be detected as counter due to resets
    EXPECT_NE(detected, DataType::COUNTER);
}

TEST_F(AdaptiveCompressorTest, EdgeCaseAlmostConstant) {
    // Create data that's almost constant but not quite
    std::vector<double> data = {42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 43.0};
    
    // Temporarily adjust threshold for this test
    AdaptiveCompressionConfig test_config = config_;
    test_config.constant_threshold = 0.85; // 85% threshold for this test
    compressor_->updateConfig(test_config);
    
    DataType detected = compressor_->detectDataType(data);
    // Should be detected as constant due to adjusted threshold
    EXPECT_EQ(detected, DataType::CONSTANT);
}

TEST_F(AdaptiveCompressorTest, PerformanceBenchmark) {
    const size_t large_size = 10000;
    auto counter_data = generateCounterData(large_size);
    auto gauge_data = generateGaugeData(large_size);
    auto histogram_data = generateHistogramData(large_size);
    auto constant_data = generateConstantData(large_size);
    
    // Measure compression time and ratios
    auto start = std::chrono::high_resolution_clock::now();
    auto counter_compressed = compressor_->compress(counter_data);
    auto counter_time = std::chrono::high_resolution_clock::now() - start;
    
    start = std::chrono::high_resolution_clock::now();
    auto gauge_compressed = compressor_->compress(gauge_data);
    auto gauge_time = std::chrono::high_resolution_clock::now() - start;
    
    start = std::chrono::high_resolution_clock::now();
    auto histogram_compressed = compressor_->compress(histogram_data);
    auto histogram_time = std::chrono::high_resolution_clock::now() - start;
    
    start = std::chrono::high_resolution_clock::now();
    auto constant_compressed = compressor_->compress(constant_data);
    auto constant_time = std::chrono::high_resolution_clock::now() - start;
    
    // Calculate compression ratios
    double counter_ratio = static_cast<double>(counter_compressed.size()) / (counter_data.size() * sizeof(double));
    double gauge_ratio = static_cast<double>(gauge_compressed.size()) / (gauge_data.size() * sizeof(double));
    double histogram_ratio = static_cast<double>(histogram_compressed.size()) / (histogram_data.size() * sizeof(double));
    double constant_ratio = static_cast<double>(constant_compressed.size()) / (constant_data.size() * sizeof(double));
    
    // Verify performance targets are met
    EXPECT_LT(counter_ratio, 0.8); // 20% compression improvement target
    EXPECT_LT(gauge_ratio, 1.0);   // Some compression
    EXPECT_LT(histogram_ratio, 0.8); // 20% compression improvement target
    EXPECT_LT(constant_ratio, 0.1); // 90% compression for constants
    
    // Verify reasonable performance (should complete in reasonable time)
    auto max_expected_time = std::chrono::milliseconds(100);
    EXPECT_LT(counter_time, max_expected_time);
    EXPECT_LT(gauge_time, max_expected_time);
    EXPECT_LT(histogram_time, max_expected_time);
    EXPECT_LT(constant_time, max_expected_time);
}

} // namespace internal
} // namespace storage
} // namespace tsdb 