#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <filesystem>
#include "tsdb/storage/sharded_wal.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

class ShardedWALTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "test_data/sharded_wal_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(ShardedWALTest, BasicLogAndReplay) {
    {
        ShardedWAL wal(test_dir_, 4);
        
        core::Labels labels;
        labels.add("metric", "cpu");
        labels.add("host", "server1");
        core::TimeSeries series(labels);
        series.add_sample(1000, 1.0);
        
        ASSERT_TRUE(wal.log(series).ok());
    }
    
    {
        ShardedWAL wal(test_dir_, 4);
        int count = 0;
        auto result = wal.replay([&](const core::TimeSeries& series) {
            count++;
            EXPECT_EQ(series.samples().size(), 1);
            EXPECT_EQ(series.samples()[0].timestamp(), 1000);
            EXPECT_DOUBLE_EQ(series.samples()[0].value(), 1.0);
        });
        ASSERT_TRUE(result.ok());
        EXPECT_EQ(count, 1);
    }
}

TEST_F(ShardedWALTest, ShardingDistribution) {
    ShardedWAL wal(test_dir_, 4);
    
    // Create series that should hash to different shards
    // This is probabilistic but with enough series we should see distribution
    int num_series = 100;
    for (int i = 0; i < num_series; ++i) {
        core::Labels labels;
        labels.add("metric", "cpu");
        labels.add("id", std::to_string(i));
        core::TimeSeries series(labels);
        ASSERT_TRUE(wal.log(series).ok());
    }
    
    // Check that multiple shards were created
    int shards_found = 0;
    for (const auto& entry : std::filesystem::directory_iterator(test_dir_)) {
        if (entry.is_directory() && entry.path().filename().string().rfind("shard_", 0) == 0) {
            shards_found++;
        }
    }
    EXPECT_GT(shards_found, 1);
}

TEST_F(ShardedWALTest, ConcurrentWrites) {
    int num_threads = 8;
    int writes_per_thread = 100;
    std::atomic<int> success_count{0};
    
    {
        ShardedWAL wal(test_dir_, 8);
        
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < writes_per_thread; ++j) {
                    core::Labels labels;
                    labels.add("metric", "cpu");
                    labels.add("thread", std::to_string(i));
                    labels.add("iter", std::to_string(j));
                    core::TimeSeries series(labels);
                    series.add_sample(1000 + j, 1.0 * j);
                    
                    if (wal.log(series).ok()) {
                        success_count++;
                    }
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    } // Destructor flushes
    
    EXPECT_EQ(success_count, num_threads * writes_per_thread);
    
    // Verify replay
    {
        ShardedWAL wal(test_dir_, 8);
        int replay_count = 0;
        wal.replay([&](const core::TimeSeries& series) {
            replay_count++;
        });
        EXPECT_EQ(replay_count, num_threads * writes_per_thread);
    }
}

} // namespace storage
} // namespace tsdb
