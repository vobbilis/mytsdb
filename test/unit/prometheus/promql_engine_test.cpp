#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/storage/adapter.h"

using namespace tsdb::prometheus::promql;
using namespace tsdb::prometheus::model;
using namespace tsdb::prometheus::storage;

// Mock Storage Adapter
class MockStorageAdapter : public StorageAdapter {
public:
    MOCK_METHOD(Matrix, SelectSeries, (const std::vector<LabelMatcher>&, int64_t, int64_t), (override));
    MOCK_METHOD(std::vector<std::string>, LabelNames, (), (override));
    MOCK_METHOD(std::vector<std::string>, LabelValues, (const std::string&), (override));
};

class PromQLEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage_ = new MockStorageAdapter();
        options_.storage_adapter = storage_;
        engine_ = std::make_unique<Engine>(options_);
    }

    void TearDown() override {
        delete storage_;
    }

    MockStorageAdapter* storage_;
    EngineOptions options_;
    std::unique_ptr<Engine> engine_;
};

TEST_F(PromQLEngineTest, TestTimeFunction) {
    int64_t eval_time = 1000000; // 1000 seconds
    auto result = engine_->ExecuteInstant("time()", eval_time);
    
    ASSERT_FALSE(result.hasError()) << result.error;
    ASSERT_TRUE(result.value.isScalar());
    EXPECT_EQ(result.value.getScalar().timestamp, eval_time);
    EXPECT_DOUBLE_EQ(result.value.getScalar().value, 1000.0);
}

TEST_F(PromQLEngineTest, TestScalarArithmetic) {
    // 1 + 1
    auto result = engine_->ExecuteInstant("1 + 1", 0);
    ASSERT_FALSE(result.hasError()) << result.error;
    ASSERT_TRUE(result.value.isScalar());
    EXPECT_DOUBLE_EQ(result.value.getScalar().value, 2.0);
}

TEST_F(PromQLEngineTest, TestRateFunction) {
    // rate(http_requests_total[5m])
    // We need to mock storage response
    
    int64_t eval_time = 300000; // 5 minutes
    int64_t lookback = 300000;  // 5 minutes
    
    // Construct mock matrix
    Matrix matrix;
    Series s1;
    s1.metric.AddLabel("__name__", "http_requests_total");
    s1.metric.AddLabel("job", "api");
    // 0s: 0, 60s: 10, 120s: 20, ..., 300s: 50
    for (int i = 0; i <= 5; ++i) {
        s1.samples.emplace_back(i * 60000, i * 10.0);
    }
    matrix.push_back(s1);
    
    EXPECT_CALL(*storage_, SelectSeries(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(matrix));
        
    auto result = engine_->ExecuteInstant("rate(http_requests_total[5m])", eval_time);
    
    ASSERT_FALSE(result.hasError()) << result.error;
    ASSERT_TRUE(result.value.isVector());
    const auto& vector = result.value.getVector();
    ASSERT_EQ(vector.size(), 1);
    
    // Rate = (50 - 0) / 300 = 0.1666...
    EXPECT_NEAR(vector[0].value, 50.0 / 300.0, 0.001);
    EXPECT_EQ(vector[0].metric.GetLabelValue("job"), "api");
    EXPECT_FALSE(vector[0].metric.HasLabel("__name__")); // Function should drop metric name
}

TEST_F(PromQLEngineTest, TestSumAggregation) {
    // sum(http_requests_total)
    
    // Construct mock vector (from storage)
    // Actually vector selector returns a Vector (Instant Vector)
    // But SelectSeries returns Matrix. Evaluator converts it.
    
    Matrix matrix;
    Series s1;
    s1.metric.AddLabel("__name__", "http_requests_total");
    s1.metric.AddLabel("job", "api");
    s1.metric.AddLabel("instance", "1");
    s1.samples.emplace_back(1000, 10.0);
    
    Series s2;
    s2.metric.AddLabel("__name__", "http_requests_total");
    s2.metric.AddLabel("job", "api");
    s2.metric.AddLabel("instance", "2");
    s2.samples.emplace_back(1000, 20.0);
    
    matrix.push_back(s1);
    matrix.push_back(s2);
    
    EXPECT_CALL(*storage_, SelectSeries(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(matrix));
        
    auto result = engine_->ExecuteInstant("sum(http_requests_total)", 1000);
    
    ASSERT_FALSE(result.hasError()) << result.error;
    ASSERT_TRUE(result.value.isVector());
    const auto& vector = result.value.getVector();
    ASSERT_EQ(vector.size(), 1);
    
    EXPECT_DOUBLE_EQ(vector[0].value, 30.0);
    EXPECT_TRUE(vector[0].metric.labels().empty()); // sum() drops all labels
}

TEST_F(PromQLEngineTest, TestSumByAggregation) {
    // sum(http_requests_total) by (job)
    
    Matrix matrix;
    Series s1;
    s1.metric.AddLabel("__name__", "http_requests_total");
    s1.metric.AddLabel("job", "api");
    s1.metric.AddLabel("instance", "1");
    s1.samples.emplace_back(1000, 10.0);
    
    Series s2;
    s2.metric.AddLabel("__name__", "http_requests_total");
    s2.metric.AddLabel("job", "api");
    s2.metric.AddLabel("instance", "2");
    s2.samples.emplace_back(1000, 20.0);
    
    Series s3;
    s3.metric.AddLabel("__name__", "http_requests_total");
    s3.metric.AddLabel("job", "db");
    s3.metric.AddLabel("instance", "1");
    s3.samples.emplace_back(1000, 5.0);
    
    matrix.push_back(s1);
    matrix.push_back(s2);
    matrix.push_back(s3);
    
    EXPECT_CALL(*storage_, SelectSeries(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(matrix));
        
    auto result = engine_->ExecuteInstant("sum(http_requests_total) by (job)", 1000);
    
    ASSERT_FALSE(result.hasError()) << result.error;
    ASSERT_TRUE(result.value.isVector());
    const auto& vector = result.value.getVector();
    ASSERT_EQ(vector.size(), 2);
    
    // Order is not guaranteed, check map or find
    bool found_api = false;
    bool found_db = false;
    
    for (const auto& sample : vector) {
        auto job = sample.metric.GetLabelValue("job");
        if (job == "api") {
            EXPECT_DOUBLE_EQ(sample.value, 30.0);
            found_api = true;
        } else if (job == "db") {
            EXPECT_DOUBLE_EQ(sample.value, 5.0);
            found_db = true;
        }
    }
    
    EXPECT_TRUE(found_api);
    EXPECT_TRUE(found_db);
}
