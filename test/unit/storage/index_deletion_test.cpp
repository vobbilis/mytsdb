#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/types.h"
#include <filesystem>

namespace tsdb {
namespace storage {

class IndexDeletionTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage_ = std::make_unique<StorageImpl>();
        core::StorageConfig config;
        config.data_dir = "/tmp/tsdb_index_deletion_test";
        // Ensure clean state
        std::filesystem::remove_all(config.data_dir);
        ASSERT_TRUE(storage_->init(config).ok());
    }

    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        std::filesystem::remove_all("/tmp/tsdb_index_deletion_test");
    }

    std::unique_ptr<StorageImpl> storage_;
};

TEST_F(IndexDeletionTest, TestDeleteSeriesRemovesFromIndex) {
    // 1. Add a series
    core::Labels labels;
    labels.add("__name__", "test_metric");
    labels.add("job", "test_job");
    labels.add("instance", "localhost:9090");
    
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 1.0));
    
    ASSERT_TRUE(storage_->write(series).ok());
    
    // 2. Verify it exists in index
    {
        std::vector<core::LabelMatcher> matchers;
        matchers.push_back(core::LabelMatcher(core::MatcherType::Equal, "job", "test_job"));
        
        auto result = storage_->query(matchers, 0, 2000);
        ASSERT_TRUE(result.ok());
        ASSERT_EQ(result.value().size(), 1);
    }
    
    // 3. Delete the series
    {
        std::vector<core::LabelMatcher> matchers;
        matchers.push_back(core::LabelMatcher(core::MatcherType::Equal, "job", "test_job"));
        
        ASSERT_TRUE(storage_->delete_series(matchers).ok());
    }
    
    // 4. Verify it is gone from index
    {
        std::vector<core::LabelMatcher> matchers;
        matchers.push_back(core::LabelMatcher(core::MatcherType::Equal, "job", "test_job"));
        
        auto result = storage_->query(matchers, 0, 2000);
        ASSERT_TRUE(result.ok());
        EXPECT_EQ(result.value().size(), 0) << "Series should be removed from index";
    }
}

} // namespace storage
} // namespace tsdb
