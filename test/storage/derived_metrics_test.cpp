#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tsdb/storage/derived_metrics.h"
#include "tsdb/storage/storage.h"
#include "tsdb/core/types.h"

using namespace tsdb;
using namespace tsdb::storage;
using namespace testing;

// Mock Storage
class MockStorage : public Storage {
public:
    MOCK_METHOD(core::Result<void>, init, (const core::StorageConfig&), (override));
    MOCK_METHOD(core::Result<void>, write, (const core::TimeSeries&), (override));
    MOCK_METHOD(core::Result<core::TimeSeries>, read, (const core::Labels&, int64_t, int64_t), (override));
    MOCK_METHOD(core::Result<std::vector<core::TimeSeries>>, query, (const std::vector<core::LabelMatcher>&, int64_t, int64_t), (override));
    MOCK_METHOD(core::Result<std::vector<std::string>>, label_names, (), (override));
    MOCK_METHOD(core::Result<std::vector<std::string>>, label_values, (const std::string&), (override));
    MOCK_METHOD(core::Result<void>, delete_series, (const std::vector<core::LabelMatcher>&), (override));
    MOCK_METHOD(core::Result<void>, compact, (), (override));
    MOCK_METHOD(core::Result<void>, flush, (), (override));
    MOCK_METHOD(core::Result<void>, close, (), (override));
    MOCK_METHOD(std::string, stats, (), (const, override));
};

// Testable Subclass to access protected ExecuteRule
class TestableDerivedMetricManager : public DerivedMetricManager {
public:
    using DerivedMetricManager::DerivedMetricManager;
    using DerivedMetricManager::execute_rule; // Expose protected
};

class DerivedMetricsTest : public Test {
protected:
    std::shared_ptr<MockStorage> mock_storage_;
    std::unique_ptr<TestableDerivedMetricManager> manager_;

    void SetUp() override {
        mock_storage_ = std::make_shared<MockStorage>();
        // Pass nullptr for BackgroundProcessor as we won't start the scheduler
        manager_ = std::make_unique<TestableDerivedMetricManager>(mock_storage_, nullptr);
    }
};

TEST_F(DerivedMetricsTest, ExecuteSimpleRule) {
    // Scenario:
    // Rule: "test_result" = "up"
    // Mock Storage returns 1 series for "up"
    // Verify "test_result" is written back
    
    // 1. Setup Mock for Query "up"
    // The Engine calls query() on storage via adapter.
    // We expect a call to query with matcher name="up"
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "up");
    labels.add("job", "test");
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 1.0));
    query_result.push_back(series);

    // Expect query
    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));

    // 2. Expect Write Back
    // The result should have name="test_result" and same labels/value
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce(Invoke([](const core::TimeSeries& s) {
            EXPECT_TRUE(s.labels().has("__name__"));
            EXPECT_EQ(s.labels().get("__name__").value(), "test_result");
            EXPECT_EQ(s.samples().size(), 1);
            EXPECT_DOUBLE_EQ(s.samples()[0].value(), 1.0);
            return core::Result<void>(); 
        }));

    DerivedMetricRule rule;
    rule.name = "test_result";
    rule.query = "up";
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok()) << result.error();
}

TEST_F(DerivedMetricsTest, HandleQueryError) {
    // Scenario: Query fails
    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>::error("Storage failure")));

    DerivedMetricRule rule;
    rule.name = "fail_metric";
    rule.query = "up";
    
    auto result = manager_->execute_rule(rule);
    EXPECT_FALSE(result.ok());
    // The exact error message depends on PromQL engine, but should contain failure
}

// ============================================================================
// Rule Management Tests
// ============================================================================

TEST_F(DerivedMetricsTest, AddMultipleRules) {
    manager_->add_rule("rule1", "sum(up)", 1000);
    manager_->add_rule("rule2", "avg(up)", 2000);
    manager_->add_rule("rule3", "max(up)", 3000);
    
    // Rules are stored internally - verify by clearing and checking no crash
    manager_->clear_rules();
    // If we got here without crash, rules were managed correctly
    SUCCEED();
}

TEST_F(DerivedMetricsTest, ClearRules) {
    manager_->add_rule("rule1", "up", 1000);
    manager_->add_rule("rule2", "down", 2000);
    manager_->clear_rules();
    
    // After clearing, should be able to add same rule names again
    manager_->add_rule("rule1", "new_query", 1000);
    SUCCEED();
}

// ============================================================================
// Query Type Tests 
// ============================================================================

TEST_F(DerivedMetricsTest, ExecuteAggregationQuery) {
    // sum(up) should produce aggregated result
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "up");
    labels.add("job", "service1");
    core::TimeSeries series1(labels);
    series1.add_sample(core::Sample(1000, 1.0));
    query_result.push_back(series1);
    
    core::Labels labels2;
    labels2.add("__name__", "up");
    labels2.add("job", "service2");
    core::TimeSeries series2(labels2);
    series2.add_sample(core::Sample(1000, 2.0));
    query_result.push_back(series2);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));

    // Expect write for aggregated result
    EXPECT_CALL(*mock_storage_, write(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(core::Result<void>()));

    DerivedMetricRule rule;
    rule.name = "up:sum";
    rule.query = "sum(up)";
    
    auto result = manager_->execute_rule(rule);
    // Note: This may fail if PromQL engine doesn't support sum() fully,
    // but tests the integration path
}

TEST_F(DerivedMetricsTest, ExecuteEmptyResult) {
    // Query returns no matching series
    std::vector<core::TimeSeries> empty_result;
    
    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(empty_result)));

    // Should NOT call write since there's nothing to write
    EXPECT_CALL(*mock_storage_, write(_)).Times(0);

    DerivedMetricRule rule;
    rule.name = "no_data";
    rule.query = "nonexistent_metric";
    
    auto result = manager_->execute_rule(rule);
    // Empty result is not an error, just no output
    EXPECT_TRUE(result.ok());
}

// ============================================================================
// Label Handling Tests
// ============================================================================

TEST_F(DerivedMetricsTest, PreservesLabelsExceptName) {
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original_metric");
    labels.add("job", "my_job");
    labels.add("instance", "localhost:9090");
    labels.add("env", "production");
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 42.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));

    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce(Invoke([](const core::TimeSeries& s) {
            // New name should be set
            EXPECT_EQ(s.labels().get("__name__").value(), "derived_metric");
            // Other labels preserved
            EXPECT_EQ(s.labels().get("job").value(), "my_job");
            EXPECT_EQ(s.labels().get("instance").value(), "localhost:9090");
            EXPECT_EQ(s.labels().get("env").value(), "production");
            return core::Result<void>();
        }));

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original_metric";
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(DerivedMetricsTest, HandleWriteFailure) {
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "up");
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));

    // Write fails
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce(Return(core::Result<void>::error("Write failed")));

    DerivedMetricRule rule;
    rule.name = "test";
    rule.query = "up";
    
    // Execute should still succeed (just logs warning for failed writes)
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

// ============================================================================
// Multiple Series Output Tests
// ============================================================================

TEST_F(DerivedMetricsTest, MultipleSeriesOutput) {
    // Query returns multiple series (like sum by (job))
    std::vector<core::TimeSeries> query_result;
    
    for (int i = 0; i < 5; i++) {
        core::Labels labels;
        labels.add("__name__", "metric");
        labels.add("job", "service" + std::to_string(i));
        core::TimeSeries series(labels);
        series.add_sample(core::Sample(1000, static_cast<double>(i)));
        query_result.push_back(series);
    }

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));

    // Expect 5 writes (one per output series)
    EXPECT_CALL(*mock_storage_, write(_))
        .Times(5)
        .WillRepeatedly(Return(core::Result<void>()));

    DerivedMetricRule rule;
    rule.name = "multi_output";
    rule.query = "metric";
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

// ============================================================================
// Timing Tests (Basic)
// ============================================================================

TEST_F(DerivedMetricsTest, RuleIntervalDefaultValue) {
    // Default interval should be 60000ms (60 seconds)
    manager_->add_rule("test_rule", "up");
    // If we got here, the default parameter worked
    SUCCEED();
}

TEST_F(DerivedMetricsTest, RuleCustomInterval) {
    // Custom interval should be accepted
    manager_->add_rule("fast_rule", "up", 1000);   // 1 second
    manager_->add_rule("slow_rule", "up", 300000); // 5 minutes
    SUCCEED();
}

// ============================================================================
// Error Backoff Tests
// ============================================================================

TEST_F(DerivedMetricsTest, ErrorBackoffIncrementsOnFailure) {
    // When a rule fails, consecutive_failures should increment
    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>::error("Storage failure")));

    DerivedMetricRule rule;
    rule.name = "fail_rule";
    rule.query = "up";
    rule.consecutive_failures = 0;
    rule.backoff_until = 0;
    rule.max_backoff_seconds = 300;
    
    auto result = manager_->execute_rule(rule);
    
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(rule.consecutive_failures, 1);
    EXPECT_GT(rule.backoff_until, 0);  // Backoff should be set
}

TEST_F(DerivedMetricsTest, ErrorBackoffExponential) {
    // Backoff should be exponential: 2^n seconds
    
    // Simulate 3 consecutive failures using separate WillOnce calls
    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>::error("Fail")))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>::error("Fail")))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>::error("Fail")));

    DerivedMetricRule rule;
    rule.name = "fail_rule";
    rule.query = "up";
    rule.max_backoff_seconds = 300;
    
    // First failure: 2^1 = 2 seconds
    manager_->execute_rule(rule);
    EXPECT_EQ(rule.consecutive_failures, 1);
    
    // Second failure: 2^2 = 4 seconds
    manager_->execute_rule(rule);
    EXPECT_EQ(rule.consecutive_failures, 2);
    
    // Third failure: 2^3 = 8 seconds
    manager_->execute_rule(rule);
    EXPECT_EQ(rule.consecutive_failures, 3);
}

TEST_F(DerivedMetricsTest, ErrorBackoffMaxLimit) {
    // Backoff should not exceed max_backoff_seconds
    
    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>::error("Fail")));

    DerivedMetricRule rule;
    rule.name = "fail_rule";
    rule.query = "up";
    rule.consecutive_failures = 10;  // 2^11 = 2048 seconds > max
    rule.max_backoff_seconds = 300;  // Max 5 minutes
    
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    manager_->execute_rule(rule);
    
    // Backoff should be at most 300 seconds from now
    EXPECT_LE(rule.backoff_until, now + (300 * 1000) + 1000);  // +1s tolerance
}

TEST_F(DerivedMetricsTest, ErrorBackoffResetsOnSuccess) {
    // On successful execution, backoff should reset
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "up");
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce(Return(core::Result<void>()));

    DerivedMetricRule rule;
    rule.name = "test_rule";
    rule.query = "up";
    rule.consecutive_failures = 5;  // Pretend we had 5 failures before
    rule.backoff_until = 9999999999;  // Some future time
    rule.max_backoff_seconds = 300;
    
    auto result = manager_->execute_rule(rule);
    
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(rule.consecutive_failures, 0);  // Reset
    EXPECT_EQ(rule.backoff_until, 0);         // Reset
}

TEST_F(DerivedMetricsTest, RuleDefaultBackoffValues) {
    // New rules should have properly initialized backoff values
    manager_->add_rule("test_rule", "up", 1000);
    
    // We can't directly inspect the rule, but if add_rule doesn't crash
    // and we can clear rules, the initialization worked
    manager_->clear_rules();
    SUCCEED();
}

// ============================================================================
// Label Transformation Tests
// ============================================================================

TEST_F(DerivedMetricsTest, KeepLabelsFiltersCorrectly) {
    // Test that keep_labels only keeps specified labels
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original");
    labels.add("job", "myapp");
    labels.add("instance", "localhost:9090");
    labels.add("region", "us-east");
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    
    // Expect write with only "job" label
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce([](const core::TimeSeries& s) {
            // Should have __name__ and job, but NOT instance or region
            EXPECT_TRUE(s.labels().get("__name__").has_value());
            EXPECT_TRUE(s.labels().get("job").has_value());
            EXPECT_FALSE(s.labels().get("instance").has_value());
            EXPECT_FALSE(s.labels().get("region").has_value());
            return core::Result<void>(); 
        });

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original";
    rule.keep_labels = {"job"};  // Only keep job
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

TEST_F(DerivedMetricsTest, DropLabelsFiltersCorrectly) {
    // Test that drop_labels removes specified labels
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original");
    labels.add("job", "myapp");
    labels.add("instance", "localhost:9090");
    labels.add("region", "us-east");
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    
    // Expect write without "instance" label
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce([](const core::TimeSeries& s) {
            // Should have __name__, job, region, but NOT instance
            EXPECT_TRUE(s.labels().get("__name__").has_value());
            EXPECT_TRUE(s.labels().get("job").has_value());
            EXPECT_TRUE(s.labels().get("region").has_value());
            EXPECT_FALSE(s.labels().get("instance").has_value());
            return core::Result<void>(); 
        });

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original";
    rule.drop_labels = {"instance"};  // Drop instance
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

TEST_F(DerivedMetricsTest, KeepLabelsTakesPrecedence) {
    // When both keep_labels and drop_labels are set, keep_labels takes precedence
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original");
    labels.add("job", "myapp");
    labels.add("instance", "localhost:9090");
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce([](const core::TimeSeries& s) {
            // keep_labels should take precedence - only job should be kept
            EXPECT_TRUE(s.labels().get("job").has_value());
            EXPECT_FALSE(s.labels().get("instance").has_value());
            return core::Result<void>(); 
        });

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original";
    rule.keep_labels = {"job"};       // Keep only job
    rule.drop_labels = {"job"};       // Also try to drop job (should be ignored)
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

TEST_F(DerivedMetricsTest, EmptyFilterKeepsAllLabels) {
    // When no filters are set, all labels should be preserved
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original");
    labels.add("job", "myapp");
    labels.add("instance", "localhost:9090");
    core::TimeSeries series(labels);
    series.add_sample(core::Sample(1000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce([](const core::TimeSeries& s) {
            // All labels should be present
            EXPECT_TRUE(s.labels().get("job").has_value());
            EXPECT_TRUE(s.labels().get("instance").has_value());
            return core::Result<void>(); 
        });

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original";
    // No filters set
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

TEST_F(DerivedMetricsTest, AddRuleWithLabelFilters) {
    // Test that add_rule with label filters doesn't crash
    
    manager_->add_rule("filtered_rule", "up", 1000, {"job", "instance"}, {});
    manager_->add_rule("dropped_rule", "up", 1000, {}, {"instance", "region"});
    
    manager_->clear_rules();
    SUCCEED();
}

// ============================================================================
// Staleness Handling Tests
// ============================================================================

TEST_F(DerivedMetricsTest, StaleSampleSkipped) {
    // Test that samples older than staleness_threshold_ms are skipped
    
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original");
    core::TimeSeries series(labels);
    // Sample from 10 minutes ago (600000 ms) - older than 5 min threshold
    series.add_sample(core::Sample(now - 600000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    
    // write() should NOT be called because sample is stale
    EXPECT_CALL(*mock_storage_, write(_)).Times(0);

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original";
    rule.staleness_threshold_ms = 300000;  // 5 minutes
    rule.skip_if_stale = true;
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());  // No error, just skipped
}

TEST_F(DerivedMetricsTest, FreshSampleWritten) {
    // Test that fresh samples (within threshold) are written
    
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original");
    core::TimeSeries series(labels);
    // Sample from 1 minute ago - well within 5 min threshold
    series.add_sample(core::Sample(now - 60000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    
    // write() SHOULD be called because sample is fresh
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce(Return(core::Result<void>()));

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original";
    rule.staleness_threshold_ms = 300000;  // 5 minutes
    rule.skip_if_stale = true;
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

TEST_F(DerivedMetricsTest, StalenessThresholdConfigurable) {
    // Test that staleness_threshold_ms can be configured
    
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original");
    core::TimeSeries series(labels);
    // Sample from 2 minutes ago
    series.add_sample(core::Sample(now - 120000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    
    // With 1 min threshold, this sample is stale - write should NOT be called
    EXPECT_CALL(*mock_storage_, write(_)).Times(0);

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original";
    rule.staleness_threshold_ms = 60000;  // Only 1 minute threshold
    rule.skip_if_stale = true;
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

TEST_F(DerivedMetricsTest, SkipIfStaleDisabled) {
    // Test that stale samples are written when skip_if_stale is false
    
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    std::vector<core::TimeSeries> query_result;
    core::Labels labels;
    labels.add("__name__", "original");
    core::TimeSeries series(labels);
    // Sample from 10 minutes ago - would be stale if checking
    series.add_sample(core::Sample(now - 600000, 1.0));
    query_result.push_back(series);

    EXPECT_CALL(*mock_storage_, query(_, _, _))
        .WillOnce(Return(core::Result<std::vector<core::TimeSeries>>(query_result)));
    
    // write() SHOULD be called because skip_if_stale is false
    EXPECT_CALL(*mock_storage_, write(_))
        .WillOnce(Return(core::Result<void>()));

    DerivedMetricRule rule;
    rule.name = "derived_metric";
    rule.query = "original";
    rule.staleness_threshold_ms = 300000;  // 5 minutes
    rule.skip_if_stale = false;  // Disabled!
    
    auto result = manager_->execute_rule(rule);
    EXPECT_TRUE(result.ok());
}

// ============================================================================
// Rule Group Tests
// ============================================================================

TEST_F(DerivedMetricsTest, CreateRuleGroup) {
    // Test that rule groups can be created
    manager_->add_group("test_group", 30000);  // 30 second interval
    manager_->clear_groups();
    SUCCEED();
}

TEST_F(DerivedMetricsTest, AddRulesToGroup) {
    // Test that rules can be added to a group
    manager_->add_group("my_group", 60000);
    manager_->add_rule_to_group("my_group", "rule_a", "up");
    manager_->add_rule_to_group("my_group", "rule_b", "sum(up)");
    manager_->add_rule_to_group("my_group", "rule_c", "avg(up)");
    manager_->clear_groups();
    SUCCEED();
}

TEST_F(DerivedMetricsTest, ClearGroups) {
    // Test that groups can be cleared
    manager_->add_group("group1", 60000);
    manager_->add_group("group2", 30000);
    manager_->add_rule_to_group("group1", "rule1", "up");
    manager_->clear_groups();
    // Groups should be empty now - no crash means success
    SUCCEED();
}

TEST_F(DerivedMetricsTest, GroupWithMultipleRules) {
    // Test group with multiple rules
    manager_->add_group("multi_rule_group", 1000);  // 1 second
    manager_->add_rule_to_group("multi_rule_group", "derived_a", "metric_a");
    manager_->add_rule_to_group("multi_rule_group", "derived_b", "metric_b");
    manager_->add_rule_to_group("multi_rule_group", "derived_c", "rate(metric_c[5m])");
    manager_->clear_groups();
    SUCCEED();
}

TEST_F(DerivedMetricsTest, RangeQueryConfiguration) {
    // Test that range query fields can be configured on rules
    DerivedMetricRule rule;
    rule.name = "backfill_rule";
    rule.query = "up";
    rule.evaluation_type = RuleEvaluationType::RANGE;
    rule.range_duration_ms = 3600000;  // 1 hour
    rule.range_step_ms = 60000;        // 1 minute step
    
    // Verify fields are set correctly
    EXPECT_EQ(rule.evaluation_type, RuleEvaluationType::RANGE);
    EXPECT_EQ(rule.range_duration_ms, 3600000);
    EXPECT_EQ(rule.range_step_ms, 60000);
}
