#include <gtest/gtest.h>
#include <algorithm>
#include <thread>
#include <vector>
#include "tsdb/storage/sharded_index.h"
#include "tsdb/core/types.h"
#include "tsdb/core/matcher.h"

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

TEST_F(ShardedIndexTest, NameRoutingQueriesOnlyRelevantShards) {
    ShardedIndex index(4);

    // Distribute two metrics across disjoint shard sets by choosing IDs.
    // shard = id % 4
    // foo -> shards 0,1
    // bar -> shards 2,3
    {
        core::Labels labels;
        labels.add("__name__", "foo");
        labels.add("job", "a");
        ASSERT_TRUE(index.add_series(0, labels).ok());
    }
    {
        core::Labels labels;
        labels.add("__name__", "foo");
        labels.add("job", "b");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }
    {
        core::Labels labels;
        labels.add("__name__", "bar");
        labels.add("job", "c");
        ASSERT_TRUE(index.add_series(2, labels).ok());
    }
    {
        core::Labels labels;
        labels.add("__name__", "bar");
        labels.add("job", "d");
        ASSERT_TRUE(index.add_series(3, labels).ok());
    }

    index.reset_shard_query_counts();

    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "foo");
    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());

    // Should return IDs 0 and 1 (order not guaranteed).
    auto ids = result.value();
    ASSERT_EQ(ids.size(), 2);
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids[0], 0);
    EXPECT_EQ(ids[1], 1);

    // Routing should only query shards 0 and 1.
    auto counts = index.get_shard_query_counts();
    ASSERT_EQ(counts.size(), 4u);
    EXPECT_GT(counts[0], 0u);
    EXPECT_GT(counts[1], 0u);
    EXPECT_EQ(counts[2], 0u);
    EXPECT_EQ(counts[3], 0u);

    // Without __name__ equality, we should scatter-gather all shards.
    index.reset_shard_query_counts();
    std::vector<core::LabelMatcher> no_name;
    no_name.emplace_back(core::MatcherType::Equal, "job", "a");
    auto result2 = index.find_series(no_name);
    ASSERT_TRUE(result2.ok());
    auto counts2 = index.get_shard_query_counts();
    ASSERT_EQ(counts2.size(), 4u);
    EXPECT_GT(counts2[0], 0u);
    EXPECT_GT(counts2[1], 0u);
    EXPECT_GT(counts2[2], 0u);
    EXPECT_GT(counts2[3], 0u);
}

TEST_F(ShardedIndexTest, RegexMatchFindsExpectedSeries) {
    ShardedIndex index(4);

    // Series 1
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("job", "api");
        labels.add("instance", "server1");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }

    // Series 2
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("job", "db");
        labels.add("instance", "server2");
        ASSERT_TRUE(index.add_series(2, labels).ok());
    }

    // Regex match "a.*" should match "api" only
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");  // common PromQL pattern
    matchers.emplace_back(core::MatcherType::RegexMatch, "job", "a.*");

    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0], 1);
}

TEST_F(ShardedIndexTest, InvalidRegexMatchExcludesAllSeries) {
    ShardedIndex index(4);

    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("job", "api");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }

    // RegexMatch with invalid regex should behave like the current Index implementation:
    // treat as non-match (exclude series).
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::RegexMatch, "job", "("); // invalid

    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(ShardedIndexTest, InvalidRegexNoMatchIsIgnored) {
    ShardedIndex index(4);

    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("job", "api");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }

    // RegexNoMatch with invalid regex is ignored by Index (does not exclude series).
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::RegexNoMatch, "job", "("); // invalid

    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0], 1);
}

TEST_F(ShardedIndexTest, RegexNoMatchFiltersMatchingSeries) {
    ShardedIndex index(4);

    // Series 1: job=api
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("job", "api");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }

    // Series 2: job=db
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("job", "db");
        ASSERT_TRUE(index.add_series(2, labels).ok());
    }

    // job!~"a.*" should exclude "api" and keep "db"
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::RegexNoMatch, "job", "a.*");

    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0], 2);
}

TEST_F(ShardedIndexTest, MultipleRegexMatchersWorkTogether) {
    ShardedIndex index(4);

    // Series 1: job=api, instance=server1
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("job", "api");
        labels.add("instance", "server1");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }

    // Series 2: job=api, instance=db01
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("job", "api");
        labels.add("instance", "db01");
        ASSERT_TRUE(index.add_series(2, labels).ok());
    }

    // Need BOTH: job=~"a.*" and instance=~"server.*" -> should match only series 1
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::RegexMatch, "job", "a.*");
    matchers.emplace_back(core::MatcherType::RegexMatch, "instance", "server.*");

    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0], 1);
}

TEST_F(ShardedIndexTest, NotEqualExcludesMatchingValueButKeepsAbsentLabel) {
    ShardedIndex index(4);

    // Series 1: has env=prod
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("env", "prod");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }

    // Series 2: has env=dev
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("env", "dev");
        ASSERT_TRUE(index.add_series(2, labels).ok());
    }

    // Series 3: env label is absent (treated as "")
    {
        core::Labels labels;
        labels.add("__name__", "up");
        ASSERT_TRUE(index.add_series(3, labels).ok());
    }

    // env!="prod" should keep dev and absent
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::NotEqual, "env", "prod");

    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.value().size(), 2);
    // Order is not guaranteed; assert set membership.
    EXPECT_TRUE((result.value()[0] == 2 || result.value()[0] == 3));
    EXPECT_TRUE((result.value()[1] == 2 || result.value()[1] == 3));
    EXPECT_NE(result.value()[0], result.value()[1]);
}

TEST_F(ShardedIndexTest, NotEqualEmptyStringExcludesAbsentLabelAndEmptyValue) {
    ShardedIndex index(4);

    // Series 1: env="prod"
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("env", "prod");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }

    // Series 2: env="" (explicit empty value)
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("env", "");
        ASSERT_TRUE(index.add_series(2, labels).ok());
    }

    // Series 3: env label absent (treated as "")
    {
        core::Labels labels;
        labels.add("__name__", "up");
        ASSERT_TRUE(index.add_series(3, labels).ok());
    }

    // env!="" should match only env="prod"
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::NotEqual, "env", "");

    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0], 1);
}

TEST_F(ShardedIndexTest, RegexNoMatchWhereRegexMatchesEmptyExcludesAbsentLabel) {
    ShardedIndex index(4);

    // Series 1: env label absent (treated as "")
    {
        core::Labels labels;
        labels.add("__name__", "up");
        ASSERT_TRUE(index.add_series(1, labels).ok());
    }

    // Series 2: env="prod"
    {
        core::Labels labels;
        labels.add("__name__", "up");
        labels.add("env", "prod");
        ASSERT_TRUE(index.add_series(2, labels).ok());
    }

    // Regex ".*" matches the empty string, so env!~".*" should exclude env absent and env present.
    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::RegexNoMatch, "env", ".*");

    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.value().empty());
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
