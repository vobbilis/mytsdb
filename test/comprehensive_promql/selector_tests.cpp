#include "test_fixture.h"

namespace tsdb {
namespace comprehensive {

// 1. Exact Match: metric{label="value"}
TEST_F(ComprehensivePromQLTest, SelectorExactMatch) {
    int64_t query_time = GetQueryTime();
    // We know we have pods with service="frontend"
    auto result = ExecuteQuery("http_requests_total{service=\"frontend\"}", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    
    for (const auto& sample : vector) {
        // Verify label matches
        // We need access to labels. Sample has metric (Labels) if it's promql::Sample?
        // promql::Sample has `metric` which is `std::vector<Label>`.
        bool found = false;
        for (const auto& label : sample.metric.labels()) {
            if (label.first == "service" && label.second == "frontend") {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Result should have service='frontend'";
    }
}

// 2. Regex Match: metric{label=~"val.*"}
TEST_F(ComprehensivePromQLTest, SelectorRegexMatch) {
    int64_t query_time = GetQueryTime();
    // service starts with "front"
    auto result = ExecuteQuery("http_requests_total{service=~\"front.*\"}", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    
    for (const auto& sample : vector) {
        bool found = false;
        for (const auto& label : sample.metric.labels()) {
            if (label.first == "service" && label.second == "frontend") {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Result should match regex 'front.*'";
    }
}

// 3. Regex Non-Match: metric{label!~"val.*"}
TEST_F(ComprehensivePromQLTest, SelectorRegexNonMatch) {
    int64_t query_time = GetQueryTime();
    // service does NOT start with "front" (so backend, db, etc.)
    auto result = ExecuteQuery("http_requests_total{service!~\"front.*\"}", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    
    for (const auto& sample : vector) {
        for (const auto& label : sample.metric.labels()) {
            if (label.first == "service") {
                EXPECT_NE(label.second, "frontend");
            }
        }
    }
}

// 4. Not Equal: metric{label!="value"}
TEST_F(ComprehensivePromQLTest, SelectorNotEqual) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("http_requests_total{service!=\"frontend\"}", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    
    for (const auto& sample : vector) {
        for (const auto& label : sample.metric.labels()) {
            if (label.first == "service") {
                EXPECT_NE(label.second, "frontend");
            }
        }
    }
}

// 5. Multiple Matchers
TEST_F(ComprehensivePromQLTest, SelectorMultipleMatchers) {
    int64_t query_time = GetQueryTime();
    // service="frontend" AND method="GET"
    auto result = ExecuteQuery("http_requests_total{service=\"frontend\", method=\"GET\"}", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    
    for (const auto& sample : vector) {
        bool service_ok = false;
        bool method_ok = false;
        for (const auto& label : sample.metric.labels()) {
            if (label.first == "service" && label.second == "frontend") service_ok = true;
            if (label.first == "method" && label.second == "GET") method_ok = true;
        }
        EXPECT_TRUE(service_ok && method_ok);
    }
}

// 6. Empty Matcher (should be ignored or handled)
// metric{label=""} is valid in PromQL (matches empty label, which means label missing)
TEST_F(ComprehensivePromQLTest, SelectorEmptyMatcher) {
    int64_t query_time = GetQueryTime();
    // method="" should match nothing if all series have method="GET"
    auto result = ExecuteQuery("http_requests_total{method=\"\"}", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_EQ(vector.size(), 0) << "Should not match anything with empty method if all have it";
}

// 7. Missing Label (Not Equal Empty)
// metric{missing_label!=""} should match all
TEST_F(ComprehensivePromQLTest, SelectorMissingLabelNotEmpty) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("http_requests_total{non_existent_label!=\"\"}", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_EQ(vector.size(), 0) << "non_existent_label!=\"\" implies label must exist and not be empty";
}

// 8. Missing Label (Equal Empty)
// metric{missing_label=""} matches all
TEST_F(ComprehensivePromQLTest, SelectorMissingLabelEmpty) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("http_requests_total{non_existent_label=\"\"}", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0) << "Should match all series";
}

} // namespace comprehensive
} // namespace tsdb
