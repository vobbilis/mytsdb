#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include <iostream>

namespace tsdb {
namespace integration {

class DebugStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        core::StorageConfig config;
        config.data_dir = "./test/data/storageimpl_phases/debug";
        config.object_pool_config.time_series_initial_size = 10;
        config.object_pool_config.time_series_max_size = 100;
        config.object_pool_config.labels_initial_size = 20;
        config.object_pool_config.labels_max_size = 200;
        config.object_pool_config.samples_initial_size = 50;
        config.object_pool_config.samples_max_size = 500;
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
    }
    
    std::unique_ptr<storage::StorageImpl> storage_;
};

TEST_F(DebugStatsTest, PrintStatsOutput) {
    std::cout << "=== INITIAL STATS ===" << std::endl;
    std::string initial_stats = storage_->stats();
    std::cout << initial_stats << std::endl;
    
    // Write some data
    core::Labels labels;
    labels.add("__name__", "test_metric");
    labels.add("test", "debug");
    
    core::TimeSeries series(labels);
    for (int i = 0; i < 10; ++i) {
        series.add_sample(1000 + i, 100.0 + i);
    }
    
    auto write_result = storage_->write(series);
    ASSERT_TRUE(write_result.ok()) << "Write failed: " << write_result.error();
    
    std::cout << "\n=== AFTER WRITE STATS ===" << std::endl;
    std::string after_write_stats = storage_->stats();
    std::cout << after_write_stats << std::endl;
    
    // Read some data
    auto read_result = storage_->read(labels, 1000, 1010);
    ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
    
    std::cout << "\n=== AFTER READ STATS ===" << std::endl;
    std::string after_read_stats = storage_->stats();
    std::cout << after_read_stats << std::endl;
}

} // namespace integration
} // namespace tsdb 