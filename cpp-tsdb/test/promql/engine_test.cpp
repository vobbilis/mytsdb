#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>
#include "tsdb/promql/engine.hpp"
#include "tsdb/storage/storage.hpp"
#include "../test_utils.hpp"

namespace tsdb::test {

class PromQLTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for tests
        test_path_ = std::filesystem::temp_directory_path() / "tsdb_promql_test";
        std::filesystem::create_directories(test_path_);
        
        // Initialize storage
        StorageConfig config{
            .data_path = test_path_.string(),
            .max_block_size = 1024 * 1024
        };
        storage_ = std::make_unique<Storage>(config);
        engine_ = std::make_unique<PromQLEngine>(storage_.get());

        // Setup test data
        setupTestData();
    }

    void TearDown() override {
        engine_.reset();
        storage_.reset();
        std::filesystem::remove_all(test_path_);
    }

    void setupTestData() {
        std::vector<Series> series = {
            TestUtils::generateTestSeries({
                {"__name__", "http_requests_total"},
                {"method", "GET"},
                {"status", "200"}
            }, 100),
            TestUtils::generateTestSeries({
                {"__name__", "http_requests_total"},
                {"method", "POST"},
                {"status", "200"}
            }, 100),
            TestUtils::generateTestSeries({
                {"__name__", "http_requests_total"},
                {"method", "GET"},
                {"status", "500"}
            }, 100)
        };

        for (const auto& s : series) {
            storage_->writeSeries(s);
        }
    }

    std::filesystem::path test_path_;
    std::unique_ptr<Storage> storage_;
    std::unique_ptr<PromQLEngine> engine_;
};

TEST_F(PromQLTest, BasicQueries) {
    // Test instant query
    {
        auto result = engine_->instantQuery(
            R"(http_requests_total{method="GET"})",
            std::chrono::system_clock::now()
        );
        ASSERT_TRUE(result);
        EXPECT_EQ(result->series.size(), 2); // GET/200 and GET/500
    }

    // Test range query
    {
        auto now = std::chrono::system_clock::now();
        auto start = now - std::chrono::hours(1);
        auto result = engine_->rangeQuery(
            "rate(http_requests_total[5m])",
            start,
            now,
            std::chrono::minutes(5)
        );
        ASSERT_TRUE(result);
        EXPECT_FALSE(result->series.empty());
    }

    // Test aggregation
    {
        auto result = engine_->instantQuery(
            "sum(http_requests_total) by (method)",
            std::chrono::system_clock::now()
        );
        ASSERT_TRUE(result);
        EXPECT_EQ(result->series.size(), 2); // GET and POST methods
    }

    // Test binary operation
    {
        auto result = engine_->instantQuery(
            R"(http_requests_total{status="200"} / http_requests_total)",
            std::chrono::system_clock::now()
        );
        ASSERT_TRUE(result);
        EXPECT_FALSE(result->series.empty());
    }

    // Test function evaluation
    {
        auto result = engine_->instantQuery(
            "increase(http_requests_total[5m])",
            std::chrono::system_clock::now()
        );
        ASSERT_TRUE(result);
        EXPECT_FALSE(result->series.empty());
    }
}

TEST_F(PromQLTest, ErrorCases) {
    // Test syntax error
    {
        auto result = engine_->instantQuery(
            "invalid{query",
            std::chrono::system_clock::now()
        );
        EXPECT_FALSE(result);
    }

    // Test invalid metric name
    {
        auto result = engine_->instantQuery(
            "non_existent_metric",
            std::chrono::system_clock::now()
        );
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->series.empty());
    }

    // Test invalid function
    {
        auto result = engine_->instantQuery(
            "invalid_function(http_requests_total)",
            std::chrono::system_clock::now()
        );
        EXPECT_FALSE(result);
    }

    // Test invalid time range
    {
        auto now = std::chrono::system_clock::now();
        auto start = now + std::chrono::hours(1); // Start after end
        auto result = engine_->rangeQuery(
            "http_requests_total",
            start,
            now,
            std::chrono::minutes(5)
        );
        EXPECT_FALSE(result);
    }
}

TEST_F(PromQLTest, Concurrency) {
    // Setup test data
    auto series = TestUtils::generateTestSeries({
        {"__name__", "test_metric"},
        {"instance", "test-1"}
    }, 1000);
    storage_->writeSeries(series);

    // Test concurrent queries
    std::vector<std::string> queries = {
        "test_metric",
        "rate(test_metric[5m])",
        "sum(test_metric)",
        "avg(test_metric)"
    };

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (const auto& query : queries) {
        threads.emplace_back([this, &query, &errors]() {
            try {
                auto result = engine_->instantQuery(
                    query,
                    std::chrono::system_clock::now()
                );
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

} // namespace tsdb::test 