#include "test_fixture.h"
#include <gtest/gtest.h>
#include <cmath>
#include "tsdb/core/types.h"

namespace tsdb {
namespace comprehensive {

class BinaryOperatorTest : public ComprehensivePromQLTest {
protected:
    void SetUp() override {
        ComprehensivePromQLTest::SetUp();
        
        // Create test data
        // Series 1: test_metric_binary_op{method="GET", handler="/api"}
        // Values: 10, 20, 30...
        GenerateSeries("test_metric_binary_op", 
                      {{"method", "GET"}, {"handler", "/api"}}, 
                      100, 10.0, 10.0);
                      
        // Series 2: test_metric_binary_op{method="POST", handler="/api"}
        // Values: 5, 10, 15...
        GenerateSeries("test_metric_binary_op", 
                      {{"method", "POST"}, {"handler", "/api"}}, 
                      100, 5.0, 5.0);
                      
        // Series 3: error_rate{method="GET"}
        // Values: 0.1, 0.2...
        GenerateSeries("error_rate", 
                      {{"method", "GET"}}, 
                      100, 0.1, 0.1);
    }

    void GenerateSeries(const std::string& name, 
                       const std::map<std::string, std::string>& labels_map,
                       int count, double start_val, double step_val) {
        tsdb::core::Labels::Map map = labels_map;
        map["__name__"] = name;
        tsdb::core::Labels labels(map);
        
        tsdb::core::TimeSeries series(labels);
        int64_t now = GetQueryTime();
        // Generate data ending at 'now'
        int64_t start_ts = now - (count * 10000); // 10s step
        
        for (int i = 0; i < count; ++i) {
            series.add_sample(start_ts + i * 10000, start_val + i * step_val);
        }
        
        auto result = storage_->write(series);
        if (!result.ok()) {
            std::cerr << "Failed to write series: " << result.error() << std::endl;
        }
    }
};

TEST_F(BinaryOperatorTest, ScalarScalarArithmetic) {
    int64_t timestamp = GetQueryTime();

    // Addition
    auto result = ExecuteQuery("2 + 3", timestamp);
    ASSERT_FALSE(result.type == tsdb::prometheus::promql::ValueType::NONE); // Check for error/empty
    ASSERT_TRUE(result.isScalar());
    EXPECT_DOUBLE_EQ(result.getScalar().value, 5.0);
    
    // Subtraction
    result = ExecuteQuery("10 - 4", timestamp);
    EXPECT_DOUBLE_EQ(result.getScalar().value, 6.0);
    
    // Multiplication
    result = ExecuteQuery("3 * 4", timestamp);
    EXPECT_DOUBLE_EQ(result.getScalar().value, 12.0);
    
    // Division
    result = ExecuteQuery("20 / 4", timestamp);
    EXPECT_DOUBLE_EQ(result.getScalar().value, 5.0);
    
    // Modulo
    result = ExecuteQuery("10 % 3", timestamp);
    EXPECT_DOUBLE_EQ(result.getScalar().value, 1.0);
    
    // Power
    result = ExecuteQuery("2 ^ 3", timestamp);
    EXPECT_DOUBLE_EQ(result.getScalar().value, 8.0);
}

TEST_F(BinaryOperatorTest, VectorScalarArithmetic) {
    int64_t timestamp = GetQueryTime();

    // Vector + Scalar
    // test_metric_binary_op{method="GET"} has value 10 + 99*10 = 1000 at end (roughly)
    // Actually loop is 0 to 99. i=99 -> 10 + 990 = 1000.
    
    auto result = ExecuteQuery("test_metric_binary_op{method=\"GET\"} + 10", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    ASSERT_EQ(result.getVector().size(), 1);
    
    double val = result.getVector()[0].value;
    EXPECT_DOUBLE_EQ(val, 1010.0); // 1000 + 10
    
    // Check if label is preserved (except __name__)
    // Result sample metric labels should NOT have __name__
    bool has_name = false;
    for (const auto& l : result.getVector()[0].metric.labels()) {
        if (l.first == "__name__") has_name = true;
    }
    EXPECT_FALSE(has_name);
    
    // Should have method="GET"
    bool has_method = false;
    for (const auto& l : result.getVector()[0].metric.labels()) {
        if (l.first == "method" && l.second == "GET") has_method = true;
    }
    EXPECT_TRUE(has_method);
}

TEST_F(BinaryOperatorTest, VectorVectorArithmeticOneToOne) {
    int64_t timestamp = GetQueryTime();

    // test_metric_binary_op{method="GET"} + test_metric_binary_op{method="GET"}
    // Should double the value.
    
    auto result = ExecuteQuery("test_metric_binary_op{method=\"GET\"} + test_metric_binary_op{method=\"GET\"}", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    ASSERT_EQ(result.getVector().size(), 1);
    
    double val = result.getVector()[0].value;
    EXPECT_DOUBLE_EQ(val, 2000.0); // 1000 + 1000
}

TEST_F(BinaryOperatorTest, ComparisonFiltering) {
    int64_t timestamp = GetQueryTime();

    // 2 > 1 -> 1 (scalar)
    auto result = ExecuteQuery("2 > 1", timestamp);
    ASSERT_TRUE(result.isScalar());
    EXPECT_DOUBLE_EQ(result.getScalar().value, 1.0);
    
    // 1 > 2 -> 0 (scalar)
    result = ExecuteQuery("1 > 2", timestamp);
    EXPECT_DOUBLE_EQ(result.getScalar().value, 0.0);
    
    // Vector > Scalar
    // test_metric_binary_op > 0 (Should keep all)
    result = ExecuteQuery("test_metric_binary_op > 0", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    // We generated 2 series for test_metric_binary_op
    EXPECT_EQ(result.getVector().size(), 2);
    
    // Vector > Huge (Should keep none)
    result = ExecuteQuery("test_metric_binary_op > 1000000000", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    EXPECT_EQ(result.getVector().size(), 0);
}

TEST_F(BinaryOperatorTest, LogicalOperators) {
    int64_t timestamp = GetQueryTime();

    // AND
    // test_metric_binary_op{method="GET"} AND test_metric_binary_op{handler="/api"}
    // Both match the same series (GET /api)
    auto result = ExecuteQuery("test_metric_binary_op{method=\"GET\"} and test_metric_binary_op{handler=\"/api\"}", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    EXPECT_EQ(result.getVector().size(), 1);
    EXPECT_EQ(result.getVector()[0].metric.GetLabelValue("method"), "GET");

    // AND (No match)
    // test_metric_binary_op{method="GET"} AND test_metric_binary_op{method="POST"}
    // Disjoint sets (assuming one-to-one matching on all labels by default)
    result = ExecuteQuery("test_metric_binary_op{method=\"GET\"} and test_metric_binary_op{method=\"POST\"}", timestamp);
    EXPECT_EQ(result.getVector().size(), 0);

    // OR
    // test_metric_binary_op{method="GET"} OR test_metric_binary_op{method="POST"}
    // Union -> 2 series
    result = ExecuteQuery("test_metric_binary_op{method=\"GET\"} or test_metric_binary_op{method=\"POST\"}", timestamp);
    EXPECT_EQ(result.getVector().size(), 2);

    // UNLESS
    // test_metric_binary_op{method="GET"} UNLESS test_metric_binary_op{method="POST"}
    // Should keep GET
    result = ExecuteQuery("test_metric_binary_op{method=\"GET\"} unless test_metric_binary_op{method=\"POST\"}", timestamp);
    EXPECT_EQ(result.getVector().size(), 1);
    EXPECT_EQ(result.getVector()[0].metric.GetLabelValue("method"), "GET");

    // UNLESS (Match)
    // test_metric_binary_op{method="GET"} UNLESS test_metric_binary_op{handler="/api"}
    // Should be empty (since GET has handler=/api)
    result = ExecuteQuery("test_metric_binary_op{method=\"GET\"} unless test_metric_binary_op{handler=\"/api\"}", timestamp);
    EXPECT_EQ(result.getVector().size(), 0);
}

TEST_F(BinaryOperatorTest, BoolModifier) {
    int64_t timestamp = GetQueryTime();

    // 2 > bool 1 -> 1 (scalar)
    auto result = ExecuteQuery("2 > bool 1", timestamp);
    ASSERT_TRUE(result.isScalar());
    EXPECT_DOUBLE_EQ(result.getScalar().value, 1.0);
    
    // 1 > bool 2 -> 0 (scalar)
    result = ExecuteQuery("1 > bool 2", timestamp);
    EXPECT_DOUBLE_EQ(result.getScalar().value, 0.0);
    
    // Vector > bool Scalar
    // test_metric_binary_op > bool 0 (Should return all with value 1)
    result = ExecuteQuery("test_metric_binary_op > bool 0", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    EXPECT_EQ(result.getVector().size(), 2);
    EXPECT_DOUBLE_EQ(result.getVector()[0].value, 1.0);
    
    // Vector > bool Huge (Should return all with value 0)
    result = ExecuteQuery("test_metric_binary_op > bool 1000000000", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    EXPECT_EQ(result.getVector().size(), 2);
    EXPECT_DOUBLE_EQ(result.getVector()[0].value, 0.0);
}

TEST_F(BinaryOperatorTest, VectorMatchingGrouping) {
    int64_t timestamp = GetQueryTime();

    // Setup data for Many-to-One
    // Many: test_metric_binary_op (method=GET/POST, handler=/api)
    // One: error_rate (method=GET)
    
    // Many-to-One (group_left)
    // test_metric_binary_op * on(method) group_left(handler) error_rate
    // Should match GET (2 series in LHS? No, LHS has GET and POST. RHS has GET. So only GET matches).
    // Wait, LHS has {method="GET", handler="/api"} and {method="POST", handler="/api"}.
    // RHS has {method="GET"}.
    // Matching on method.
    // LHS GET matches RHS GET.
    // LHS POST matches nothing.
    // Result: 1 series (GET).
    
    auto result = ExecuteQuery("test_metric_binary_op * on(method) group_left error_rate", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    EXPECT_EQ(result.getVector().size(), 1);
    EXPECT_EQ(result.getVector()[0].metric.GetLabelValue("method"), "GET");
    // Value should be 1000 * 0.1 = 100 (roughly)
    // 1010 * 10.1? No, generated values are start + i*step.
    // test_metric: 10 + 99*10 = 1000.
    // error_rate: 0.1 + 99*0.1 = 10.0.
    // Result: 1000 * 10 = 10000.
    EXPECT_NEAR(result.getVector()[0].value, 10000.0, 1000.0);

    // One-to-Many (group_right)
    // error_rate * on(method) group_right test_metric_binary_op
    // RHS has GET and POST. LHS has GET.
    // GET matches GET.
    // Result: 1 series (from RHS? No, result usually takes labels from "Many" side + "One" side included labels).
    // Wait, result labels come from the "Many" side?
    // No, result labels come from LHS, plus included labels from RHS.
    // But for group_right, LHS is One, RHS is Many.
    // So for each LHS, we find matches in RHS.
    // If LHS matches multiple RHS, we get multiple results.
    // Here LHS (GET) matches RHS (GET). Only 1 match in RHS?
    // Yes, RHS has {method="GET", handler="/api"} (1 series).
    // So result is 1 series.
    
    result = ExecuteQuery("error_rate * on(method) group_right test_metric_binary_op", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    EXPECT_EQ(result.getVector().size(), 1);
    
    // Check include labels?
    // group_left(handler) means include handler from RHS (error_rate)?
    // No, error_rate doesn't have handler.
    // In first query: test_metric (Many) * error_rate (One). group_left.
    // We keep all labels from Many (test_metric), so handler is already there.
    
    // Let's try to include a label from One side.
    // Add a label to error_rate?
    // I can't easily add labels to existing series without generating new ones.
    // But I can use existing labels.
    // error_rate has method="GET".
    // test_metric has method="GET".
    
    // Let's try One-to-Many where One matches MULTIPLE Many.
    // Create another series for test_metric with method="GET" but different handler.
    GenerateSeries("test_metric_binary_op", 
                  {{"method", "GET"}, {"handler", "/login"}}, 
                  100, 20.0, 20.0);
                  
    // Now test_metric has 2 series with method="GET" (/api and /login).
    // error_rate has 1 series with method="GET".
    
    // One-to-Many (group_right)
    // error_rate * on(method) group_right test_metric_binary_op
    // LHS (error_rate) matches 2 series in RHS.
    // Should return 2 series.
    result = ExecuteQuery("error_rate * on(method) group_right test_metric_binary_op", timestamp);
    EXPECT_EQ(result.getVector().size(), 2);
}

} // namespace comprehensive
} // namespace tsdb
