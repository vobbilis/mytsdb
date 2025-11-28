#include <gtest/gtest.h>
#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/model/types.h"
#include <cmath>
#include <limits>

namespace tsdb {
namespace prometheus {
namespace promql {

class RemainingFunctionsTest : public ::testing::Test {
protected:
    FunctionRegistry& registry = FunctionRegistry::Instance();
    int64_t timestamp = 1000;

    RemainingFunctionsTest() {
        RegisterOverTimeAggregations(registry);
        RegisterRemainingAggregations(registry);
        RegisterRemainingUtilityFunctions(registry);
    }

    Value CallFunction(const std::string& name, const std::vector<Value>& args) {
        auto func = registry.Get(name);
        if (!func) {
            throw std::runtime_error("Function not found: " + name);
        }
        // Mock evaluator not needed for these functions except for timestamp in absent_over_time
        // We can pass nullptr or a mock if needed.
        // For absent_over_time, we need a way to get timestamp.
        // Let's use a simple mock evaluator if possible, or just pass nullptr and handle it.
        // Actually, absent_over_time uses eval->timestamp().
        // We need to mock Evaluator.
        
        // Simple mock evaluator
        class MockEvaluator : public Evaluator {
        public:
            MockEvaluator(int64_t ts) : Evaluator(ts, 0, nullptr) {}
        };
        
        MockEvaluator eval(timestamp);
        return func->implementation(args, &eval);
    }
};

TEST_F(RemainingFunctionsTest, QuantileOverTime) {
    // 0.5 quantile of [0, 10, 20, 30] -> 15
    Matrix matrix;
    Series s;
    s.metric.AddLabel("__name__", "test");
    s.samples = {
        tsdb::prometheus::Sample(100, 0.0),
        tsdb::prometheus::Sample(200, 10.0),
        tsdb::prometheus::Sample(300, 20.0),
        tsdb::prometheus::Sample(400, 30.0)
    };
    matrix.push_back(s);

    std::vector<Value> args = {
        Value(Scalar{0, 0.5}), // phi
        Value(matrix)
    };

    Value result = CallFunction("quantile_over_time", args);
    ASSERT_TRUE(result.isVector());
    auto vec = result.getVector();
    ASSERT_EQ(vec.size(), 1);
    EXPECT_DOUBLE_EQ(vec[0].value, 15.0);
}

TEST_F(RemainingFunctionsTest, StddevOverTime) {
    // stddev of [2, 4, 4, 4, 5, 5, 7, 9] is 2.0
    Matrix matrix;
    Series s;
    s.metric.AddLabel("__name__", "test");
    s.samples = {
        tsdb::prometheus::Sample(100, 2.0),
        tsdb::prometheus::Sample(200, 4.0),
        tsdb::prometheus::Sample(300, 4.0),
        tsdb::prometheus::Sample(400, 4.0),
        tsdb::prometheus::Sample(500, 5.0),
        tsdb::prometheus::Sample(600, 5.0),
        tsdb::prometheus::Sample(700, 7.0),
        tsdb::prometheus::Sample(800, 9.0)
    };
    matrix.push_back(s);

    std::vector<Value> args = { Value(matrix) };
    Value result = CallFunction("stddev_over_time", args);
    ASSERT_TRUE(result.isVector());
    auto vec = result.getVector();
    ASSERT_EQ(vec.size(), 1);
    EXPECT_DOUBLE_EQ(vec[0].value, 2.0);
}

TEST_F(RemainingFunctionsTest, LastOverTime) {
    Matrix matrix;
    Series s;
    s.metric.AddLabel("__name__", "test");
    s.samples = {
        tsdb::prometheus::Sample(100, 10.0),
        tsdb::prometheus::Sample(200, 20.0)
    };
    matrix.push_back(s);

    std::vector<Value> args = { Value(matrix) };
    Value result = CallFunction("last_over_time", args);
    ASSERT_TRUE(result.isVector());
    auto vec = result.getVector();
    ASSERT_EQ(vec.size(), 1);
    EXPECT_DOUBLE_EQ(vec[0].value, 20.0);
    EXPECT_EQ(vec[0].timestamp, 200);
}

TEST_F(RemainingFunctionsTest, PresentOverTime) {
    Matrix matrix;
    Series s;
    s.metric.AddLabel("__name__", "test");
    s.samples = { tsdb::prometheus::Sample(100, 10.0) };
    matrix.push_back(s);

    std::vector<Value> args = { Value(matrix) };
    Value result = CallFunction("present_over_time", args);
    ASSERT_TRUE(result.isVector());
    auto vec = result.getVector();
    ASSERT_EQ(vec.size(), 1);
    EXPECT_DOUBLE_EQ(vec[0].value, 1.0);
}

TEST_F(RemainingFunctionsTest, AbsentOverTime) {
    // Case 1: Has values -> Empty vector
    Matrix matrix1;
    Series s;
    s.metric.AddLabel("__name__", "test");
    s.samples = { tsdb::prometheus::Sample(100, 10.0) };
    matrix1.push_back(s);

    std::vector<Value> args1 = { Value(matrix1) };
    Value result1 = CallFunction("absent_over_time", args1);
    ASSERT_TRUE(result1.isVector());
    EXPECT_TRUE(result1.getVector().empty());

    // Case 2: No values -> Vector with 1
    Matrix matrix2; // Empty matrix
    std::vector<Value> args2 = { Value(matrix2) };
    Value result2 = CallFunction("absent_over_time", args2);
    ASSERT_TRUE(result2.isVector());
    auto vec2 = result2.getVector();
    ASSERT_EQ(vec2.size(), 1);
    EXPECT_DOUBLE_EQ(vec2[0].value, 1.0);
    EXPECT_EQ(vec2[0].timestamp, timestamp);
}

TEST_F(RemainingFunctionsTest, Group) {
    Vector vec;
    LabelSet l1; l1.AddLabel("a", "1");
    LabelSet l2; l2.AddLabel("a", "2");
    vec.push_back(Sample{l1, 100, 10.0});
    vec.push_back(Sample{l2, 100, 20.0});

    std::vector<Value> args = { Value(vec) };
    Value result = CallFunction("group", args);
    ASSERT_TRUE(result.isVector());
    auto res_vec = result.getVector();
    ASSERT_EQ(res_vec.size(), 2);
    EXPECT_DOUBLE_EQ(res_vec[0].value, 1.0);
    EXPECT_DOUBLE_EQ(res_vec[1].value, 1.0);
}

TEST_F(RemainingFunctionsTest, CountValues) {
    Vector vec;
    LabelSet l1; l1.AddLabel("a", "1");
    LabelSet l2; l2.AddLabel("a", "2");
    LabelSet l3; l3.AddLabel("a", "3");
    
    // Two samples with value 10, one with value 20
    vec.push_back(Sample{l1, 100, 10.0});
    vec.push_back(Sample{l2, 100, 10.0});
    vec.push_back(Sample{l3, 100, 20.0});

    std::vector<Value> args = {
        Value(String{0, "count_label"}),
        Value(vec)
    };

    Value result = CallFunction("count_values", args);
    ASSERT_TRUE(result.isVector());
    auto res_vec = result.getVector();
    ASSERT_EQ(res_vec.size(), 2); // One for 10.0, one for 20.0

    // Check for value 10.0 (count should be 2)
    bool found_10 = false;
    bool found_20 = false;
    
    for (const auto& s : res_vec) {
        auto val_str = s.metric.GetLabelValue("count_label");
        ASSERT_TRUE(val_str.has_value());
        
        double val = std::stod(val_str.value());
        if (std::abs(val - 10.0) < 0.001) {
            EXPECT_DOUBLE_EQ(s.value, 2.0);
            found_10 = true;
        } else if (std::abs(val - 20.0) < 0.001) {
            EXPECT_DOUBLE_EQ(s.value, 1.0);
            found_20 = true;
        }
    }
    
    EXPECT_TRUE(found_10);
    EXPECT_TRUE(found_20);
}

TEST_F(RemainingFunctionsTest, SortByLabel) {
    Vector vec;
    LabelSet l1; l1.AddLabel("instance", "b");
    LabelSet l2; l2.AddLabel("instance", "a");
    LabelSet l3; l3.AddLabel("instance", "c");
    
    vec.push_back(Sample{l1, 100, 1.0});
    vec.push_back(Sample{l2, 100, 2.0});
    vec.push_back(Sample{l3, 100, 3.0});

    std::vector<Value> args = {
        Value(vec),
        Value(String{0, "instance"})
    };

    Value result = CallFunction("sort_by_label", args);
    ASSERT_TRUE(result.isVector());
    auto res_vec = result.getVector();
    ASSERT_EQ(res_vec.size(), 3);
    
    EXPECT_EQ(res_vec[0].metric.GetLabelValue("instance").value(), "a");
    EXPECT_EQ(res_vec[1].metric.GetLabelValue("instance").value(), "b");
    EXPECT_EQ(res_vec[2].metric.GetLabelValue("instance").value(), "c");
}

TEST_F(RemainingFunctionsTest, SortByLabelDesc) {
    Vector vec;
    LabelSet l1; l1.AddLabel("instance", "b");
    LabelSet l2; l2.AddLabel("instance", "a");
    LabelSet l3; l3.AddLabel("instance", "c");
    
    vec.push_back(Sample{l1, 100, 1.0});
    vec.push_back(Sample{l2, 100, 2.0});
    vec.push_back(Sample{l3, 100, 3.0});

    std::vector<Value> args = {
        Value(vec),
        Value(String{0, "instance"})
    };

    Value result = CallFunction("sort_by_label_desc", args);
    ASSERT_TRUE(result.isVector());
    auto res_vec = result.getVector();
    ASSERT_EQ(res_vec.size(), 3);
    
    EXPECT_EQ(res_vec[0].metric.GetLabelValue("instance").value(), "c");
    EXPECT_EQ(res_vec[1].metric.GetLabelValue("instance").value(), "b");
    EXPECT_EQ(res_vec[2].metric.GetLabelValue("instance").value(), "a");
}

TEST_F(RemainingFunctionsTest, StdvarOverTime) {
    // stdvar of [2, 4, 4, 4, 5, 5, 7, 9]
    // mean = 5.0
    // variance = sum((x - mean)^2) / N
    // (9 + 1 + 1 + 1 + 0 + 0 + 4 + 16) / 8 = 32 / 8 = 4.0
    Matrix matrix;
    Series s;
    s.metric.AddLabel("__name__", "test");
    s.samples = {
        tsdb::prometheus::Sample(100, 2.0),
        tsdb::prometheus::Sample(200, 4.0),
        tsdb::prometheus::Sample(300, 4.0),
        tsdb::prometheus::Sample(400, 4.0),
        tsdb::prometheus::Sample(500, 5.0),
        tsdb::prometheus::Sample(600, 5.0),
        tsdb::prometheus::Sample(700, 7.0),
        tsdb::prometheus::Sample(800, 9.0)
    };
    matrix.push_back(s);

    std::vector<Value> args = { Value(matrix) };
    Value result = CallFunction("stdvar_over_time", args);
    ASSERT_TRUE(result.isVector());
    auto vec = result.getVector();
    ASSERT_EQ(vec.size(), 1);
    EXPECT_DOUBLE_EQ(vec[0].value, 4.0);
}

TEST_F(RemainingFunctionsTest, Changes) {
    // [1, 1, 2, 2, 2, 3, 1] -> changes: 1->2 (1), 2->3 (1), 3->1 (1) = 3 changes
    Matrix matrix;
    Series s;
    s.metric.AddLabel("__name__", "test");
    s.samples = {
        tsdb::prometheus::Sample(100, 1.0),
        tsdb::prometheus::Sample(200, 1.0),
        tsdb::prometheus::Sample(300, 2.0),
        tsdb::prometheus::Sample(400, 2.0),
        tsdb::prometheus::Sample(500, 2.0),
        tsdb::prometheus::Sample(600, 3.0),
        tsdb::prometheus::Sample(700, 1.0)
    };
    matrix.push_back(s);

    std::vector<Value> args = { Value(matrix) };
    Value result = CallFunction("changes", args);
    ASSERT_TRUE(result.isVector());
    auto vec = result.getVector();
    ASSERT_EQ(vec.size(), 1);
    EXPECT_DOUBLE_EQ(vec[0].value, 3.0);
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
