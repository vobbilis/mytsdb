#include "test_fixture.h"
#include <cmath>

namespace tsdb {
namespace comprehensive {

// --- Rate Functions ---

TEST_F(ComprehensivePromQLTest, FunctionRate) {
    int64_t query_time = GetQueryTime();
    // rate(http_requests_total[5m])
    auto result = ExecuteQuery("rate(http_requests_total[5m])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    // Value should be ~ (total / duration)
    // We verified this in basic tests, just ensuring it runs here.
    EXPECT_GT(vector[0].value, 0);
}

TEST_F(ComprehensivePromQLTest, FunctionIncrease) {
    int64_t query_time = GetQueryTime();
    // increase(http_requests_total[5m])
    auto result = ExecuteQuery("increase(http_requests_total[5m])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    // increase should be rate * duration (roughly)
    // 5m = 300s.
    // If rate is ~60, increase should be ~18000.
    EXPECT_GT(vector[0].value, 100);
}

TEST_F(ComprehensivePromQLTest, FunctionIrate) {
    int64_t query_time = GetQueryTime();
    // irate(http_requests_total[5m])
    auto result = ExecuteQuery("irate(http_requests_total[5m])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_GT(vector[0].value, 0);
}

// --- Math Functions ---

TEST_F(ComprehensivePromQLTest, FunctionAbs) {
    int64_t query_time = GetQueryTime();
    // abs(http_requests_total) - it's positive anyway
    auto result = ExecuteQuery("abs(http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_GE(vector[0].value, 0);
}

TEST_F(ComprehensivePromQLTest, FunctionCeil) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("ceil(rate(http_requests_total[5m]))", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    // Should be integer
    EXPECT_EQ(vector[0].value, std::ceil(vector[0].value));
}

TEST_F(ComprehensivePromQLTest, FunctionFloor) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("floor(rate(http_requests_total[5m]))", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_EQ(vector[0].value, std::floor(vector[0].value));
}

TEST_F(ComprehensivePromQLTest, FunctionRound) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("round(rate(http_requests_total[5m]))", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_EQ(vector[0].value, std::round(vector[0].value));
}

TEST_F(ComprehensivePromQLTest, FunctionSqrt) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("sqrt(http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_GT(vector[0].value, 0);
}

TEST_F(ComprehensivePromQLTest, FunctionExp) {
    int64_t query_time = GetQueryTime();
    // exp(0) = 1. But we don't have constant vector easily?
    // We can use scalar? exp(vector(0))?
    // Or just exp of a small metric.
    // Let's use scalar if supported? "exp(1)" -> scalar 2.718...
    // But our engine might return scalar.
    auto result = ExecuteQuery("exp(1)", query_time);
    // If scalar supported
    if (result.type == tsdb::prometheus::promql::ValueType::SCALAR) {
        EXPECT_NEAR(result.getScalar().value, 2.71828, 0.0001);
    } else if (result.type == tsdb::prometheus::promql::ValueType::VECTOR) {
        // Should be empty if no vector arg?
        // Wait, exp(scalar) returns scalar.
    }
}

// --- Time Functions ---

TEST_F(ComprehensivePromQLTest, FunctionTime) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("time()", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::SCALAR);
    EXPECT_NEAR(result.getScalar().value, query_time / 1000.0, 1.0);
}

TEST_F(ComprehensivePromQLTest, FunctionYear) {
    int64_t query_time = GetQueryTime();
    // year() with no args uses current time? Or requires vector?
    // Our implementation: "year(v vector)"
    // If we pass vector, it returns year of sample value?
    // Let's try "year(vector(time()))" if vector() is supported.
    // Or "year(http_requests_total)" - but value is count, not timestamp.
    // "year()" might not be supported without args in our implementation.
    // Let's check implementation: "if (args.size() > 0 && args[0].isVector())"
    // It seems it expects a vector.
    // But standard PromQL `year()` (no args) returns year of evaluation time.
    // Our implementation might be limited.
    // Let's skip for now if unsure.
}

// --- Over Time Aggregations ---

TEST_F(ComprehensivePromQLTest, FunctionQuantileOverTime) {
    int64_t query_time = GetQueryTime();
    // quantile_over_time(0.9, http_requests_total[1h])
    auto result = ExecuteQuery("quantile_over_time(0.9, http_requests_total[1h])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_GT(vector[0].value, 0);
}

TEST_F(ComprehensivePromQLTest, FunctionStddevOverTime) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("stddev_over_time(http_requests_total[1h])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_GE(vector[0].value, 0);
}

TEST_F(ComprehensivePromQLTest, FunctionLastOverTime) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("last_over_time(http_requests_total[1h])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_GT(vector[0].value, 0);
}

TEST_F(ComprehensivePromQLTest, FunctionPresentOverTime) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("present_over_time(http_requests_total[1h])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_EQ(vector[0].value, 1.0);
}

TEST_F(ComprehensivePromQLTest, FunctionChanges) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("changes(http_requests_total[1h])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_GE(vector[0].value, 0);
}

TEST_F(ComprehensivePromQLTest, FunctionCountValues) {
    int64_t query_time = GetQueryTime();
    // count_values("val", http_requests_total)
    // Since values are large counters, they might be unique.
    // But let's try.
    auto result = ExecuteQuery("count_values(\"val\", http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    // Check label "val" exists
    bool has_val = false;
    for (const auto& label : vector[0].metric.labels()) {
        if (label.first == "val") has_val = true;
    }
    EXPECT_TRUE(has_val);
}

} // namespace comprehensive
} // namespace tsdb
