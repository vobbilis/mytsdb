#include "test_fixture.h"
#include <cmath>

namespace tsdb {
namespace comprehensive {

// 1. Sum Aggregation
TEST_F(ComprehensivePromQLTest, AggregationSum) {
    int64_t query_time = GetQueryTime();
    // sum(http_requests_total)
    auto result = ExecuteQuery("sum(http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    ASSERT_EQ(vector.size(), 1);
    EXPECT_GT(vector[0].value, 0);
    // We expect value to be roughly (num_pods * requests_per_pod)
    // 50 nodes * 20 pods = 1000 pods.
    // Each pod has requests.
}

// 2. Avg Aggregation
TEST_F(ComprehensivePromQLTest, AggregationAvg) {
    int64_t query_time = GetQueryTime();
    // avg(http_requests_total)
    auto result = ExecuteQuery("avg(http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    ASSERT_EQ(vector.size(), 1);
    EXPECT_GT(vector[0].value, 0);
}

// 3. Min Aggregation
TEST_F(ComprehensivePromQLTest, AggregationMin) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("min(http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    ASSERT_EQ(vector.size(), 1);
    EXPECT_GE(vector[0].value, 0);
}

// 4. Max Aggregation
TEST_F(ComprehensivePromQLTest, AggregationMax) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("max(http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    ASSERT_EQ(vector.size(), 1);
    EXPECT_GT(vector[0].value, 0);
}

// 5. Count Aggregation
TEST_F(ComprehensivePromQLTest, AggregationCount) {
    int64_t query_time = GetQueryTime();
    auto result = ExecuteQuery("count(http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    ASSERT_EQ(vector.size(), 1);
    // We generate 200 series (40 pods × 5 services)
    EXPECT_GE(vector[0].value, 180); 
    EXPECT_LE(vector[0].value, 220);
}

// 6. Grouping: BY (service)
TEST_F(ComprehensivePromQLTest, AggregationByService) {
    int64_t query_time = GetQueryTime();
    // sum by (service) (http_requests_total)
    auto result = ExecuteQuery("sum by (service) (http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    // Services: frontend, backend, db, cache, auth (5 services)
    EXPECT_EQ(vector.size(), 5);
    
    std::set<std::string> services;
    for (const auto& sample : vector) {
        // Should only have 'service' label
        EXPECT_EQ(sample.metric.labels().size(), 1);
        auto it = sample.metric.labels().find("service");
        ASSERT_NE(it, sample.metric.labels().end());
        services.insert(it->second);
        EXPECT_GT(sample.value, 0);
    }
    EXPECT_EQ(services.size(), 5);
    EXPECT_TRUE(services.count("frontend"));
    EXPECT_TRUE(services.count("backend"));
}

// 7. Grouping: WITHOUT (pod, node, instance, job, etc.)
// Hard to list all to exclude to get just service, but let's try excluding pod and node.
// http_requests_total has: pod, service, method, status, cluster, scope...
// If we exclude pod, we aggregate over pods.
TEST_F(ComprehensivePromQLTest, AggregationWithoutPod) {
    int64_t query_time = GetQueryTime();
    // sum without (pod) (http_requests_total)
    // Result should preserve service, method, status, cluster, etc.
    // And have fewer series than total (since many pods share same service/method/status).
    auto result = ExecuteQuery("sum without (pod) (http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_GT(vector.size(), 0);
    EXPECT_LT(vector.size(), 1000); // Should be aggregated
    
    for (const auto& sample : vector) {
        // Should NOT have 'pod' label
        bool has_pod = false;
        for (const auto& label : sample.metric.labels()) {
            if (label.first == "pod") has_pod = true;
        }
        EXPECT_FALSE(has_pod);
    }
}

// 8. Stddev (Standard Deviation)
// 8. Stddev (Standard Deviation)
// 8. Stddev (Standard Deviation)
TEST_F(ComprehensivePromQLTest, AggregationStdDevPushdown) {
    int64_t query_time = GetQueryTime();
    
    // 1. Get raw samples to calculate expected stddev
    auto raw_result = ExecuteQuery("http_requests_total", query_time);
    ASSERT_EQ(raw_result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto raw_vector = raw_result.getVector();
    ASSERT_GT(raw_vector.size(), 1) << "Need at least 2 samples for stddev";

    // Calculate expected stddev manually (Welford's or simple 2-pass)
    double mean = 0.0;
    for (const auto& s : raw_vector) {
        mean += s.value;
    }
    mean /= raw_vector.size();
    
    double variance = 0.0;
    for (const auto& s : raw_vector) {
        double diff = s.value - mean;
        variance += diff * diff;
    }
    // Prometheus uses population variance (divide by N)
    variance /= raw_vector.size();
    double expected_stddev = std::sqrt(variance);
    
    // 2. Execute pushdown query
    auto result = ExecuteQuery("stddev(http_requests_total)", query_time);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto results = result.getVector();
    ASSERT_EQ(results.size(), 1);
    
    // 3. Compare
    std::cout << "Expected StdDev: " << expected_stddev << ", Actual: " << results[0].value << std::endl;
    EXPECT_NEAR(results[0].value, expected_stddev, 0.001);
}

// 9. Quantile
TEST_F(ComprehensivePromQLTest, AggregationQuantilePushdown) {
    int64_t query_time = GetQueryTime();
    
    // 1. Get raw samples
    auto raw_result = ExecuteQuery("http_requests_total", query_time);
    ASSERT_EQ(raw_result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto raw_vector = raw_result.getVector();
    ASSERT_GT(raw_vector.size(), 1);

    // 2. Calculate expected quantile (0.9)
    std::vector<double> values;
    for (const auto& s : raw_vector) {
        values.push_back(s.value);
    }
    std::sort(values.begin(), values.end());
    
    double q = 0.9;
    double rank = q * (values.size() - 1);
    size_t lower_idx = static_cast<size_t>(std::floor(rank));
    size_t upper_idx = static_cast<size_t>(std::ceil(rank));
    double weight = rank - lower_idx;
    double expected_quantile = values[lower_idx] + weight * (values[upper_idx] - values[lower_idx]);
    
    // 3. Execute pushdown query
    auto result = ExecuteQuery("quantile(0.9, http_requests_total)", query_time);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto results = result.getVector();
    ASSERT_EQ(results.size(), 1);
    
    // 4. Compare
    std::cout << "Expected Quantile(0.9): " << expected_quantile << ", Actual: " << results[0].value << std::endl;
    EXPECT_NEAR(results[0].value, expected_quantile, 0.001);
}

// 9. TopK
TEST_F(ComprehensivePromQLTest, AggregationTopK) {
    int64_t query_time = GetQueryTime();
    // topk(3, http_requests_total)
    auto result = ExecuteQuery("topk(3, http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_LE(vector.size(), 3);
    EXPECT_GT(vector.size(), 0);
    
    // Check if sorted? PromQL topk doesn't guarantee sort order in result vector, 
    // but usually implementation might return them.
    // But values should be the highest.
}

// 10. BottomK
TEST_F(ComprehensivePromQLTest, AggregationBottomK) {
    int64_t query_time = GetQueryTime();
    // bottomk(3, http_requests_total)
    auto result = ExecuteQuery("bottomk(3, http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    EXPECT_LE(vector.size(), 3);
    EXPECT_GT(vector.size(), 0);
}

// 11. Quantile
TEST_F(ComprehensivePromQLTest, AggregationQuantile) {
    int64_t query_time = GetQueryTime();
    // quantile(0.95, http_requests_total)
    auto result = ExecuteQuery("quantile(0.95, http_requests_total)", query_time);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto vector = result.getVector();
    ASSERT_EQ(vector.size(), 1);
    EXPECT_GT(vector[0].value, 0);
}
// Benchmark
TEST_F(ComprehensivePromQLTest, BenchmarkAggregation) {
    int64_t query_time = GetQueryTime();
    
    // Warmup
    ExecuteQuery("http_requests_total", query_time);
    
    // 1. Raw Query (Baseline)
    auto start_raw = std::chrono::high_resolution_clock::now();
    auto raw_result = ExecuteQuery("http_requests_total", query_time);
    auto end_raw = std::chrono::high_resolution_clock::now();
    auto duration_raw = std::chrono::duration_cast<std::chrono::milliseconds>(end_raw - start_raw).count();
    
    // 2. Pushdown STDDEV
    auto start_stddev = std::chrono::high_resolution_clock::now();
    ExecuteQuery("stddev(http_requests_total)", query_time);
    auto end_stddev = std::chrono::high_resolution_clock::now();
    auto duration_stddev = std::chrono::duration_cast<std::chrono::milliseconds>(end_stddev - start_stddev).count();
    
    // 3. Pushdown QUANTILE
    auto start_quantile = std::chrono::high_resolution_clock::now();
    ExecuteQuery("quantile(0.9, http_requests_total)", query_time);
    auto end_quantile = std::chrono::high_resolution_clock::now();
    auto duration_quantile = std::chrono::duration_cast<std::chrono::milliseconds>(end_quantile - start_quantile).count();
    
    std::cout << "\n=== BENCHMARK RESULTS ===\n";
    std::cout << "Raw Fetch (http_requests_total): " << duration_raw << " ms\n";
    std::cout << "Pushdown STDDEV: " << duration_stddev << " ms\n";
    std::cout << "Pushdown QUANTILE: " << duration_quantile << " ms\n";
    std::cout << "=========================\n";
}

} // namespace comprehensive
} // namespace tsdb
