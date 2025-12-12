// Integration test: StorageImpl query semantics for negative matchers (!= and !~).
//
// This specifically exercises the StorageImpl -> ShardedIndex -> Index matcher path
// end-to-end, to catch any correctness regressions when we optimize negative matchers
// in the primary index.

#include <gtest/gtest.h>
#include "tsdb/core/config.h"
#include "tsdb/core/matcher.h"
#include "tsdb/core/types.h"
#include "tsdb/storage/storage_impl.h"

#include <filesystem>
#include <memory>
#include "../test_util/temp_dir.h"

namespace tsdb {
namespace integration {
namespace {

class NegativeMatchersIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = tsdb::testutil::MakeUniqueTestDir("tsdb_negative_matchers_test");
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);

        core::StorageConfig config;
        config.data_dir = test_dir_.string();
        // Keep background off for determinism.
        config.background_config.enable_background_processing = false;

        storage_ = std::make_shared<storage::StorageImpl>();
        ASSERT_TRUE(storage_->init(config).ok());
    }

    void TearDown() override {
        if (storage_) {
            storage_->close();
            storage_.reset();
        }
        std::filesystem::remove_all(test_dir_);
    }

    void WriteSeries(core::SeriesID id,
                     const std::string& name,
                     const std::map<std::string, std::string>& labels) {
        core::Labels l;
        l.add("__name__", name);
        for (const auto& [k, v] : labels) {
            l.add(k, v);
        }
        core::TimeSeries ts(l);
        ts.add_sample(core::Sample(1000, static_cast<double>(id)));
        ASSERT_TRUE(storage_->write(ts).ok());
    }

    std::filesystem::path test_dir_;
    std::shared_ptr<storage::StorageImpl> storage_;
};

TEST_F(NegativeMatchersIntegrationTest, NotEqualKeepsAbsentLabelAndFiltersEqualValue) {
    // Series: env=prod, env=dev, env absent
    WriteSeries(1, "up", {{"env", "prod"}});
    WriteSeries(2, "up", {{"env", "dev"}});
    WriteSeries(3, "up", {});  // env absent

    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::NotEqual, "env", "prod");

    auto res = storage_->query(matchers, 0, 2000);
    ASSERT_TRUE(res.ok());
    // Expect 2 series: env=dev and env absent
    ASSERT_EQ(res.value().size(), 2);
}

TEST_F(NegativeMatchersIntegrationTest, NotEqualEmptyStringExcludesAbsentLabel) {
    // Series: env=prod, env empty, env absent
    WriteSeries(1, "up", {{"env", "prod"}});
    WriteSeries(2, "up", {{"env", ""}});
    WriteSeries(3, "up", {});  // env absent

    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::NotEqual, "env", "");

    auto res = storage_->query(matchers, 0, 2000);
    ASSERT_TRUE(res.ok());
    // Only env=prod should match.
    ASSERT_EQ(res.value().size(), 1);
    EXPECT_EQ(res.value()[0].labels().get("env").value_or(""), "prod");
}

TEST_F(NegativeMatchersIntegrationTest, RegexNoMatchWhereRegexMatchesEmptyExcludesAbsentLabel) {
    // Regex ".*" matches empty, so env!~".*" should match nothing (absent treated as "").
    WriteSeries(1, "up", {{"env", "prod"}});
    WriteSeries(2, "up", {});  // env absent

    std::vector<core::LabelMatcher> matchers;
    matchers.emplace_back(core::MatcherType::Equal, "__name__", "up");
    matchers.emplace_back(core::MatcherType::RegexNoMatch, "env", ".*");

    auto res = storage_->query(matchers, 0, 2000);
    ASSERT_TRUE(res.ok());
    EXPECT_TRUE(res.value().empty());
}

} // namespace
} // namespace integration
} // namespace tsdb


