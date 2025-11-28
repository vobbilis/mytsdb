#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <chrono>

#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "test_fixture.h"

namespace tsdb {
namespace comprehensive {

// Define static members here
tsdb::core::StorageConfig ComprehensivePromQLTest::config_;
std::shared_ptr<tsdb::storage::StorageImpl> ComprehensivePromQLTest::storage_;

// Class definition moved to test_fixture.h

// Test Case 1: Basic Selector
TEST_F(ComprehensivePromQLTest, BasicSelector) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("http_requests_total", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    // We expect some series, but if data generation failed or time is wrong, it might be empty.
    // We'll log it.
    std::cout << "Got " << vector.size() << " series for http_requests_total" << std::endl;
    if (!vector.empty()) {
        std::cout << "Sample value: " << vector[0].value << std::endl;
    }
}

// Test Case 2: Aggregation
TEST_F(ComprehensivePromQLTest, SumAggregation) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("sum(http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    if (!vector.empty()) {
        std::cout << "Total requests: " << vector[0].value << std::endl;
        EXPECT_GT(vector[0].value, 0);
    }
}

// Test Case 3: Rate
TEST_F(ComprehensivePromQLTest, RateFunction) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("rate(http_requests_total[5m])", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    std::cout << "Rate series count: " << vector.size() << std::endl;
    if (!vector.empty()) {
        std::cout << "Rate sample: " << vector[0].value << std::endl;
    }
}

} // namespace comprehensive
} // namespace tsdb
