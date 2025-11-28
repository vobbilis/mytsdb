#include <gtest/gtest.h>
#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/promql/value.h"

using namespace tsdb::prometheus::promql;
using namespace tsdb::prometheus::model;
using tsdb::prometheus::LabelSet;

// Test label_replace function
TEST(LabelManipulationTest, LabelReplace) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("label_replace");
    ASSERT_NE(func, nullptr);
    
    // Create test vector
    Vector vec;
    LabelSet labels1, labels2;
    labels1.AddLabel("instance", "localhost:9090");
    labels1.AddLabel("job", "prometheus");
    labels2.AddLabel("instance", "localhost:8080");
    labels2.AddLabel("job", "node_exporter");
    
    vec.push_back(Sample{labels1, 1000, 100.0});
    vec.push_back(Sample{labels2, 1000, 200.0});
    
    // Test: Extract port from instance label
    // Regex: localhost:(.*)
    // Replacement: $1
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(String{1000, "port"}));           // dst_label
    args.push_back(Value(String{1000, "$1"}));             // replacement
    args.push_back(Value(String{1000, "instance"}));       // src_label
    args.push_back(Value(String{1000, "localhost:(.*)"})); // regex
    
    Value result = func->implementation(args, nullptr);
    ASSERT_TRUE(result.isVector());
    
    const auto& result_vec = result.getVector();
    ASSERT_EQ(result_vec.size(), 2);
    
    // Check that port label was added
    auto port1 = result_vec[0].metric.GetLabelValue("port");
    auto port2 = result_vec[1].metric.GetLabelValue("port");
    
    ASSERT_TRUE(port1.has_value());
    ASSERT_TRUE(port2.has_value());
    EXPECT_EQ(*port1, "9090");
    EXPECT_EQ(*port2, "8080");
    
    // Original labels should still be present
    EXPECT_TRUE(result_vec[0].metric.GetLabelValue("instance").has_value());
    EXPECT_TRUE(result_vec[0].metric.GetLabelValue("job").has_value());
}

// Test label_replace with no match
TEST(LabelManipulationTest, LabelReplaceNoMatch) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("label_replace");
    
    Vector vec;
    LabelSet labels;
    labels.AddLabel("instance", "localhost:9090");
    vec.push_back(Sample{labels, 1000, 100.0});
    
    // Regex that doesn't match
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(String{1000, "port"}));
    args.push_back(Value(String{1000, "$1"}));
    args.push_back(Value(String{1000, "instance"}));
    args.push_back(Value(String{1000, "nomatch:(.*)"})); // Won't match
    
    Value result = func->implementation(args, nullptr);
    const auto& result_vec = result.getVector();
    
    // Port label should not be added when regex doesn't match
    auto port = result_vec[0].metric.GetLabelValue("port");
    EXPECT_FALSE(port.has_value());
}

// Test label_join function
TEST(LabelManipulationTest, LabelJoin) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("label_join");
    ASSERT_NE(func, nullptr);
    
    // Create test vector
    Vector vec;
    LabelSet labels;
    labels.AddLabel("job", "prometheus");
    labels.AddLabel("instance", "localhost");
    labels.AddLabel("port", "9090");
    
    vec.push_back(Sample{labels, 1000, 100.0});
    
    // Join job, instance, and port with ":"
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(String{1000, "endpoint"}));    // dst_label
    args.push_back(Value(String{1000, ":"}));           // separator
    args.push_back(Value(String{1000, "job"}));         // src_label_1
    args.push_back(Value(String{1000, "instance"}));    // src_label_2
    args.push_back(Value(String{1000, "port"}));        // src_label_3
    
    Value result = func->implementation(args, nullptr);
    ASSERT_TRUE(result.isVector());
    
    const auto& result_vec = result.getVector();
    ASSERT_EQ(result_vec.size(), 1);
    
    // Check joined label
    auto endpoint = result_vec[0].metric.GetLabelValue("endpoint");
    ASSERT_TRUE(endpoint.has_value());
    EXPECT_EQ(*endpoint, "prometheus:localhost:9090");
    
    // Original labels should still be present
    EXPECT_TRUE(result_vec[0].metric.GetLabelValue("job").has_value());
    EXPECT_TRUE(result_vec[0].metric.GetLabelValue("instance").has_value());
    EXPECT_TRUE(result_vec[0].metric.GetLabelValue("port").has_value());
}

// Test label_join with missing labels
TEST(LabelManipulationTest, LabelJoinMissingLabels) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("label_join");
    
    Vector vec;
    LabelSet labels;
    labels.AddLabel("job", "prometheus");
    // instance label is missing
    labels.AddLabel("port", "9090");
    
    vec.push_back(Sample{labels, 1000, 100.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(String{1000, "endpoint"}));
    args.push_back(Value(String{1000, ":"}));
    args.push_back(Value(String{1000, "job"}));
    args.push_back(Value(String{1000, "instance"}));  // Missing
    args.push_back(Value(String{1000, "port"}));
    
    Value result = func->implementation(args, nullptr);
    const auto& result_vec = result.getVector();
    
    // Should join with empty string for missing label
    auto endpoint = result_vec[0].metric.GetLabelValue("endpoint");
    ASSERT_TRUE(endpoint.has_value());
    EXPECT_EQ(*endpoint, "prometheus::9090");  // Empty between ::
}

// Test label_join with single label
TEST(LabelManipulationTest, LabelJoinSingleLabel) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("label_join");
    
    Vector vec;
    LabelSet labels;
    labels.AddLabel("job", "prometheus");
    vec.push_back(Sample{labels, 1000, 100.0});
    
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(String{1000, "copy"}));
    args.push_back(Value(String{1000, "-"}));
    args.push_back(Value(String{1000, "job"}));
    
    Value result = func->implementation(args, nullptr);
    const auto& result_vec = result.getVector();
    
    auto copy = result_vec[0].metric.GetLabelValue("copy");
    ASSERT_TRUE(copy.has_value());
    EXPECT_EQ(*copy, "prometheus");
}

// Test label_replace with multiple capture groups
TEST(LabelManipulationTest, LabelReplaceMultipleGroups) {
    auto& registry = FunctionRegistry::Instance();
    const auto* func = registry.Get("label_replace");
    
    Vector vec;
    LabelSet labels;
    labels.AddLabel("path", "/api/v1/query");
    vec.push_back(Sample{labels, 1000, 100.0});
    
    // Extract version and endpoint: /api/(.*)/(.*)
    std::vector<Value> args;
    args.push_back(Value(vec));
    args.push_back(Value(String{1000, "info"}));
    args.push_back(Value(String{1000, "version=$1,endpoint=$2"}));
    args.push_back(Value(String{1000, "path"}));
    args.push_back(Value(String{1000, "/api/(.*)/(.*)"}));
    
    Value result = func->implementation(args, nullptr);
    const auto& result_vec = result.getVector();
    
    auto info = result_vec[0].metric.GetLabelValue("info");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(*info, "version=v1,endpoint=query");
}
