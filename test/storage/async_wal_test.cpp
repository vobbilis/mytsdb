#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tsdb/storage/async_wal_shard.h"
#include <filesystem>
#include <thread>
#include <chrono>

namespace tsdb {
namespace storage {

class AsyncWALShardTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "async_wal_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
};

TEST_F(AsyncWALShardTest, BasicLogAndReplay) {
    std::string shard_dir = (test_dir_ / "shard_0").string();
    
    // 1. Write data
    {
        AsyncWALShard shard(shard_dir);
        
        core::Labels labels;
        labels.add("metric", "test");
        core::TimeSeries series(labels);
        series.add_sample(1000, 1.0);
        
        auto result = shard.log(series);
        ASSERT_TRUE(result.ok());
        
        // Wait a bit for background thread to flush
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } // Destructor should ensure flush
    
    // 2. Replay
    {
        AsyncWALShard shard(shard_dir);
        int count = 0;
        auto result = shard.replay([&](const core::TimeSeries& s) {
            count++;
            EXPECT_EQ(s.labels().get("metric"), "test");
            ASSERT_EQ(s.samples().size(), 1);
            EXPECT_EQ(s.samples()[0].timestamp(), 1000);
            EXPECT_EQ(s.samples()[0].value(), 1.0);
        });
        ASSERT_TRUE(result.ok());
        EXPECT_EQ(count, 1);
    }
}

TEST_F(AsyncWALShardTest, BatchingBehavior) {
    std::string shard_dir = (test_dir_ / "shard_1").string();
    const int NUM_SERIES = 5000;
    
    // 1. Write many series
    {
        AsyncWALShard shard(shard_dir);
        
        for (int i = 0; i < NUM_SERIES; ++i) {
            core::Labels labels;
            labels.add("metric", "test_" + std::to_string(i));
            core::TimeSeries series(labels);
            series.add_sample(1000 + i, 1.0 * i);
            
            auto result = shard.log(series);
            ASSERT_TRUE(result.ok());
        }
    } // Destructor ensures all 5000 are flushed
    
    // 2. Replay
    {
        AsyncWALShard shard(shard_dir);
        int count = 0;
        auto result = shard.replay([&](const core::TimeSeries& s) {
            count++;
        });
        ASSERT_TRUE(result.ok());
        EXPECT_EQ(count, NUM_SERIES);
    }
}

TEST_F(AsyncWALShardTest, ConcurrentWrites) {
    std::string shard_dir = (test_dir_ / "shard_2").string();
    const int NUM_THREADS = 4;
    const int SERIES_PER_THREAD = 1000;
    
    // 1. Concurrent writes
    {
        AsyncWALShard shard(shard_dir);
        std::vector<std::thread> threads;
        
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < SERIES_PER_THREAD; ++i) {
                    core::Labels labels;
                    labels.add("thread", std::to_string(t));
                    labels.add("id", std::to_string(i));
                    core::TimeSeries series(labels);
                    series.add_sample(1000 + i, 1.0 * i);
                    
                    shard.log(series);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }
    
    // 2. Replay
    {
        AsyncWALShard shard(shard_dir);
        std::atomic<int> count{0};
        auto result = shard.replay([&](const core::TimeSeries& s) {
            count++;
        });
        ASSERT_TRUE(result.ok());
        EXPECT_EQ(count, NUM_THREADS * SERIES_PER_THREAD);
    }
}

} // namespace storage
} // namespace tsdb
