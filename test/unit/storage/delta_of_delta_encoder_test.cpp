#include "tsdb/storage/delta_of_delta_encoder.h"
#include <gtest/gtest.h>
#include <random>
#include <algorithm>
#include <chrono>

namespace tsdb {
namespace storage {
namespace internal {

class DeltaOfDeltaEncoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.min_block_size = 16;
        config_.max_block_size = 512;
        config_.enable_irregular_handling = true;
        config_.enable_zigzag_encoding = true;
        config_.compression_level = 6;
        encoder_ = std::make_unique<DeltaOfDeltaEncoder>(config_);
    }
    
    DeltaOfDeltaConfig config_;
    std::unique_ptr<DeltaOfDeltaEncoder> encoder_;
    
    // Helper functions to generate test data
    std::vector<int64_t> generateRegularTimestamps(size_t count, int64_t interval = 1000) {
        std::vector<int64_t> timestamps;
        timestamps.reserve(count);
        
        int64_t current_time = 1609459200000; // 2021-01-01 00:00:00 UTC
        for (size_t i = 0; i < count; i++) {
            timestamps.push_back(current_time);
            current_time += interval;
        }
        
        return timestamps;
    }
    
    std::vector<int64_t> generateIrregularTimestamps(size_t count) {
        std::vector<int64_t> timestamps;
        timestamps.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int64_t> interval_dist(500, 2000);
        
        int64_t current_time = 1609459200000; // 2021-01-01 00:00:00 UTC
        for (size_t i = 0; i < count; i++) {
            timestamps.push_back(current_time);
            current_time += interval_dist(gen);
        }
        
        return timestamps;
    }
    
    std::vector<int64_t> generateHighFrequencyTimestamps(size_t count) {
        std::vector<int64_t> timestamps;
        timestamps.reserve(count);
        
        int64_t current_time = 1609459200000; // 2021-01-01 00:00:00 UTC
        for (size_t i = 0; i < count; i++) {
            timestamps.push_back(current_time);
            current_time += 10; // 10ms intervals
        }
        
        return timestamps;
    }
    
    std::vector<int64_t> generateLowFrequencyTimestamps(size_t count) {
        std::vector<int64_t> timestamps;
        timestamps.reserve(count);
        
        int64_t current_time = 1609459200000; // 2021-01-01 00:00:00 UTC
        for (size_t i = 0; i < count; i++) {
            timestamps.push_back(current_time);
            current_time += 60000; // 1 minute intervals
        }
        
        return timestamps;
    }
};

TEST_F(DeltaOfDeltaEncoderTest, BasicCompressionDecompression) {
    auto timestamps = generateRegularTimestamps(100);
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    // Verify data integrity
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, RegularIntervalCompression) {
    auto timestamps = generateRegularTimestamps(500, 1000);
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Check compression ratio
    double compression_ratio = static_cast<double>(timestamps.size() * sizeof(int64_t)) / compressed.size();
    EXPECT_GT(compression_ratio, 2.0); // Should achieve at least 2x compression
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, IrregularIntervalCompression) {
    auto timestamps = generateIrregularTimestamps(200);
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, HighFrequencyCompression) {
    auto timestamps = generateHighFrequencyTimestamps(1000);
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Check compression ratio for high-frequency data
    double compression_ratio = static_cast<double>(timestamps.size() * sizeof(int64_t)) / compressed.size();
    EXPECT_GT(compression_ratio, 3.0); // Should achieve good compression for regular intervals
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, LowFrequencyCompression) {
    auto timestamps = generateLowFrequencyTimestamps(50);
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, EmptyData) {
    std::vector<int64_t> empty_timestamps;
    
    // Compress empty data
    auto compressed = encoder_->compress(empty_timestamps);
    EXPECT_TRUE(compressed.empty());
    
    // Decompress empty data
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
}

TEST_F(DeltaOfDeltaEncoderTest, SingleTimestamp) {
    std::vector<int64_t> single_timestamp = {1609459200000};
    
    // Compress
    auto compressed = encoder_->compress(single_timestamp);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), 1);
    EXPECT_EQ(decompressed[0], single_timestamp[0]);
}

TEST_F(DeltaOfDeltaEncoderTest, TwoTimestamps) {
    std::vector<int64_t> two_timestamps = {1609459200000, 1609459201000};
    
    // Compress
    auto compressed = encoder_->compress(two_timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), 2);
    EXPECT_EQ(decompressed[0], two_timestamps[0]);
    EXPECT_EQ(decompressed[1], two_timestamps[1]);
}

TEST_F(DeltaOfDeltaEncoderTest, CustomBlockSize) {
    auto timestamps = generateRegularTimestamps(200);
    
    // Compress with custom block size
    auto compressed = encoder_->compressWithBlockSize(timestamps, 32);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, CompressionStats) {
    auto timestamps = generateRegularTimestamps(100);
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    
    // Check stats
    auto stats = encoder_->getStats();
    EXPECT_EQ(stats.original_size, timestamps.size() * sizeof(int64_t));
    EXPECT_EQ(stats.compressed_size, compressed.size());
    EXPECT_GT(stats.compression_ratio, 1.0);
    EXPECT_GT(stats.blocks_processed, 0);
    EXPECT_GT(stats.average_delta, 0.0);
}

TEST_F(DeltaOfDeltaEncoderTest, ConfigurationUpdate) {
    auto timestamps = generateRegularTimestamps(100);
    
    // Test with default config
    auto compressed1 = encoder_->compress(timestamps);
    
    // Update config
    DeltaOfDeltaConfig new_config = config_;
    new_config.min_block_size = 8;
    new_config.max_block_size = 64;
    encoder_->updateConfig(new_config);
    
    // Test with new config
    auto compressed2 = encoder_->compress(timestamps);
    
    // Both should work correctly
    auto decompressed1 = encoder_->decompress(compressed1);
    auto decompressed2 = encoder_->decompress(compressed2);
    
    EXPECT_EQ(decompressed1.size(), timestamps.size());
    EXPECT_EQ(decompressed2.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed1[i], timestamps[i]);
        EXPECT_EQ(decompressed2[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, FactoryCreation) {
    // Test default factory
    auto default_encoder = DeltaOfDeltaEncoderFactory::create();
    EXPECT_NE(default_encoder, nullptr);
    
    // Test custom config factory
    auto custom_encoder = DeltaOfDeltaEncoderFactory::create(config_);
    EXPECT_NE(custom_encoder, nullptr);
    
    // Test use case factory
    auto high_freq_encoder = DeltaOfDeltaEncoderFactory::createForUseCase("high_frequency");
    EXPECT_NE(high_freq_encoder, nullptr);
    
    auto low_freq_encoder = DeltaOfDeltaEncoderFactory::createForUseCase("low_frequency");
    EXPECT_NE(low_freq_encoder, nullptr);
    
    auto irregular_encoder = DeltaOfDeltaEncoderFactory::createForUseCase("irregular");
    EXPECT_NE(irregular_encoder, nullptr);
}

TEST_F(DeltaOfDeltaEncoderTest, EdgeCaseNegativeDeltas) {
    std::vector<int64_t> timestamps = {
        1609459200000,  // Base time
        1609459199000,  // -1000ms
        1609459198000,  // -1000ms
        1609459197000,  // -1000ms
        1609459196000   // -1000ms
    };
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, EdgeCaseZeroDeltas) {
    std::vector<int64_t> timestamps = {
        1609459200000,  // Base time
        1609459200000,  // Same time
        1609459200000,  // Same time
        1609459200000,  // Same time
        1609459200000   // Same time
    };
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, EdgeCaseLargeTimestamps) {
    std::vector<int64_t> timestamps = {
        INT64_MAX - 4000,
        INT64_MAX - 3000,
        INT64_MAX - 2000,
        INT64_MAX - 1000,
        INT64_MAX
    };
    
    // Compress
    auto compressed = encoder_->compress(timestamps);
    EXPECT_FALSE(compressed.empty());
    
    // Decompress and verify
    auto decompressed = encoder_->decompress(compressed);
    EXPECT_EQ(decompressed.size(), timestamps.size());
    
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, PerformanceBenchmark) {
    const size_t large_size = 10000;
    auto regular_timestamps = generateRegularTimestamps(large_size);
    auto irregular_timestamps = generateIrregularTimestamps(large_size);
    
    // Measure compression time for regular intervals
    auto start = std::chrono::high_resolution_clock::now();
    auto regular_compressed = encoder_->compress(regular_timestamps);
    auto regular_time = std::chrono::high_resolution_clock::now() - start;
    
    // Measure compression time for irregular intervals
    start = std::chrono::high_resolution_clock::now();
    auto irregular_compressed = encoder_->compress(irregular_timestamps);
    auto irregular_time = std::chrono::high_resolution_clock::now() - start;
    
    // Verify both work correctly
    auto regular_decompressed = encoder_->decompress(regular_compressed);
    auto irregular_decompressed = encoder_->decompress(irregular_compressed);
    
    EXPECT_EQ(regular_decompressed.size(), regular_timestamps.size());
    EXPECT_EQ(irregular_decompressed.size(), irregular_timestamps.size());
    
    // Check compression ratios
    double regular_ratio = static_cast<double>(regular_timestamps.size() * sizeof(int64_t)) / regular_compressed.size();
    double irregular_ratio = static_cast<double>(irregular_timestamps.size() * sizeof(int64_t)) / irregular_compressed.size();
    
    EXPECT_GT(regular_ratio, 2.0);   // Regular intervals should compress well
    EXPECT_GT(irregular_ratio, 1.5); // Irregular intervals should still compress
    
    // Performance should be reasonable (less than 1ms for 10k timestamps)
    auto regular_ms = std::chrono::duration_cast<std::chrono::microseconds>(regular_time).count();
    auto irregular_ms = std::chrono::duration_cast<std::chrono::microseconds>(irregular_time).count();
    
    EXPECT_LT(regular_ms, 1000);   // Less than 1ms
    EXPECT_LT(irregular_ms, 1000); // Less than 1ms
}

TEST_F(DeltaOfDeltaEncoderTest, ZigzagEncoding) {
    // Test with zigzag encoding enabled
    config_.enable_zigzag_encoding = true;
    auto zigzag_encoder = std::make_unique<DeltaOfDeltaEncoder>(config_);
    
    auto timestamps = generateRegularTimestamps(100);
    auto compressed = zigzag_encoder->compress(timestamps);
    auto decompressed = zigzag_encoder->decompress(compressed);
    
    EXPECT_EQ(decompressed.size(), timestamps.size());
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed[i], timestamps[i]);
    }
    
    // Test with zigzag encoding disabled
    config_.enable_zigzag_encoding = false;
    auto no_zigzag_encoder = std::make_unique<DeltaOfDeltaEncoder>(config_);
    
    auto compressed2 = no_zigzag_encoder->compress(timestamps);
    auto decompressed2 = no_zigzag_encoder->decompress(compressed2);
    
    EXPECT_EQ(decompressed2.size(), timestamps.size());
    for (size_t i = 0; i < timestamps.size(); i++) {
        EXPECT_EQ(decompressed2[i], timestamps[i]);
    }
}

TEST_F(DeltaOfDeltaEncoderTest, ResetStats) {
    auto timestamps = generateRegularTimestamps(100);
    
    // Compress and get stats
    encoder_->compress(timestamps);
    auto stats1 = encoder_->getStats();
    EXPECT_GT(stats1.blocks_processed, 0);
    
    // Reset stats
    encoder_->resetStats();
    auto stats2 = encoder_->getStats();
    EXPECT_EQ(stats2.blocks_processed, 0);
    EXPECT_EQ(stats2.original_size, 0);
    EXPECT_EQ(stats2.compressed_size, 0);
}

} // namespace internal
} // namespace storage
} // namespace tsdb 