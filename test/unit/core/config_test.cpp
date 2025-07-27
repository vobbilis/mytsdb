#include <gtest/gtest.h>
#include "tsdb/core/config.h"
#include <string>

namespace tsdb {
namespace core {
namespace {

TEST(StorageConfigTest, DefaultConstruction) {
    StorageConfig config;
    EXPECT_EQ(config.data_dir, "");
    EXPECT_EQ(config.block_size, 0);
    EXPECT_EQ(config.max_blocks_per_series, 0);
    EXPECT_EQ(config.cache_size_bytes, 0);
    EXPECT_EQ(config.block_duration, 0);
    EXPECT_EQ(config.retention_period, 0);
    EXPECT_FALSE(config.enable_compression);
}

TEST(StorageConfigTest, CustomConstruction) {
    StorageConfig config;
    config.data_dir = "/tmp/tsdb";
    config.block_size = 4096;
    config.max_blocks_per_series = 1000;
    config.cache_size_bytes = 1024 * 1024;
    config.block_duration = 3600 * 1000;
    config.retention_period = 7 * 24 * 3600 * 1000;
    config.enable_compression = true;
    
    EXPECT_EQ(config.data_dir, "/tmp/tsdb");
    EXPECT_EQ(config.block_size, 4096);
    EXPECT_EQ(config.max_blocks_per_series, 1000);
    EXPECT_EQ(config.cache_size_bytes, 1024 * 1024);
    EXPECT_EQ(config.block_duration, 3600 * 1000);
    EXPECT_EQ(config.retention_period, 7 * 24 * 3600 * 1000);
    EXPECT_TRUE(config.enable_compression);
}

TEST(StorageConfigTest, CopyConstruction) {
    StorageConfig original;
    original.data_dir = "/tmp/tsdb";
    original.block_size = 4096;
    original.enable_compression = true;
    
    StorageConfig copy(original);
    EXPECT_EQ(copy.data_dir, original.data_dir);
    EXPECT_EQ(copy.block_size, original.block_size);
    EXPECT_EQ(copy.enable_compression, original.enable_compression);
}

TEST(StorageConfigTest, Assignment) {
    StorageConfig config1;
    config1.data_dir = "/tmp/tsdb1";
    config1.block_size = 2048;
    
    StorageConfig config2;
    config2.data_dir = "/tmp/tsdb2";
    config2.block_size = 4096;
    
    config1 = config2;
    EXPECT_EQ(config1.data_dir, "/tmp/tsdb2");
    EXPECT_EQ(config1.block_size, 4096);
}

TEST(StorageConfigTest, DefaultFactory) {
    StorageConfig config = StorageConfig::Default();
    EXPECT_EQ(config.data_dir, "data");
    EXPECT_EQ(config.block_size, 64 * 1024 * 1024);  // 64MB
    EXPECT_EQ(config.max_blocks_per_series, 1024);
    EXPECT_EQ(config.cache_size_bytes, 1024 * 1024 * 1024);  // 1GB
    EXPECT_EQ(config.block_duration, 3600 * 1000);  // 1 hour
    EXPECT_EQ(config.retention_period, 7 * 24 * 3600 * 1000);  // 1 week
    EXPECT_TRUE(config.enable_compression);
}

TEST(GranularityTest, DefaultConstruction) {
    Granularity granularity;
    EXPECT_EQ(granularity.type, GranularityType::NORMAL);
    EXPECT_EQ(granularity.min_interval, 0);
    EXPECT_EQ(granularity.retention, 0);
}

TEST(GranularityTest, FactoryMethods) {
    auto high_freq = Granularity::HighFrequency();
    EXPECT_EQ(high_freq.type, GranularityType::HIGH_FREQUENCY);
    EXPECT_EQ(high_freq.min_interval, 100'000);  // 100 microseconds
    EXPECT_EQ(high_freq.retention, 86'400'000);  // 24 hours
    
    auto normal = Granularity::Normal();
    EXPECT_EQ(normal.type, GranularityType::NORMAL);
    EXPECT_EQ(normal.min_interval, 1'000);  // 1 second
    EXPECT_EQ(normal.retention, 604'800'000);  // 1 week
    
    auto low_freq = Granularity::LowFrequency();
    EXPECT_EQ(low_freq.type, GranularityType::LOW_FREQUENCY);
    EXPECT_EQ(low_freq.min_interval, 60'000);  // 1 minute
    EXPECT_EQ(low_freq.retention, 31'536'000'000);  // 1 year
}

TEST(HistogramConfigTest, DefaultConstruction) {
    HistogramConfig config;
    EXPECT_EQ(config.relative_accuracy, 0.0);
    EXPECT_EQ(config.max_num_buckets, 0);
    EXPECT_FALSE(config.use_fixed_buckets);
    EXPECT_TRUE(config.bounds.empty());
}

TEST(HistogramConfigTest, DefaultFactory) {
    HistogramConfig config = HistogramConfig::Default();
    EXPECT_EQ(config.relative_accuracy, 0.01);  // 1%
    EXPECT_EQ(config.max_num_buckets, 2048);
    EXPECT_FALSE(config.use_fixed_buckets);
    EXPECT_TRUE(config.bounds.empty());
}

TEST(QueryConfigTest, DefaultConstruction) {
    QueryConfig config;
    EXPECT_EQ(config.max_concurrent_queries, 0);
    EXPECT_EQ(config.query_timeout, 0);
    EXPECT_EQ(config.max_samples_per_query, 0);
    EXPECT_EQ(config.max_series_per_query, 0);
}

TEST(QueryConfigTest, DefaultFactory) {
    QueryConfig config = QueryConfig::Default();
    EXPECT_EQ(config.max_concurrent_queries, 100);
    EXPECT_EQ(config.query_timeout, 30 * 1000);  // 30 seconds
    EXPECT_EQ(config.max_samples_per_query, 1000000);  // 1M samples
    EXPECT_EQ(config.max_series_per_query, 10000);  // 10K series
}

TEST(ConfigTest, GlobalConfig) {
    Config config;
    
    // Test default construction
    EXPECT_EQ(config.storage().data_dir, "");
    EXPECT_EQ(config.storage().block_size, 0);
    
    // Test setting values through the config
    // Note: The actual Config class doesn't provide setters, so we test the getters
    auto storage_config = config.storage();
    auto query_config = config.query();
    auto histogram_config = config.histogram();
    auto granularity_config = config.granularity();
    
    EXPECT_EQ(storage_config.data_dir, "");
    EXPECT_EQ(query_config.max_concurrent_queries, 0);
    EXPECT_EQ(histogram_config.relative_accuracy, 0.0);
    EXPECT_EQ(granularity_config.type, GranularityType::NORMAL);
}

TEST(ConfigTest, DefaultFactory) {
    Config config = Config::Default();
    
    auto storage_config = config.storage();
    auto query_config = config.query();
    auto histogram_config = config.histogram();
    auto granularity_config = config.granularity();
    
    // Test that default values are set
    EXPECT_EQ(storage_config.data_dir, "data");
    EXPECT_EQ(query_config.max_concurrent_queries, 100);
    EXPECT_EQ(histogram_config.relative_accuracy, 0.01);
    EXPECT_EQ(granularity_config.type, GranularityType::NORMAL);
}

} // namespace
} // namespace core
} // namespace tsdb 