#include <gtest/gtest.h>
#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/promql/value.h"

using namespace tsdb::prometheus::promql;
using namespace tsdb::prometheus::model;
using tsdb::prometheus::LabelSet;

// Test sort function
TEST(UtilityTest, Sort) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("sort");
    ASSERT_NE(func, nullptr);
    
    Vector vec;
    LabelSet l1, l2, l3;
    l1.AddLabel("instance", "1");
    l2.AddLabel("instance", "2");
    l3.AddLabel("instance", "3");
    
    vec.push_back(Sample{l1, 1000, 30.0});
    vec.push_back(Sample{l2, 1000, 10.0});
    vec.push_back(Sample{l3, 1000, 20.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    
    Value result = func->implementation(args, nullptr);
    const auto& sorted = result.getVector();
    
    ASSERT_EQ(sorted.size(), 3);
    EXPECT_EQ(sorted[0].value, 10.0);
    EXPECT_EQ(sorted[1].value, 20.0);
    EXPECT_EQ(sorted[2].value, 30.0);
}

// Test sort_desc function
TEST(UtilityTest, SortDesc) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("sort_desc");
    ASSERT_NE(func, nullptr);
    
    Vector vec;
    LabelSet l1, l2, l3;
    l1.AddLabel("instance", "1");
    l2.AddLabel("instance", "2");
    l3.AddLabel("instance", "3");
    
    vec.push_back(Sample{l1, 1000, 30.0});
    vec.push_back(Sample{l2, 1000, 10.0});
    vec.push_back(Sample{l3, 1000, 20.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    
    Value result = func->implementation(args, nullptr);
    const auto& sorted = result.getVector();
    
    ASSERT_EQ(sorted.size(), 3);
    EXPECT_EQ(sorted[0].value, 30.0);
    EXPECT_EQ(sorted[1].value, 20.0);
    EXPECT_EQ(sorted[2].value, 10.0);
}

// Test clamp function
TEST(UtilityTest, Clamp) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("clamp");
    ASSERT_NE(func, nullptr);
    
    Vector vec;
    LabelSet labels;
    vec.push_back(Sample{labels, 1000, 5.0});
    vec.push_back(Sample{labels, 1000, 15.0});
    vec.push_back(Sample{labels, 1000, 25.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(Scalar{1000, 10.0})); // min
    args.push_back(Value(Scalar{1000, 20.0})); // max
    
    Value result = func->implementation(args, nullptr);
    const auto& clamped = result.getVector();
    
    ASSERT_EQ(clamped.size(), 3);
    EXPECT_EQ(clamped[0].value, 10.0); // 5 clamped to 10
    EXPECT_EQ(clamped[1].value, 15.0); // 15 unchanged
    EXPECT_EQ(clamped[2].value, 20.0); // 25 clamped to 20
}

// Test clamp_max function
TEST(UtilityTest, ClampMax) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("clamp_max");
    ASSERT_NE(func, nullptr);
    
    Vector vec;
    LabelSet labels;
    vec.push_back(Sample{labels, 1000, 5.0});
    vec.push_back(Sample{labels, 1000, 15.0});
    vec.push_back(Sample{labels, 1000, 25.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(Scalar{1000, 20.0})); // max
    
    Value result = func->implementation(args, nullptr);
    const auto& clamped = result.getVector();
    
    ASSERT_EQ(clamped.size(), 3);
    EXPECT_EQ(clamped[0].value, 5.0);  // unchanged
    EXPECT_EQ(clamped[1].value, 15.0); // unchanged
    EXPECT_EQ(clamped[2].value, 20.0); // 25 clamped to 20
}

// Test clamp_min function
TEST(UtilityTest, ClampMin) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("clamp_min");
    ASSERT_NE(func, nullptr);
    
    Vector vec;
    LabelSet labels;
    vec.push_back(Sample{labels, 1000, 5.0});
    vec.push_back(Sample{labels, 1000, 15.0});
    vec.push_back(Sample{labels, 1000, 25.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(Scalar{1000, 10.0})); // min
    
    Value result = func->implementation(args, nullptr);
    const auto& clamped = result.getVector();
    
    ASSERT_EQ(clamped.size(), 3);
    EXPECT_EQ(clamped[0].value, 10.0); // 5 clamped to 10
    EXPECT_EQ(clamped[1].value, 15.0); // unchanged
    EXPECT_EQ(clamped[2].value, 25.0); // unchanged
}

// Test vector function
TEST(UtilityTest, Vector) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("vector");
    ASSERT_NE(func, nullptr);
    
    std::vector<Value> args;
    args.push_back(Value(Scalar{1000, 42.0}));
    
    Value result = func->implementation(args, nullptr);
    ASSERT_TRUE(result.isVector());
    
    const auto& vec = result.getVector();
    ASSERT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0].value, 42.0);
    EXPECT_EQ(vec[0].timestamp, 1000);
}

// Test scalar function
TEST(UtilityTest, Scalar) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("scalar");
    ASSERT_NE(func, nullptr);
    
    // Create mock evaluator
    class MockEvaluator : public Evaluator {
    public:
        MockEvaluator() : Evaluator(1000, 300000, nullptr) {}
    };
    MockEvaluator eval;
    
    // Test with single-element vector
    Vector vec;
    LabelSet labels;
    vec.push_back(Sample{labels, 1000, 42.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    
    Value result = func->implementation(args, &eval);
    ASSERT_TRUE(result.isScalar());
    EXPECT_EQ(result.getScalar().value, 42.0);
    
    // Test with empty vector (should return NaN)
    Vector empty_vec;
    args.clear();
    args.push_back(Value(empty_vec));
    
    Value result2 = func->implementation(args, &eval);
    ASSERT_TRUE(result2.isScalar());
    EXPECT_TRUE(std::isnan(result2.getScalar().value));
    
    // Test with multi-element vector (should return NaN)
    Vector multi_vec;
    multi_vec.push_back(Sample{labels, 1000, 1.0});
    multi_vec.push_back(Sample{labels, 1000, 2.0});
    args.clear();
    args.push_back(Value(multi_vec));
    
    Value result3 = func->implementation(args, &eval);
    ASSERT_TRUE(result3.isScalar());
    EXPECT_TRUE(std::isnan(result3.getScalar().value));
}

// Test absent function
TEST(UtilityTest, Absent) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("absent");
    ASSERT_NE(func, nullptr);
    
    class MockEvaluator : public Evaluator {
    public:
        MockEvaluator() : Evaluator(1000, 300000, nullptr) {}
    };
    MockEvaluator eval;
    
    // Test with empty vector (should return 1)
    Vector empty_vec;
    std::vector<Value> args;
    args.push_back(Value(empty_vec));
    
    Value result = func->implementation(args, &eval);
    ASSERT_TRUE(result.isVector());
    const auto& vec1 = result.getVector();
    ASSERT_EQ(vec1.size(), 1);
    EXPECT_EQ(vec1[0].value, 1.0);
    
    // Test with non-empty vector (should return empty)
    Vector non_empty;
    LabelSet labels;
    non_empty.push_back(Sample{labels, 1000, 42.0});
    args.clear();
    args.push_back(Value(non_empty));
    
    Value result2 = func->implementation(args, &eval);
    ASSERT_TRUE(result2.isVector());
    EXPECT_TRUE(result2.getVector().empty());
}
