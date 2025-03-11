#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <thread>
#include "tsdb/alert/manager.hpp"
#include "tsdb/promql/engine.hpp"
#include "tsdb/storage/storage.hpp"
#include "../test_utils.hpp"

namespace tsdb::test {

class AlertManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for tests
        test_path_ = std::filesystem::temp_directory_path() / "tsdb_alert_test";
        std::filesystem::create_directories(test_path_);
        
        // Initialize components
        StorageConfig config{
            .data_path = test_path_.string(),
            .max_block_size = 1024 * 1024
        };
        storage_ = std::make_unique<Storage>(config);
        engine_ = std::make_unique<PromQLEngine>(storage_.get());
        manager_ = std::make_unique<AlertManager>(engine_.get());

        // Setup test data
        setupTestData();
    }

    void TearDown() override {
        manager_.reset();
        engine_.reset();
        storage_.reset();
        std::filesystem::remove_all(test_path_);
    }

    void setupTestData() {
        auto series = TestUtils::generateTestSeries({
            {"__name__", "error_rate"},
            {"service", "api"}
        }, 100);
        storage_->writeSeries(series);
    }

    std::filesystem::path test_path_;
    std::unique_ptr<Storage> storage_;
    std::unique_ptr<PromQLEngine> engine_;
    std::unique_ptr<AlertManager> manager_;
};

TEST_F(AlertManagerTest, BasicRules) {
    // Test simple threshold rule
    {
        Rule rule{
            .name = "high_error_rate",
            .query = "error_rate > 0.5",
            .duration = std::chrono::minutes(5),
            .labels = {
                {"severity", "critical"}
            },
            .annotations = {
                {"summary", "High error rate detected"}
            }
        };

        EXPECT_NO_THROW(manager_->addRule(rule));

        // Run evaluation
        auto alerts = manager_->evaluate();
        ASSERT_TRUE(alerts);
        EXPECT_FALSE(alerts->empty());
    }

    // Test rate rule
    {
        Rule rule{
            .name = "error_spike",
            .query = "rate(error_rate[5m]) > 0.1",
            .duration = std::chrono::minutes(5),
            .labels = {
                {"severity", "warning"}
            },
            .annotations = {
                {"summary", "Error rate is increasing rapidly"}
            }
        };

        EXPECT_NO_THROW(manager_->addRule(rule));

        // Run evaluation
        auto alerts = manager_->evaluate();
        ASSERT_TRUE(alerts);
        EXPECT_FALSE(alerts->empty());
    }
}

TEST_F(AlertManagerTest, StatePersistence) {
    Rule rule{
        .name = "test_alert",
        .query = "error_rate > 0.5",
        .duration = std::chrono::minutes(5),
        .labels = {
            {"severity", "warning"}
        }
    };

    EXPECT_NO_THROW(manager_->addRule(rule));

    // Test alert state transitions
    auto alerts1 = manager_->evaluate();
    ASSERT_TRUE(alerts1);
    
    auto alerts2 = manager_->evaluate();
    ASSERT_TRUE(alerts2);

    // Check if state is maintained
    EXPECT_EQ(alerts1->size(), alerts2->size());
    for (size_t i = 0; i < alerts1->size(); ++i) {
        EXPECT_EQ((*alerts1)[i].state, (*alerts2)[i].state);
    }
}

TEST_F(AlertManagerTest, Notifications) {
    // Create notification channel
    auto notifications = std::make_shared<AlertChannel>();
    manager_ = std::make_unique<AlertManager>(engine_.get(), notifications);

    Rule rule{
        .name = "test_notification",
        .query = "error_rate > 0.5",
        .duration = std::chrono::seconds(1),
        .labels = {
            {"severity", "critical"}
        }
    };

    EXPECT_NO_THROW(manager_->addRule(rule));

    // Run evaluation
    auto alerts = manager_->evaluate();
    ASSERT_TRUE(alerts);

    // Check for notification
    Alert alert;
    EXPECT_TRUE(notifications->try_dequeue(alert));
    EXPECT_EQ(alert.name, "test_notification");
}

TEST_F(AlertManagerTest, Concurrency) {
    // Add multiple rules
    std::vector<Rule> rules = {
        {
            .name = "rule1",
            .query = "metric1 > 0.5",
            .duration = std::chrono::minutes(5)
        },
        {
            .name = "rule2",
            .query = "metric2 > 0.5",
            .duration = std::chrono::minutes(5)
        },
        {
            .name = "rule3",
            .query = "metric3 > 0.5",
            .duration = std::chrono::minutes(5)
        }
    };

    for (const auto& rule : rules) {
        EXPECT_NO_THROW(manager_->addRule(rule));
    }

    // Test concurrent evaluations
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &errors]() {
            try {
                auto alerts = manager_->evaluate();
                if (!alerts) {
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