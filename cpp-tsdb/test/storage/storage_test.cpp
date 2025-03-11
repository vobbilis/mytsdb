#include <gtest/gtest.h>
#include <filesystem>
#include <thread>
#include <vector>
#include "tsdb/storage/storage.hpp"
#include "../test_utils.hpp"

namespace tsdb::test {

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for tests
        test_path_ = std::filesystem::temp_directory_path() / "tsdb_test";
        std::filesystem::create_directories(test_path_);
        
        // Initialize storage
        StorageConfig config{
            .data_path = test_path_.string(),
            .max_block_size = 1024 * 1024
        };
        storage_ = std::make_unique<Storage>(config);
    }

    void TearDown() override {
        storage_.reset();
        std::filesystem::remove_all(test_path_);
    }

    std::filesystem::path test_path_;
    std::unique_ptr<Storage> storage_;
};

TEST_F(StorageTest, BasicOperations) {
    // Test data
    auto series = TestUtils::generateTestSeries({
        {"__name__", "test_metric"},
        {"instance", "test-1"}
    }, 100);

    // Test Write
    EXPECT_NO_THROW(storage_->writeSeries(series));

    // Test Read
    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours(1);
    auto result = storage_->readSeries(
        series.getLabels(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()
    );

    ASSERT_TRUE(result);
    EXPECT_EQ(result->getSamples().size(), series.getSamples().size());

    // Test Query
    LabelMatcher matcher{
        .type = MatchType::Equal,
        .name = "__name__",
        .value = "test_metric"
    };

    auto results = storage_->query(
        {matcher},
        std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()
    );

    EXPECT_FALSE(results.empty());
}

TEST_F(StorageTest, Concurrency) {
    // Generate test data
    auto testData = TestUtils::generateTestData(10, 1000);
    
    // Test concurrent writes
    {
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (const auto& series : testData) {
            threads.emplace_back([this, &series, &errors]() {
                try {
                    storage_->writeSeries(series);
                } catch (...) {
                    errors++;
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(errors, 0);
    }

    // Test concurrent reads
    {
        auto now = std::chrono::system_clock::now();
        auto start = now - std::chrono::hours(1);
        auto startNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            start.time_since_epoch()
        ).count();
        auto endNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        ).count();

        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (const auto& series : testData) {
            threads.emplace_back([this, &series, startNs, endNs, &errors]() {
                try {
                    auto result = storage_->readSeries(series.getLabels(), startNs, endNs);
                    if (!result) {
                        errors++;
                    }
                } catch (...) {
                    errors++;
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(errors, 0);
    }
}

TEST_F(StorageTest, Compaction) {
    // Configure storage with small block size to trigger compaction
    StorageConfig config{
        .data_path = test_path_.string(),
        .max_block_size = 1024  // Small block size
    };
    storage_ = std::make_unique<Storage>(config);

    // Write enough data to trigger compaction
    auto series = TestUtils::generateTestSeries({
        {"__name__", "test_metric"},
        {"instance", "test-1"}
    }, 10000);

    EXPECT_NO_THROW(storage_->writeSeries(series));

    // Wait for compaction
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify data is still readable
    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours(1);
    auto result = storage_->readSeries(
        series.getLabels(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()
    );

    ASSERT_TRUE(result);
    EXPECT_EQ(result->getSamples().size(), series.getSamples().size());
}

TEST_F(StorageTest, Recovery) {
    // Write initial data
    auto series = TestUtils::generateTestSeries({
        {"__name__", "test_metric"},
        {"instance", "test-1"}
    }, 100);

    EXPECT_NO_THROW(storage_->writeSeries(series));

    // Close storage
    storage_.reset();

    // Reopen storage
    StorageConfig config{
        .data_path = test_path_.string(),
        .max_block_size = 1024 * 1024
    };
    storage_ = std::make_unique<Storage>(config);

    // Verify data survived
    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours(1);
    auto result = storage_->readSeries(
        series.getLabels(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()
    );

    ASSERT_TRUE(result);
    EXPECT_EQ(result->getSamples().size(), series.getSamples().size());
}

} // namespace tsdb::test 