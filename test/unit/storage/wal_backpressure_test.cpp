#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <filesystem>
#include "tsdb/storage/async_wal_shard.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

class WALBackpressureTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "test_wal_backpressure_" + std::to_string(std::time(nullptr));
        std::filesystem::create_directory(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(WALBackpressureTest, TestUnboundedQueueGrowth) {
    // Set a small max queue size
    size_t max_queue_size = 100;
    AsyncWALShard wal(test_dir_, max_queue_size);

    // Slow down the worker significantly
    wal.Test_SetWorkerDelay(std::chrono::milliseconds(10));

    // Push more items than the queue limit
    size_t num_items = 200;
    for (size_t i = 0; i < num_items; ++i) {
        core::Labels labels;
        labels.add("metric", "test");
        core::TimeSeries series(labels);
        series.add_sample(1000 + i, 1.0);
        wal.log(series);
    }

    // Check queue size immediately
    size_t queue_size = wal.GetQueueSize();
    
    // With backpressure, the queue should NOT grow beyond the limit.
    // The producer should have been blocked until space was available.
    EXPECT_LE(queue_size, max_queue_size) << "Queue size should be bounded by max_queue_size";
    
    // Allow worker to finish processing
    wal.flush();
    EXPECT_EQ(wal.GetQueueSize(), 0);
}

} // namespace storage
} // namespace tsdb
