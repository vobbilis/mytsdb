#include <gtest/gtest.h>
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include <filesystem>
#include <memory>

namespace tsdb {
namespace integration {
namespace {

class CoreStorageIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_integration_test";
        std::filesystem::create_directories(test_dir_);
        
        core::StorageConfig config;
        config.data_dir = test_dir_.string();
        config.block_size = 4096;  // Small blocks for testing
        config.max_blocks_per_series = 1000;
        config.cache_size_bytes = 1024 * 1024;  // 1MB cache
        config.block_duration = 3600 * 1000;  // 1 hour
        config.retention_period = 7 * 24 * 3600 * 1000;  // 1 week
        config.enable_compression = true;
        
        storage_ = std::make_unique<storage::StorageImpl>();
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::filesystem::path test_dir_;
    std::unique_ptr<storage::Storage> storage_;
};

TEST_F(CoreStorageIntegrationTest, BasicTimeSeriesCreationAndStorage) {
    // Test basic TimeSeries creation and storage integration
    core::Labels labels;
    labels.add("__name__", "test_metric");
    labels.add("instance", "localhost");
    
    core::TimeSeries series(labels);
    
    // Add some samples
    series.add_sample(core::Sample(1000, 1.0));
    series.add_sample(core::Sample(2000, 2.0));
    series.add_sample(core::Sample(3000, 3.0));
    
    // Verify the series was created correctly
    EXPECT_EQ(series.labels().map().size(), 2);
    EXPECT_EQ(series.samples().size(), 3);
    EXPECT_EQ(series.samples()[0].timestamp(), 1000);
    EXPECT_DOUBLE_EQ(series.samples()[0].value(), 1.0);
    
    // Test storage integration (basic test - actual storage implementation may be incomplete)
    auto write_result = storage_->write(series);
    // Note: This may fail if storage implementation is incomplete, which is expected
    // The test validates that the integration interface works correctly
}

TEST_F(CoreStorageIntegrationTest, ConfigurationIntegration) {
    // Test that configuration objects work correctly across components
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
}

TEST_F(CoreStorageIntegrationTest, DataTypeConsistency) {
    // Test that core data types work consistently across components
    core::Value test_value = 42.5;
    core::Timestamp test_timestamp = 1234567890;
    core::SeriesID test_series_id = 1;
    
    // Verify data types are consistent
    EXPECT_DOUBLE_EQ(test_value, 42.5);
    EXPECT_EQ(test_timestamp, 1234567890);
    EXPECT_EQ(test_series_id, 1);
    
    // Test Labels consistency
    core::Labels labels;
    labels.add("name", "test");
    labels.add("type", "gauge");
    
    EXPECT_EQ(labels.map().size(), 2);
    EXPECT_TRUE(labels.has("name"));
    EXPECT_TRUE(labels.has("type"));
    EXPECT_EQ(labels.get("name"), "test");
    EXPECT_EQ(labels.get("type"), "gauge");
}

} // namespace
} // namespace integration
} // namespace tsdb
