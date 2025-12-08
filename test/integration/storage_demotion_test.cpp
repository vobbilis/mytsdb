#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include <filesystem>
#include <chrono>
#include <thread>

namespace tsdb {
namespace integration {

class StorageDemotionTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/tsdb_demotion_test_" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        config_.data_dir = test_dir_;
        config_.block_size = 1024;
        config_.enable_compression = true;
        // Enable background processing for demotion
        config_.background_config.enable_background_processing = true;
        // config_.background_config.task_interval = std::chrono::milliseconds(100); // Fast flush for testing
    }

    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    core::StorageConfig config_;
    std::unique_ptr<storage::StorageImpl> storage_;
};

TEST_F(StorageDemotionTest, CompactionWorksAfterDemotion) {
    // 1. Initialize Storage
    storage_ = std::make_unique<storage::StorageImpl>();
    ASSERT_TRUE(storage_->init(config_).ok());

    core::Labels labels({{"metric", "test_demotion"}, {"host", "server1"}});
    core::TimeSeries series(labels);
    int64_t start_time = 1000;

    // 2. Create multiple Parquet blocks to satisfy compaction threshold (MIN_FILES_TO_COMPACT = 5)
    // We write 6 blocks to be safe
    for (int b = 0; b < 6; ++b) {
        series.samples().clear(); 
        // Write enough samples to fill a block (limit is 10,000)
        for (int i = 0; i < 10050; ++i) {
            // Use monotonic timestamps to ensure clean ordering
            int64_t sample_time = start_time + (b * 10050 + i) * 1000;
            series.add_sample(core::Sample(sample_time, static_cast<double>(b * 10050 + i)));
        }
        ASSERT_TRUE(storage_->write(series).ok());
        
        // Wait for block seal timestamp to be strictly less than flush time
        // Use a robust sleep to ensure clock tick
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Force flush (demotion) of this sealed block immediately
        // This ensures each block becomes a separate Parquet file
        auto result = storage_->execute_background_flush(0);
        ASSERT_TRUE(result.ok());
    }
    
    // Wait for block seal timestamps to be strictly less than flush time
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Force flush (demotion) of all sealed blocks
    // auto result = storage_->execute_background_flush(0);
    // ASSERT_TRUE(result.ok());

    // 4. Verify we have at least 6 Parquet files
    int parquet_count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(test_dir_)) {
        if (entry.path().extension() == ".parquet") {
            parquet_count++;
        }
    }
    ASSERT_GE(parquet_count, 6) << "Expected at least 6 Parquet files";

    // 5. Trigger Compaction
    // This relies on block_to_series_ being updated during demotion
    auto result = storage_->execute_background_compaction();
    ASSERT_TRUE(result.ok()) << "Compaction failed: " << result.error();

    // 6. Verify Compaction Result
    // Check if the "2/" directory exists (compaction output)
    bool compaction_dir_found = std::filesystem::exists(test_dir_ + "/2");
    ASSERT_TRUE(compaction_dir_found) << "Compaction output directory '2/' not found. Compaction likely didn't run due to missing metadata.";
    
    // Check if there are files in it
    int compacted_files = 0;
    if (compaction_dir_found) {
        for (const auto& entry : std::filesystem::directory_iterator(test_dir_ + "/2")) {
            if (entry.path().extension() == ".parquet") {
                compacted_files++;
            }
        }
    }
    ASSERT_GT(compacted_files, 0) << "No compacted files found.";
}

} // namespace integration
} // namespace tsdb
