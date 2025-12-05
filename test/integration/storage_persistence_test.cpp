#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include <filesystem>
#include <chrono>
#include <thread>

namespace tsdb {
namespace integration {

class StoragePersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/tsdb_persistence_test_" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        config_.data_dir = test_dir_;
        config_.block_size = 1024;
        config_.enable_compression = true;
        // Disable background processing to have manual control
        config_.background_config.enable_background_processing = false;
    }

    void TearDown() override {
        if (storage_) {
            storage_->close(); // Ensure clean shutdown
        }
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    core::StorageConfig config_;
    std::unique_ptr<storage::StorageImpl> storage_;
};

TEST_F(StoragePersistenceTest, DataSurvivesRestart) {
    // 1. Initialize Storage
    storage_ = std::make_unique<storage::StorageImpl>();
    ASSERT_TRUE(storage_->init(config_).ok());

    // 2. Write data
    int64_t start_time = 1000;
    int64_t count = 100;
    core::Labels labels({{"metric", "test_persistence"}, {"host", "server1"}});
    core::TimeSeries series(labels);
    
    for (int i = 0; i < count; ++i) {
        series.add_sample(core::Sample(start_time + i * 1000, static_cast<double>(i)));
    }
    ASSERT_TRUE(storage_->write(series).ok());

    // 3. Force flush and close (simulating shutdown)
    // We need to ensure data is flushed to blocks.
    // Since we can't easily force a block seal via public API without filling it,
    // we rely on close() to flush pending data.
    // However, the current bug is that init() doesn't load BLOCKS.
    // So we need to make sure a block is actually created.
    // Let's write enough data to trigger a block seal if possible, or rely on close() flushing.
    
    // Actually, StorageImpl::close() calls flush_nolock() which calls block_manager_->flush().
    // This should persist the current mutable block.
    storage_->close();
    storage_.reset();

    // Verify block file exists
    bool block_found = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(test_dir_)) {
        if (entry.path().extension() == ".block") {
            block_found = true;
            break;
        }
    }
    ASSERT_TRUE(block_found) << "No .block file found after close()";

    // 4. Re-initialize Storage (simulating restart)
    storage_ = std::make_unique<storage::StorageImpl>();
    ASSERT_TRUE(storage_->init(config_).ok());

    // 5. Query data
    auto result = storage_->read(labels, start_time, start_time + count * 1000);
    ASSERT_TRUE(result.ok()) << "Read failed: " << result.error();
    
    const auto& read_series = result.value();
    ASSERT_EQ(read_series.samples().size(), count) << "Data loss detected after restart!";
    
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(read_series.samples()[i].timestamp(), start_time + i * 1000);
        EXPECT_EQ(read_series.samples()[i].value(), static_cast<double>(i));
    }
}

TEST_F(StoragePersistenceTest, DataSurvivesWALLoss) {
    // 1. Initialize Storage
    storage_ = std::make_unique<storage::StorageImpl>();
    ASSERT_TRUE(storage_->init(config_).ok());

    // 2. Write data
    int64_t start_time = 1000;
    int64_t count = 100;
    core::Labels labels({{"metric", "test_persistence_wal_loss"}, {"host", "server1"}});
    core::TimeSeries series(labels);
    
    for (int i = 0; i < count; ++i) {
        series.add_sample(core::Sample(start_time + i * 1000, static_cast<double>(i)));
    }
    ASSERT_TRUE(storage_->write(series).ok());

    // 3. Force flush and close
    storage_->close();
    storage_.reset();

    // Verify block file exists
    bool block_found = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(test_dir_)) {
        if (entry.path().extension() == ".block") {
            block_found = true;
            break;
        }
    }
    ASSERT_TRUE(block_found) << "No .block file found after close()";

    // DELETE WAL TO FORCE BLOCK LOADING
    std::filesystem::remove_all(test_dir_ + "/wal");

    // 4. Re-initialize Storage
    storage_ = std::make_unique<storage::StorageImpl>();
    ASSERT_TRUE(storage_->init(config_).ok());

    // 5. Query data
    auto result = storage_->read(labels, start_time, start_time + count * 1000);
    ASSERT_TRUE(result.ok()) << "Read failed: " << result.error();
    
    const auto& read_series = result.value();
    ASSERT_EQ(read_series.samples().size(), count) << "Data loss detected after WAL loss!";
}

TEST_F(StoragePersistenceTest, QueryWorksAfterRestart) {
    // 1. Initialize Storage
    storage_ = std::make_unique<storage::StorageImpl>();
    ASSERT_TRUE(storage_->init(config_).ok());

    // 2. Write data
    int64_t start_time = 1000;
    int64_t count = 100;
    core::Labels labels({{"metric", "test_query_restart"}, {"host", "server1"}});
    core::TimeSeries series(labels);
    
    for (int i = 0; i < count; ++i) {
        series.add_sample(core::Sample(start_time + i * 1000, static_cast<double>(i)));
    }
    ASSERT_TRUE(storage_->write(series).ok());

    // 3. Force flush and close
    storage_->close();
    storage_.reset();

    // 4. Re-initialize Storage
    storage_ = std::make_unique<storage::StorageImpl>();
    ASSERT_TRUE(storage_->init(config_).ok());

    // 5. Query data using LabelMatcher (exercises query path)
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "metric", "test_query_restart");
    
    auto result = storage_->query(matchers, start_time, start_time + count * 1000);
    ASSERT_TRUE(result.ok()) << "Query failed: " << result.error();
    
    const auto& series_list = result.value();
    ASSERT_EQ(series_list.size(), 1) << "Expected 1 series, found " << series_list.size();
    ASSERT_EQ(series_list[0].samples().size(), count) << "Data loss detected in query!";
}

} // namespace integration
} // namespace tsdb
