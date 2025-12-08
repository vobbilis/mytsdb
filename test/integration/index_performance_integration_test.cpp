/**
 * @file index_performance_integration_test.cpp
 * @brief Integration tests for Index performance with self-monitoring metrics
 * 
 * These tests simulate realistic workloads and validate that the Roaring Bitmap
 * implementation delivers the expected performance improvements.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "tsdb/storage/index.h"
#include "tsdb/storage/sharded_index.h"

using namespace tsdb::storage;
using namespace tsdb::core;

namespace {

// Simulates Kubernetes metric labels
Labels generate_k8s_labels(int pod_num, int container_num) {
    std::string namespace_name = "namespace-" + std::to_string(pod_num / 100);
    std::string pod_name = "pod-" + std::to_string(pod_num);
    std::string container_name = "container-" + std::to_string(container_num);
    std::string node_name = "node-" + std::to_string(pod_num % 10);
    
    return Labels(Labels::Map{
        {"__name__", "container_cpu_usage_seconds_total"},
        {"namespace", namespace_name},
        {"pod", pod_name},
        {"container", container_name},
        {"node", node_name},
        {"cluster", "production-cluster"},
        {"region", "us-east-1"},
        {"instance", node_name + ":9090"}
    });
}

// Simulates Prometheus HTTP server metrics
Labels generate_http_labels(int endpoint_num) {
    std::vector<std::string> methods = {"GET", "POST", "PUT", "DELETE", "PATCH"};
    std::vector<std::string> statuses = {"200", "201", "204", "400", "401", "403", "404", "500", "502", "503"};
    
    return Labels(Labels::Map{
        {"__name__", "http_requests_total"},
        {"method", methods[endpoint_num % methods.size()]},
        {"status", statuses[endpoint_num % statuses.size()]},
        {"endpoint", "/api/v1/resource/" + std::to_string(endpoint_num)},
        {"service", "api-server"},
        {"instance", "api-" + std::to_string(endpoint_num % 5) + ":8080"}
    });
}

struct BenchmarkResult {
    double write_rate;        // series/sec
    double query_rate;        // queries/sec
    double avg_query_time_ms;
    double p50_query_time_ms;
    double p99_query_time_ms;
    uint64_t total_adds;
    uint64_t total_lookups;
    uint64_t total_intersects;
    double avg_add_time_us;
    double avg_lookup_time_us;
    double avg_intersect_time_us;
};

void print_benchmark_result(const std::string& name, const BenchmarkResult& result) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Benchmark: " << name << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nThroughput:" << std::endl;
    std::cout << "  Write Rate:     " << result.write_rate << " series/sec" << std::endl;
    std::cout << "  Query Rate:     " << result.query_rate << " queries/sec" << std::endl;
    std::cout << "\nQuery Latency:" << std::endl;
    std::cout << "  Average:        " << result.avg_query_time_ms << " ms" << std::endl;
    std::cout << "  P50:            " << result.p50_query_time_ms << " ms" << std::endl;
    std::cout << "  P99:            " << result.p99_query_time_ms << " ms" << std::endl;
    std::cout << "\nIndex Metrics:" << std::endl;
    std::cout << "  Total Adds:     " << result.total_adds << std::endl;
    std::cout << "  Total Lookups:  " << result.total_lookups << std::endl;
    std::cout << "  Total Intersects: " << result.total_intersects << std::endl;
    std::cout << "\nPer-Operation Timing:" << std::endl;
    std::cout << "  Avg Add:        " << result.avg_add_time_us << " µs" << std::endl;
    std::cout << "  Avg Lookup:     " << result.avg_lookup_time_us << " µs" << std::endl;
    std::cout << "  Avg Intersect:  " << result.avg_intersect_time_us << " µs" << std::endl;
}

} // namespace

// ============================================================================
// Integration Tests
// ============================================================================

class IndexIntegrationTest : public ::testing::Test {
protected:
    ShardedIndex index_{16}; // 16 shards like production
};

TEST_F(IndexIntegrationTest, KubernetesMetricsWorkload) {
    // Simulate K8s cluster: 10 namespaces, 100 pods each, 3 containers per pod
    const int NUM_NAMESPACES = 10;
    const int PODS_PER_NAMESPACE = 100;
    const int CONTAINERS_PER_POD = 3;
    const int TOTAL_SERIES = NUM_NAMESPACES * PODS_PER_NAMESPACE * CONTAINERS_PER_POD;
    
    std::cout << "\n=== K8s Metrics Workload ===" << std::endl;
    std::cout << "Total series: " << TOTAL_SERIES << std::endl;
    
    // Phase 1: Add series
    auto start = std::chrono::high_resolution_clock::now();
    
    SeriesID id = 0;
    for (int ns = 0; ns < NUM_NAMESPACES; ++ns) {
        for (int pod = 0; pod < PODS_PER_NAMESPACE; ++pod) {
            for (int cont = 0; cont < CONTAINERS_PER_POD; ++cont) {
                auto labels = generate_k8s_labels(ns * PODS_PER_NAMESPACE + pod, cont);
                auto result = index_.add_series(id++, labels);
                ASSERT_TRUE(result.ok());
            }
        }
    }
    
    auto write_end = std::chrono::high_resolution_clock::now();
    double write_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        write_end - start).count() / 1000.0;
    
    // Phase 2: Run queries
    const int NUM_QUERIES = 500;
    std::vector<double> query_times;
    
    // Simulate realistic queries
    std::vector<std::vector<LabelMatcher>> queries = {
        // Query 1: All pods in a namespace
        {{MatcherType::Equal, "namespace", "namespace-5"}},
        
        // Query 2: Specific container across all pods
        {{MatcherType::Equal, "container", "container-0"}},
        
        // Query 3: All pods on a node
        {{MatcherType::Equal, "node", "node-3"}},
        
        // Query 4: Specific pod (point query)
        {{MatcherType::Equal, "namespace", "namespace-2"},
         {MatcherType::Equal, "pod", "pod-250"}},
        
        // Query 5: Cross-namespace container query
        {{MatcherType::Equal, "container", "container-1"},
         {MatcherType::Equal, "cluster", "production-cluster"}}
    };
    
    for (int i = 0; i < NUM_QUERIES; ++i) {
        const auto& matchers = queries[i % queries.size()];
        
        auto q_start = std::chrono::high_resolution_clock::now();
        auto result = index_.find_series(matchers);
        auto q_end = std::chrono::high_resolution_clock::now();
        
        ASSERT_TRUE(result.ok());
        query_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            q_end - q_start).count() / 1000.0);
    }
    
    // Calculate stats
    std::sort(query_times.begin(), query_times.end());
    double total_query_time = 0;
    for (double t : query_times) total_query_time += t;
    
    auto metrics = index_.get_aggregated_metrics();
    
    BenchmarkResult result;
    result.write_rate = TOTAL_SERIES / (write_time_ms / 1000.0);
    result.query_rate = NUM_QUERIES / (total_query_time / 1000.0);
    result.avg_query_time_ms = total_query_time / NUM_QUERIES;
    result.p50_query_time_ms = query_times[NUM_QUERIES / 2];
    result.p99_query_time_ms = query_times[static_cast<int>(NUM_QUERIES * 0.99)];
    result.total_adds = metrics.total_add_count;
    result.total_lookups = metrics.total_lookup_count;
    result.total_intersects = metrics.total_intersect_count;
    result.avg_add_time_us = metrics.avg_add_time_us();
    result.avg_lookup_time_us = metrics.avg_lookup_time_us();
    result.avg_intersect_time_us = metrics.avg_intersect_time_us();
    
    print_benchmark_result("Kubernetes Metrics Workload", result);
    
    // Assertions
    EXPECT_GT(result.write_rate, 10000) << "Write rate below 10K series/sec";
    EXPECT_LT(result.avg_query_time_ms, 10.0) << "Avg query time exceeds 10ms";
    EXPECT_LT(result.p99_query_time_ms, 50.0) << "P99 query time exceeds 50ms";
    EXPECT_LT(result.avg_lookup_time_us, 1000.0) << "Avg lookup time exceeds 1ms";
}

TEST_F(IndexIntegrationTest, HTTPMetricsWorkload) {
    const int NUM_SERIES = 10000;
    
    std::cout << "\n=== HTTP Metrics Workload ===" << std::endl;
    
    // Add series
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto labels = generate_http_labels(i);
        auto result = index_.add_series(i, labels);
        ASSERT_TRUE(result.ok());
    }
    
    auto write_end = std::chrono::high_resolution_clock::now();
    double write_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        write_end - start).count() / 1000.0;
    
    // Run queries
    const int NUM_QUERIES = 500;
    std::vector<double> query_times;
    
    for (int i = 0; i < NUM_QUERIES; ++i) {
        std::vector<LabelMatcher> matchers;
        
        switch (i % 5) {
            case 0: // All GET requests
                matchers = {{MatcherType::Equal, "method", "GET"}};
                break;
            case 1: // All 500 errors
                matchers = {{MatcherType::Equal, "status", "500"}};
                break;
            case 2: // GET with 200 status
                matchers = {{MatcherType::Equal, "method", "GET"},
                           {MatcherType::Equal, "status", "200"}};
                break;
            case 3: // All error statuses (4xx, 5xx)
                matchers = {{MatcherType::RegexMatch, "status", "4.*|5.*"}};
                break;
            case 4: // Specific endpoint
                matchers = {{MatcherType::Equal, "endpoint", "/api/v1/resource/" + std::to_string(i % 100)}};
                break;
        }
        
        auto q_start = std::chrono::high_resolution_clock::now();
        auto result = index_.find_series(matchers);
        auto q_end = std::chrono::high_resolution_clock::now();
        
        ASSERT_TRUE(result.ok());
        query_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            q_end - q_start).count() / 1000.0);
    }
    
    // Calculate stats
    std::sort(query_times.begin(), query_times.end());
    double total_query_time = 0;
    for (double t : query_times) total_query_time += t;
    
    auto metrics = index_.get_aggregated_metrics();
    
    BenchmarkResult result;
    result.write_rate = NUM_SERIES / (write_time_ms / 1000.0);
    result.query_rate = NUM_QUERIES / (total_query_time / 1000.0);
    result.avg_query_time_ms = total_query_time / NUM_QUERIES;
    result.p50_query_time_ms = query_times[NUM_QUERIES / 2];
    result.p99_query_time_ms = query_times[static_cast<int>(NUM_QUERIES * 0.99)];
    result.total_adds = metrics.total_add_count;
    result.total_lookups = metrics.total_lookup_count;
    result.total_intersects = metrics.total_intersect_count;
    result.avg_add_time_us = metrics.avg_add_time_us();
    result.avg_lookup_time_us = metrics.avg_lookup_time_us();
    result.avg_intersect_time_us = metrics.avg_intersect_time_us();
    
    print_benchmark_result("HTTP Metrics Workload", result);
    
    // Assertions
    EXPECT_GT(result.write_rate, 50000) << "Write rate below 50K series/sec";
    EXPECT_LT(result.p99_query_time_ms, 20.0) << "P99 query time exceeds 20ms";
}

TEST_F(IndexIntegrationTest, HighCardinalityLabels) {
    // Test with high cardinality labels (worst case for posting lists)
    const int NUM_SERIES = 50000;
    
    std::cout << "\n=== High Cardinality Workload ===" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        Labels labels(Labels::Map{
            {"__name__", "metric"},
            {"unique_id", std::to_string(i)},  // High cardinality
            {"bucket", std::to_string(i % 100)}  // Medium cardinality
        });
        auto result = index_.add_series(i, labels);
        ASSERT_TRUE(result.ok());
    }
    
    auto write_end = std::chrono::high_resolution_clock::now();
    double write_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        write_end - start).count() / 1000.0;
    
    // Test finding by bucket (should return 500 series each)
    const int NUM_QUERIES = 200;
    std::vector<double> query_times;
    
    for (int i = 0; i < NUM_QUERIES; ++i) {
        std::vector<LabelMatcher> matchers = {
            {MatcherType::Equal, "bucket", std::to_string(i % 100)}
        };
        
        auto q_start = std::chrono::high_resolution_clock::now();
        auto result = index_.find_series(matchers);
        auto q_end = std::chrono::high_resolution_clock::now();
        
        ASSERT_TRUE(result.ok());
        EXPECT_EQ(result.value().size(), 500);
        query_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            q_end - q_start).count() / 1000.0);
    }
    
    std::sort(query_times.begin(), query_times.end());
    double total_query_time = 0;
    for (double t : query_times) total_query_time += t;
    
    auto metrics = index_.get_aggregated_metrics();
    
    std::cout << "Series: " << NUM_SERIES << ", Write time: " << write_time_ms << " ms" << std::endl;
    std::cout << "Avg query time: " << (total_query_time / NUM_QUERIES) << " ms" << std::endl;
    std::cout << "P99 query time: " << query_times[static_cast<int>(NUM_QUERIES * 0.99)] << " ms" << std::endl;
    std::cout << "Avg lookup: " << metrics.avg_lookup_time_us() << " µs" << std::endl;
    
    EXPECT_LT(total_query_time / NUM_QUERIES, 5.0) << "Avg query time too slow";
}

TEST_F(IndexIntegrationTest, FindSeriesWithLabelsPerformance) {
    // Test the optimized find_series_with_labels path
    const int NUM_SERIES = 10000;
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto labels = generate_k8s_labels(i, i % 3);
        index_.add_series(i, labels);
    }
    
    const int NUM_QUERIES = 100;
    
    // Compare find_series + get_labels vs find_series_with_labels
    std::vector<double> separate_times, combined_times;
    
    for (int i = 0; i < NUM_QUERIES; ++i) {
        std::vector<LabelMatcher> matchers = {
            {MatcherType::Equal, "node", "node-" + std::to_string(i % 10)}
        };
        
        // Method 1: find_series then get_labels separately
        auto start1 = std::chrono::high_resolution_clock::now();
        auto ids = index_.find_series(matchers);
        if (ids.ok()) {
            for (auto id : ids.value()) {
                auto labels = index_.get_labels(id);
            }
        }
        auto end1 = std::chrono::high_resolution_clock::now();
        separate_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            end1 - start1).count() / 1000.0);
        
        // Method 2: find_series_with_labels
        auto start2 = std::chrono::high_resolution_clock::now();
        auto combined = index_.find_series_with_labels(matchers);
        auto end2 = std::chrono::high_resolution_clock::now();
        combined_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            end2 - start2).count() / 1000.0);
    }
    
    double avg_separate = 0, avg_combined = 0;
    for (int i = 0; i < NUM_QUERIES; ++i) {
        avg_separate += separate_times[i];
        avg_combined += combined_times[i];
    }
    avg_separate /= NUM_QUERIES;
    avg_combined /= NUM_QUERIES;
    
    std::cout << "\n=== find_series_with_labels Comparison ===" << std::endl;
    std::cout << "Separate (find + get_labels): " << avg_separate << " ms avg" << std::endl;
    std::cout << "Combined (find_series_with_labels): " << avg_combined << " ms avg" << std::endl;
    std::cout << "Speedup: " << (avg_separate / avg_combined) << "x" << std::endl;
    
    // Combined should be faster (or at worst, the same)
    EXPECT_LE(avg_combined, avg_separate * 1.5);
}

TEST_F(IndexIntegrationTest, ConcurrentAccess) {
    // Test concurrent read/write access
    const int NUM_WRITERS = 2;
    const int NUM_READERS = 4;
    const int SERIES_PER_WRITER = 5000;
    const int QUERIES_PER_READER = 500;
    
    std::atomic<bool> start_flag{false};
    std::atomic<int> series_added{0};
    std::atomic<int> queries_completed{0};
    std::atomic<int> query_errors{0};
    
    std::vector<std::thread> threads;
    
    // Writer threads
    for (int w = 0; w < NUM_WRITERS; ++w) {
        threads.emplace_back([this, w, &start_flag, &series_added, SERIES_PER_WRITER]() {
            while (!start_flag.load()) std::this_thread::yield();
            
            for (int i = 0; i < SERIES_PER_WRITER; ++i) {
                SeriesID id = w * SERIES_PER_WRITER + i;
                Labels labels(Labels::Map{
                    {"__name__", "concurrent_metric"},
                    {"writer", std::to_string(w)},
                    {"id", std::to_string(id)}
                });
                index_.add_series(id, labels);
                series_added++;
            }
        });
    }
    
    // Reader threads
    for (int r = 0; r < NUM_READERS; ++r) {
        threads.emplace_back([this, r, &start_flag, &queries_completed, &query_errors, 
                              QUERIES_PER_READER, NUM_WRITERS]() {
            while (!start_flag.load()) std::this_thread::yield();
            
            for (int i = 0; i < QUERIES_PER_READER; ++i) {
                std::vector<LabelMatcher> matchers = {
                    {MatcherType::Equal, "writer", std::to_string(i % NUM_WRITERS)}
                };
                
                auto result = index_.find_series(matchers);
                if (!result.ok()) {
                    query_errors++;
                }
                queries_completed++;
            }
        });
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    start_flag = true;
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    
    std::cout << "\n=== Concurrent Access Test ===" << std::endl;
    std::cout << "Writers: " << NUM_WRITERS << ", Readers: " << NUM_READERS << std::endl;
    std::cout << "Series added: " << series_added.load() << std::endl;
    std::cout << "Queries completed: " << queries_completed.load() << std::endl;
    std::cout << "Query errors: " << query_errors.load() << std::endl;
    std::cout << "Total time: " << elapsed_ms << " ms" << std::endl;
    std::cout << "Write throughput: " << (series_added.load() / (elapsed_ms / 1000.0)) << " series/sec" << std::endl;
    std::cout << "Query throughput: " << (queries_completed.load() / (elapsed_ms / 1000.0)) << " queries/sec" << std::endl;
    
    EXPECT_EQ(query_errors.load(), 0) << "Query errors during concurrent access";
    EXPECT_EQ(series_added.load(), NUM_WRITERS * SERIES_PER_WRITER);
    EXPECT_EQ(queries_completed.load(), NUM_READERS * QUERIES_PER_READER);
}

// Test that validates index metrics are being tracked correctly
TEST_F(IndexIntegrationTest, MetricsAccuracyValidation) {
    const int NUM_SERIES = 1000;
    const int NUM_QUERIES = 100;
    
    // Track operations ourselves
    int expected_adds = 0;
    int expected_lookups = 0;
    
    index_.reset_metrics();
    
    // Add series
    for (int i = 0; i < NUM_SERIES; ++i) {
        Labels labels(Labels::Map{
            {"__name__", "test"},
            {"group", std::to_string(i % 10)}
        });
        index_.add_series(i, labels);
        expected_adds++;
    }
    
    // Perform queries
    for (int i = 0; i < NUM_QUERIES; ++i) {
        std::vector<LabelMatcher> matchers = {
            {MatcherType::Equal, "group", std::to_string(i % 10)}
        };
        index_.find_series(matchers);
        expected_lookups++;
    }
    
    // Validate metrics
    auto metrics = index_.get_aggregated_metrics();
    
    std::cout << "\n=== Metrics Accuracy Validation ===" << std::endl;
    std::cout << "Expected adds: " << expected_adds << ", Recorded: " << metrics.total_add_count << std::endl;
    std::cout << "Expected lookups: " << expected_lookups << ", Recorded: " << metrics.total_lookup_count << std::endl;
    
    // Each shard tracks its own adds/lookups, so we expect the total to match
    // Note: add_count is per-shard adds, not global. Each add goes to one shard.
    EXPECT_EQ(metrics.total_add_count, expected_adds) << "Add count mismatch";
    
    // Each query goes to ALL shards (scatter-gather), so lookups = queries * num_shards
    EXPECT_GE(metrics.total_lookup_count, expected_lookups) << "Lookup count too low";
    
    // Validate timing is non-zero
    EXPECT_GT(metrics.total_add_time_us, 0) << "Add timing not recorded";
    EXPECT_GT(metrics.total_lookup_time_us, 0) << "Lookup timing not recorded";
}

// Main entry point

