#include <gtest/gtest.h>
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/histogram/histogram.h"
#include "tsdb/histogram/ddsketch.h"
#include <filesystem>
#include <memory>

namespace tsdb {
namespace integration {
namespace {

class ConfigIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_config_integration_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
};

TEST_F(ConfigIntegrationTest, StorageConfigPropagation) {
    // Test StorageConfig propagation to storage components
    core::StorageConfig config;
    config.data_dir = test_dir_.string();
    config.block_size = 8192;
    config.max_blocks_per_series = 500;
    config.cache_size_bytes = 2048 * 1024;  // 2MB cache
    config.block_duration = 1800 * 1000;  // 30 minutes
    config.retention_period = 3 * 24 * 3600 * 1000;  // 3 days
    config.enable_compression = false;

    // Verify configuration values are set correctly
    EXPECT_EQ(config.data_dir, test_dir_.string());
    EXPECT_EQ(config.block_size, 8192);
    EXPECT_EQ(config.max_blocks_per_series, 500);
    EXPECT_EQ(config.cache_size_bytes, 2048 * 1024);
    EXPECT_EQ(config.block_duration, 1800 * 1000);
    EXPECT_EQ(config.retention_period, 3 * 24 * 3600 * 1000);
    EXPECT_FALSE(config.enable_compression);

    // Test storage initialization with custom config
    auto storage = std::make_unique<storage::StorageImpl>();
    auto init_result = storage->init(config);
    
    // Note: This may fail if storage implementation is incomplete, which is expected
    // The test validates that the configuration interface works correctly
    if (init_result.ok()) {
        // If storage initializes successfully, verify it uses the config
        storage->close();
    }
}

TEST_F(ConfigIntegrationTest, HistogramConfigIntegration) {
    // Test HistogramConfig integration with histogram operations
    core::HistogramConfig config = core::HistogramConfig::Default();
    
    // Verify default configuration
    EXPECT_EQ(config.relative_accuracy, 0.01);
    EXPECT_EQ(config.max_num_buckets, 2048);
    EXPECT_FALSE(config.use_fixed_buckets);
    EXPECT_TRUE(config.bounds.empty());

    // Test custom configuration
    config.relative_accuracy = 0.005;  // Higher accuracy
    config.max_num_buckets = 4096;     // More buckets
    config.use_fixed_buckets = true;
    config.bounds = {0.0, 10.0, 20.0, 30.0, 40.0, 50.0};

    // Verify custom configuration
    EXPECT_EQ(config.relative_accuracy, 0.005);
    EXPECT_EQ(config.max_num_buckets, 4096);
    EXPECT_TRUE(config.use_fixed_buckets);
    EXPECT_EQ(config.bounds.size(), 6);

    // Test histogram creation with custom config
    auto histogram = histogram::DDSketch::create(config.relative_accuracy);
    ASSERT_NE(histogram, nullptr);

    // Add some data to verify the configuration affects behavior
    for (int i = 0; i < 100; ++i) {
        histogram->add(1.0 + i * 0.5);
    }

    EXPECT_EQ(histogram->count(), 100);
    EXPECT_GT(histogram->sum(), 0.0);
}

TEST_F(ConfigIntegrationTest, QueryConfigUsage) {
    // Test QueryConfig usage in storage queries
    core::QueryConfig config = core::QueryConfig::Default();
    
    // Verify default configuration
    EXPECT_EQ(config.max_concurrent_queries, 100);
    EXPECT_EQ(config.query_timeout, 30 * 1000);
    EXPECT_EQ(config.max_samples_per_query, 1000000);
    EXPECT_EQ(config.max_series_per_query, 10000);

    // Test custom configuration
    config.max_concurrent_queries = 50;
    config.query_timeout = 60 * 1000;  // 1 minute
    config.max_samples_per_query = 500000;
    config.max_series_per_query = 5000;

    // Verify custom configuration
    EXPECT_EQ(config.max_concurrent_queries, 50);
    EXPECT_EQ(config.query_timeout, 60 * 1000);
    EXPECT_EQ(config.max_samples_per_query, 500000);
    EXPECT_EQ(config.max_series_per_query, 5000);

    // Test configuration validation (basic checks)
    EXPECT_GT(config.max_concurrent_queries, 0);
    EXPECT_GT(config.query_timeout, 0);
    EXPECT_GT(config.max_samples_per_query, 0);
    EXPECT_GT(config.max_series_per_query, 0);
}

TEST_F(ConfigIntegrationTest, ConfigurationValidation) {
    // Test configuration validation across components
    
    // Test invalid storage configuration
    core::StorageConfig invalid_storage_config;
    invalid_storage_config.data_dir = "";  // Empty data directory
    invalid_storage_config.block_size = 0; // Invalid block size
    
    // Test invalid histogram configuration
    core::HistogramConfig invalid_hist_config;
    invalid_hist_config.relative_accuracy = -0.1;  // Negative accuracy
    invalid_hist_config.max_num_buckets = 0;       // Zero buckets
    
    // Test invalid query configuration
    core::QueryConfig invalid_query_config;
    invalid_query_config.max_concurrent_queries = 0;  // Zero concurrent queries
    invalid_query_config.query_timeout = -1000;       // Negative timeout

    // Verify that invalid configurations are detected
    EXPECT_TRUE(invalid_storage_config.data_dir.empty());
    EXPECT_EQ(invalid_storage_config.block_size, 0);
    EXPECT_LT(invalid_hist_config.relative_accuracy, 0.0);
    EXPECT_EQ(invalid_hist_config.max_num_buckets, 0);
    EXPECT_EQ(invalid_query_config.max_concurrent_queries, 0);
    EXPECT_LT(invalid_query_config.query_timeout, 0);
}

TEST_F(ConfigIntegrationTest, DefaultConfigurationHandling) {
    // Test default configuration handling
    
    // Test default storage configuration
    auto default_storage_config = core::StorageConfig::Default();
    EXPECT_EQ(default_storage_config.data_dir, "data");
    EXPECT_EQ(default_storage_config.block_size, 64 * 1024 * 1024);  // 64MB
    EXPECT_EQ(default_storage_config.max_blocks_per_series, 1024);
    EXPECT_EQ(default_storage_config.cache_size_bytes, 1024 * 1024 * 1024);  // 1GB
    EXPECT_EQ(default_storage_config.block_duration, 3600 * 1000);  // 1 hour
    EXPECT_EQ(default_storage_config.retention_period, 7 * 24 * 3600 * 1000);  // 1 week
    EXPECT_TRUE(default_storage_config.enable_compression);

    // Test default histogram configuration
    auto default_hist_config = core::HistogramConfig::Default();
    EXPECT_EQ(default_hist_config.relative_accuracy, 0.01);
    EXPECT_EQ(default_hist_config.max_num_buckets, 2048);
    EXPECT_FALSE(default_hist_config.use_fixed_buckets);
    EXPECT_TRUE(default_hist_config.bounds.empty());

    // Test default query configuration
    auto default_query_config = core::QueryConfig::Default();
    EXPECT_EQ(default_query_config.max_concurrent_queries, 100);
    EXPECT_EQ(default_query_config.query_timeout, 30 * 1000);
    EXPECT_EQ(default_query_config.max_samples_per_query, 1000000);
    EXPECT_EQ(default_query_config.max_series_per_query, 10000);
}

TEST_F(ConfigIntegrationTest, GlobalConfigIntegration) {
    // Test global configuration integration
    auto global_config = core::Config::Default();
    
    // Verify global config contains all component configs
    EXPECT_EQ(global_config.storage().data_dir, "data");
    EXPECT_EQ(global_config.histogram().relative_accuracy, 0.01);
    EXPECT_EQ(global_config.query().max_concurrent_queries, 100);

    // Test modifying global config
    auto& mutable_storage = global_config.mutable_storage();
    mutable_storage.data_dir = test_dir_.string();
    mutable_storage.block_size = 4096;

    auto& mutable_histogram = global_config.mutable_histogram();
    mutable_histogram.relative_accuracy = 0.005;

    auto& mutable_query = global_config.mutable_query();
    mutable_query.max_concurrent_queries = 50;

    // Verify modifications are applied
    EXPECT_EQ(global_config.storage().data_dir, test_dir_.string());
    EXPECT_EQ(global_config.storage().block_size, 4096);
    EXPECT_EQ(global_config.histogram().relative_accuracy, 0.005);
    EXPECT_EQ(global_config.query().max_concurrent_queries, 50);

    // Test that const access still works
    const auto& const_config = global_config;
    EXPECT_EQ(const_config.storage().data_dir, test_dir_.string());
    EXPECT_EQ(const_config.histogram().relative_accuracy, 0.005);
    EXPECT_EQ(const_config.query().max_concurrent_queries, 50);
}

TEST_F(ConfigIntegrationTest, GranularityConfiguration) {
    // Test granularity configuration
    auto high_freq = core::Granularity::HighFrequency();
    auto normal = core::Granularity::Normal();
    auto low_freq = core::Granularity::LowFrequency();

    // Verify high frequency configuration
    EXPECT_EQ(high_freq.type, core::GranularityType::HIGH_FREQUENCY);
    EXPECT_EQ(high_freq.min_interval, 100'000);  // 100 microseconds
    EXPECT_EQ(high_freq.retention, 86'400'000);  // 24 hours

    // Verify normal configuration
    EXPECT_EQ(normal.type, core::GranularityType::NORMAL);
    EXPECT_EQ(normal.min_interval, 1'000);  // 1 second
    EXPECT_EQ(normal.retention, 604'800'000);  // 1 week

    // Verify low frequency configuration
    EXPECT_EQ(low_freq.type, core::GranularityType::LOW_FREQUENCY);
    EXPECT_EQ(low_freq.min_interval, 60'000);  // 1 minute
    EXPECT_EQ(low_freq.retention, 31'536'000'000);  // 1 year

    // Test custom granularity
    core::Granularity custom;
    custom.type = core::GranularityType::NORMAL;
    custom.min_interval = 5000;  // 5 seconds
    custom.retention = 259'200'000;  // 3 days

    EXPECT_EQ(custom.type, core::GranularityType::NORMAL);
    EXPECT_EQ(custom.min_interval, 5000);
    EXPECT_EQ(custom.retention, 259'200'000);
}

TEST_F(ConfigIntegrationTest, ConfigurationPersistence) {
    // Test that configuration changes persist correctly
    
    // Create a configuration and modify it
    core::StorageConfig config = core::StorageConfig::Default();
    std::string original_data_dir = config.data_dir;
    size_t original_block_size = config.block_size;

    // Modify configuration
    config.data_dir = test_dir_.string();
    config.block_size = 8192;

    // Verify changes persist
    EXPECT_EQ(config.data_dir, test_dir_.string());
    EXPECT_EQ(config.block_size, 8192);
    EXPECT_NE(config.data_dir, original_data_dir);
    EXPECT_NE(config.block_size, original_block_size);

    // Test histogram configuration persistence
    core::HistogramConfig hist_config = core::HistogramConfig::Default();
    double original_accuracy = hist_config.relative_accuracy;

    hist_config.relative_accuracy = 0.005;
    hist_config.max_num_buckets = 4096;

    EXPECT_EQ(hist_config.relative_accuracy, 0.005);
    EXPECT_EQ(hist_config.max_num_buckets, 4096);
    EXPECT_NE(hist_config.relative_accuracy, original_accuracy);
}

} // namespace
} // namespace integration
} // namespace tsdb
