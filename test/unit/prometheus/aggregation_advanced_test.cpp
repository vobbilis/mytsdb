#include <gtest/gtest.h>
#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/promql/value.h"
#include <cmath>

using namespace tsdb::prometheus::promql;
using namespace tsdb::prometheus::model;
using tsdb::prometheus::LabelSet;

// Test stddev function
TEST(AggregationAdvancedTest, Stddev) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("stddev");
    ASSERT_NE(func, nullptr);
    
    // Create test vector
    Vector vec;
    LabelSet labels;
    labels.AddLabel("job", "test");
    
    vec.push_back(Sample{labels, 1000, 2.0});
    vec.push_back(Sample{labels, 1000, 4.0});
    vec.push_back(Sample{labels, 1000, 4.0});
    vec.push_back(Sample{labels, 1000, 4.0});
    vec.push_back(Sample{labels, 1000, 5.0});
    vec.push_back(Sample{labels, 1000, 5.0});
    vec.push_back(Sample{labels, 1000, 7.0});
    vec.push_back(Sample{labels, 1000, 9.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    
    Value result = func->implementation(args, nullptr);
    ASSERT_TRUE(result.isVector());
    
    const auto& result_vec = result.getVector();
    ASSERT_EQ(result_vec.size(), 1);
    
    // Expected stddev = 2.0
    EXPECT_NEAR(result_vec[0].value, 2.0, 0.01);
}

// Test stdvar function
TEST(AggregationAdvancedTest, Stdvar) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("stdvar");
    ASSERT_NE(func, nullptr);
    
    // Create test vector
    Vector vec;
    LabelSet labels;
    labels.AddLabel("job", "test");
    
    vec.push_back(Sample{labels, 1000, 2.0});
    vec.push_back(Sample{labels, 1000, 4.0});
    vec.push_back(Sample{labels, 1000, 4.0});
    vec.push_back(Sample{labels, 1000, 4.0});
    vec.push_back(Sample{labels, 1000, 5.0});
    vec.push_back(Sample{labels, 1000, 5.0});
    vec.push_back(Sample{labels, 1000, 7.0});
    vec.push_back(Sample{labels, 1000, 9.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    
    Value result = func->implementation(args, nullptr);
    ASSERT_TRUE(result.isVector());
    
    const auto& result_vec = result.getVector();
    ASSERT_EQ(result_vec.size(), 1);
    
    // Expected variance = 4.0
    EXPECT_NEAR(result_vec[0].value, 4.0, 0.01);
}

// Test topk function
TEST(AggregationAdvancedTest, Topk) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("topk");
    ASSERT_NE(func, nullptr);
    
    // Create test vector
    Vector vec;
    LabelSet labels1, labels2, labels3, labels4, labels5;
    labels1.AddLabel("instance", "1");
    labels2.AddLabel("instance", "2");
    labels3.AddLabel("instance", "3");
    labels4.AddLabel("instance", "4");
    labels5.AddLabel("instance", "5");
    
    vec.push_back(Sample{labels1, 1000, 10.0});
    vec.push_back(Sample{labels2, 1000, 20.0});
    vec.push_back(Sample{labels3, 1000, 30.0});
    vec.push_back(Sample{labels4, 1000, 40.0});
    vec.push_back(Sample{labels5, 1000, 50.0});
    
    std::vector<Value> args;
    args.push_back(Value(Scalar{1000, 3.0})); // k=3
    args.push_back(Value(vec));
    
    Value result = func->implementation(args, nullptr);
    ASSERT_TRUE(result.isVector());
    
    const auto& result_vec = result.getVector();
    ASSERT_EQ(result_vec.size(), 3);
    
    // Should return top 3: 50, 40, 30
    EXPECT_EQ(result_vec[0].value, 50.0);
    EXPECT_EQ(result_vec[1].value, 40.0);
    EXPECT_EQ(result_vec[2].value, 30.0);
}

// Test bottomk function
TEST(AggregationAdvancedTest, Bottomk) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("bottomk");
    ASSERT_NE(func, nullptr);
    
    // Create test vector
    Vector vec;
    LabelSet labels1, labels2, labels3, labels4, labels5;
    labels1.AddLabel("instance", "1");
    labels2.AddLabel("instance", "2");
    labels3.AddLabel("instance", "3");
    labels4.AddLabel("instance", "4");
    labels5.AddLabel("instance", "5");
    
    vec.push_back(Sample{labels1, 1000, 10.0});
    vec.push_back(Sample{labels2, 1000, 20.0});
    vec.push_back(Sample{labels3, 1000, 30.0});
    vec.push_back(Sample{labels4, 1000, 40.0});
    vec.push_back(Sample{labels5, 1000, 50.0});
    
    std::vector<Value> args;
    args.push_back(Value(Scalar{1000, 3.0})); // k=3
    args.push_back(Value(vec));
    
    Value result = func->implementation(args, nullptr);
    ASSERT_TRUE(result.isVector());
    
    const auto& result_vec = result.getVector();
    ASSERT_EQ(result_vec.size(), 3);
    
    // Should return bottom 3: 10, 20, 30
    EXPECT_EQ(result_vec[0].value, 10.0);
    EXPECT_EQ(result_vec[1].value, 20.0);
    EXPECT_EQ(result_vec[2].value, 30.0);
}

// Test quantile function
TEST(AggregationAdvancedTest, Quantile) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("quantile");
    ASSERT_NE(func, nullptr);
    
    // Create test vector with values 1-10
    Vector vec;
    LabelSet labels;
    labels.AddLabel("job", "test");
    
    for (int i = 1; i <= 10; i++) {
        vec.push_back(Sample{labels, 1000, static_cast<double>(i)});
    }
    
    // Test median (0.5 quantile)
    std::vector<Value> args;
    args.push_back(Value(Scalar{1000, 0.5})); // φ=0.5
    args.push_back(Value(vec));
    
    Value result = func->implementation(args, nullptr);
    ASSERT_TRUE(result.isVector());
    
    const auto& result_vec = result.getVector();
    ASSERT_EQ(result_vec.size(), 1);
    
    // Median of 1-10 should be 5.5
    EXPECT_NEAR(result_vec[0].value, 5.5, 0.01);
}

// Test quantile edge cases
TEST(AggregationAdvancedTest, QuantileEdgeCases) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("quantile");
    ASSERT_NE(func, nullptr);
    
    Vector vec;
    LabelSet labels;
    labels.AddLabel("job", "test");
    
    for (int i = 1; i <= 10; i++) {
        vec.push_back(Sample{labels, 1000, static_cast<double>(i)});
    }
    
    // Test 0.0 quantile (minimum)
    std::vector<Value> args1;
    args1.push_back(Value(Scalar{1000, 0.0}));
    args1.push_back(Value(vec));
    
    Value result1 = func->implementation(args1, nullptr);
    EXPECT_EQ(result1.getVector()[0].value, 1.0);
    
    // Test 1.0 quantile (maximum)
    std::vector<Value> args2;
    args2.push_back(Value(Scalar{1000, 1.0}));
    args2.push_back(Value(vec));
    
    Value result2 = func->implementation(args2, nullptr);
    EXPECT_EQ(result2.getVector()[0].value, 10.0);
    
    // Test 0.25 quantile
    std::vector<Value> args3;
    args3.push_back(Value(Scalar{1000, 0.25}));
    args3.push_back(Value(vec));
    
    Value result3 = func->implementation(args3, nullptr);
    EXPECT_NEAR(result3.getVector()[0].value, 3.25, 0.01);
    
    // Test 0.75 quantile
    std::vector<Value> args4;
    args4.push_back(Value(Scalar{1000, 0.75}));
    args4.push_back(Value(vec));
    
    Value result4 = func->implementation(args4, nullptr);
    EXPECT_NEAR(result4.getVector()[0].value, 7.75, 0.01);
}

// Test empty vector handling
TEST(AggregationAdvancedTest, EmptyVector) {
    auto& registry = FunctionRegistry::Instance();
    
    Vector empty_vec;
    
    // Test stddev with empty vector
    const auto* stddev_func = registry.Get("stddev");
    std::vector<Value> args1;
    args1.push_back(Value(empty_vec));
    Value result1 = stddev_func->implementation(args1, nullptr);
    EXPECT_TRUE(result1.getVector().empty());
    
    // Test topk with empty vector
    const auto* topk_func = registry.Get("topk");
    std::vector<Value> args2;
    args2.push_back(Value(Scalar{1000, 3.0}));
    args2.push_back(Value(empty_vec));
    Value result2 = topk_func->implementation(args2, nullptr);
    EXPECT_TRUE(result2.getVector().empty());
}
