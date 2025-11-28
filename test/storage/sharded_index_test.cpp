#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "tsdb/storage/sharded_index.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

class ShardedIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(ShardedIndexTest, BasicAddAndFind) {
    ShardedIndex index(4);
    
    core::Labels labels;
    labels.add("metric", "cpu");
    labels.add("host", "server1");
    core::SeriesID id = 100;
    
    ASSERT_TRUE(index.add_series(id, labels).ok());
    
#include "tsdb/core/matcher.h"
// ...

    // Find by exact match
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "metric", "cpu");
    matchers.emplace_back(core::MatcherType::Equal, "host", "server1");
    
    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0], id);
    
    // Get labels
    auto labels_result = index.get_labels(id);
    ASSERT_TRUE(labels_result.ok());
    EXPECT_EQ(labels_result.value().get("metric"), "cpu");
}

TEST_F(ShardedIndexTest, ShardingDistribution) {
    ShardedIndex index(4);
    
    // Add series with sequential IDs
    // Since we shard by ID % num_shards, this should distribute perfectly
    int num_series = 100;
    for (int i = 0; i < num_series; ++i) {
        core::Labels labels;
        labels.add("metric", "cpu");
        labels.add("id", std::to_string(i));
        ASSERT_TRUE(index.add_series(i, labels).ok());
    }
    
    // Verify we can find them all
    for (int i = 0; i < num_series; ++i) {
        auto result = index.get_labels(i);
        ASSERT_TRUE(result.ok());
        EXPECT_EQ(result.value().get("id"), std::to_string(i));
    }
}

TEST_F(ShardedIndexTest, ConcurrentAccess) {
    ShardedIndex index(8);
    
    int num_threads = 8;
    int ops_per_thread = 1000;
    std::atomic<int> success_count{0};
    
    // Concurrent writers
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                core::Labels labels;
                labels.add("metric", "cpu");
                labels.add("thread", std::to_string(i));
                labels.add("iter", std::to_string(j));
                core::SeriesID id = i * ops_per_thread + j;
                
                if (index.add_series(id, labels).ok()) {
                    success_count++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(success_count, num_threads * ops_per_thread);
    
    // Concurrent readers
    threads.clear();
    std::atomic<int> found_count{0};
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                core::SeriesID id = i * ops_per_thread + j;
                if (index.get_labels(id).ok()) {
                    found_count++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(found_count, num_threads * ops_per_thread);
}

} // namespace storage
} // namespace tsdb
