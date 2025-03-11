#include <gtest/gtest.h>
#include "tsdb/core/metric.h"
#include "tsdb/core/error.h"
#include "metric_impl.h"
#include <thread>
#include <chrono>

namespace tsdb {
namespace core {
namespace {

class MetricTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory_ = std::make_unique<MetricFactoryImpl>();
    }
    
    std::unique_ptr<MetricFactory> factory_;
};

TEST_F(MetricTest, GaugeBasic) {
    auto gauge = factory_->create_gauge("test_gauge", "Test gauge metric");
    EXPECT_EQ(gauge->type(), MetricType::GAUGE);
    EXPECT_EQ(gauge->name(), "test_gauge");
    EXPECT_EQ(gauge->help(), "Test gauge metric");
    EXPECT_TRUE(gauge->labels().empty());
    EXPECT_EQ(gauge->value(), 0.0);
}

TEST_F(MetricTest, GaugeOperations) {
    auto gauge = factory_->create_gauge("test_gauge", "Test gauge metric");
    
    gauge->set(42.0);
    EXPECT_EQ(gauge->value(), 42.0);
    
    gauge->inc();
    EXPECT_EQ(gauge->value(), 43.0);
    
    gauge->inc(5.0);
    EXPECT_EQ(gauge->value(), 48.0);
    
    gauge->dec();
    EXPECT_EQ(gauge->value(), 47.0);
    
    gauge->dec(7.0);
    EXPECT_EQ(gauge->value(), 40.0);
}

TEST_F(MetricTest, GaugeConcurrent) {
    auto gauge = factory_->create_gauge("test_gauge", "Test gauge metric");
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&gauge]() {
            for (int j = 0; j < 1000; ++j) {
                gauge->inc();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(gauge->value(), 10000.0);
}

TEST_F(MetricTest, CounterBasic) {
    auto counter = factory_->create_counter("test_counter", "Test counter metric");
    EXPECT_EQ(counter->type(), MetricType::COUNTER);
    EXPECT_EQ(counter->name(), "test_counter");
    EXPECT_EQ(counter->help(), "Test counter metric");
    EXPECT_TRUE(counter->labels().empty());
    EXPECT_EQ(counter->value(), 0.0);
}

TEST_F(MetricTest, CounterOperations) {
    auto counter = factory_->create_counter("test_counter", "Test counter metric");
    
    counter->inc();
    EXPECT_EQ(counter->value(), 1.0);
    
    counter->inc(5.0);
    EXPECT_EQ(counter->value(), 6.0);
    
    EXPECT_THROW(counter->inc(-1.0), InvalidArgumentError);
}

TEST_F(MetricTest, CounterConcurrent) {
    auto counter = factory_->create_counter("test_counter", "Test counter metric");
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < 1000; ++j) {
                counter->inc();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(counter->value(), 10000.0);
}

TEST_F(MetricTest, SummaryBasic) {
    std::vector<double> quantiles = {0.5, 0.9, 0.99};
    auto summary = factory_->create_summary(
        "test_summary", "Test summary metric", quantiles,
        std::chrono::seconds(60).count(), 5);
    
    EXPECT_EQ(summary->type(), MetricType::SUMMARY);
    EXPECT_EQ(summary->name(), "test_summary");
    EXPECT_EQ(summary->help(), "Test summary metric");
    EXPECT_TRUE(summary->labels().empty());
    EXPECT_EQ(summary->count(), 0);
    EXPECT_EQ(summary->sum(), 0.0);
}

TEST_F(MetricTest, SummaryObservations) {
    std::vector<double> quantiles = {0.5, 0.9, 0.99};
    auto summary = factory_->create_summary(
        "test_summary", "Test summary metric", quantiles,
        std::chrono::seconds(60).count(), 5);
    
    // Add some observations
    for (int i = 1; i <= 100; ++i) {
        summary->observe(i);
    }
    
    EXPECT_EQ(summary->count(), 100);
    EXPECT_EQ(summary->sum(), 5050.0);  // Sum of 1 to 100
    
    // Check quantiles
    EXPECT_NEAR(summary->quantile(0.5), 50.0, 1.0);
    EXPECT_NEAR(summary->quantile(0.9), 90.0, 1.0);
    EXPECT_NEAR(summary->quantile(0.99), 99.0, 1.0);
}

TEST_F(MetricTest, SummaryAging) {
    std::vector<double> quantiles = {0.5};
    auto summary = factory_->create_summary(
        "test_summary", "Test summary metric", quantiles,
        std::chrono::seconds(1).count(), 2);
    
    summary->observe(1.0);
    EXPECT_EQ(summary->count(), 1);
    
    // Wait for the observation to age out
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    summary->observe(2.0);  // This should trigger cleanup of old observations
    EXPECT_EQ(summary->count(), 1);
    EXPECT_NEAR(summary->quantile(0.5), 2.0, 0.1);
}

TEST_F(MetricTest, SummaryConcurrent) {
    std::vector<double> quantiles = {0.5, 0.9, 0.99};
    auto summary = factory_->create_summary(
        "test_summary", "Test summary metric", quantiles,
        std::chrono::seconds(60).count(), 5);
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&summary]() {
            for (int j = 0; j < 1000; ++j) {
                summary->observe(j);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(summary->count(), 10000);
}

} // namespace
} // namespace core
} // namespace tsdb 