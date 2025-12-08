/**
 * @file index_roaring_test.cpp
 * @brief Unit tests for Roaring Bitmap Index implementation
 * 
 * These tests verify the performance improvements from Roaring Bitmaps
 * by measuring actual operation times and comparing against baselines.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <iostream>
#include <iomanip>
#include <numeric>

#include "tsdb/storage/index.h"
#include "tsdb/storage/sharded_index.h"

using namespace tsdb::storage;
using namespace tsdb::core;

namespace {

// Helper to generate labels using add()
Labels generate_labels(int series_num) {
    Labels labels;
    labels.add("__name__", "test_metric_" + std::to_string(series_num % 100));
    labels.add("instance", "host-" + std::to_string(series_num % 50) + ".example.com:9090");
    labels.add("job", "job_" + std::to_string(series_num % 10));
    labels.add("env", series_num % 3 == 0 ? "production" : (series_num % 3 == 1 ? "staging" : "development"));
    labels.add("region", "region-" + std::to_string(series_num % 5));
    return labels;
}

// Helper for timing
class Timer {
public:
    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed_us() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
    }
    
    double elapsed_ms() const {
        return elapsed_us() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Print metrics summary
void print_metrics_summary(const std::string& title, const PerIndexMetrics& metrics) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    auto add_count = metrics.add_count.load();
    auto lookup_count = metrics.lookup_count.load();
    auto intersect_count = metrics.intersect_count.load();
    
    std::cout << "Operations:" << std::endl;
    std::cout << "  - Add:       " << add_count << std::endl;
    std::cout << "  - Lookup:    " << lookup_count << std::endl;
    std::cout << "  - Intersect: " << intersect_count << std::endl;
    
    if (add_count > 0) {
        double avg_add_us = static_cast<double>(metrics.add_time_us.load()) / add_count;
        std::cout << "Avg Add Time:       " << avg_add_us << " µs" << std::endl;
    }
    if (lookup_count > 0) {
        double avg_lookup_us = static_cast<double>(metrics.lookup_time_us.load()) / lookup_count;
        std::cout << "Avg Lookup Time:    " << avg_lookup_us << " µs" << std::endl;
    }
    if (intersect_count > 0) {
        double avg_intersect_us = static_cast<double>(metrics.intersect_time_us.load()) / intersect_count;
        std::cout << "Avg Intersect Time: " << avg_intersect_us << " µs" << std::endl;
    }
}

} // namespace

// ============================================================================
// Basic Functionality Tests
// ============================================================================

class IndexBasicTest : public ::testing::Test {
protected:
    Index index_;
};

TEST_F(IndexBasicTest, AddAndFindSingleSeries) {
    Labels labels;
    labels.add("__name__", "test_metric");
    labels.add("instance", "host1");
    
    auto add_result = index_.add_series(1, labels);
    ASSERT_TRUE(add_result.ok());
    
    // Find by exact match
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "__name__", "test_metric")
    };
    
    auto find_result = index_.find_series(matchers);
    ASSERT_TRUE(find_result.ok());
    ASSERT_EQ(find_result.value().size(), 1);
    EXPECT_EQ(find_result.value()[0], 1);
}

TEST_F(IndexBasicTest, AddAndFindMultipleSeries) {
    // Add 100 series
    for (int i = 0; i < 100; ++i) {
        auto labels = generate_labels(i);
        auto result = index_.add_series(i, labels);
        ASSERT_TRUE(result.ok());
    }
    
    // Find all series with job=job_0
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "job", "job_0")
    };
    
    auto find_result = index_.find_series(matchers);
    ASSERT_TRUE(find_result.ok());
    EXPECT_EQ(find_result.value().size(), 10); // Every 10th series
}

TEST_F(IndexBasicTest, MultiLabelIntersection) {
    // Add 100 series
    for (int i = 0; i < 100; ++i) {
        auto labels = generate_labels(i);
        auto result = index_.add_series(i, labels);
        ASSERT_TRUE(result.ok());
    }
    
    // Find series with job=job_0 AND env=production
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "job", "job_0"),
        LabelMatcher(MatcherType::Equal, "env", "production")
    };
    
    auto find_result = index_.find_series(matchers);
    ASSERT_TRUE(find_result.ok());
    // job_0 = every 10th, production = every 3rd starting at 0
    EXPECT_GE(find_result.value().size(), 3);
}

TEST_F(IndexBasicTest, NotEqualMatcher) {
    // Add series with different environments
    for (int i = 0; i < 30; ++i) {
        Labels labels;
        labels.add("__name__", "metric");
        labels.add("env", i % 3 == 0 ? "production" : (i % 3 == 1 ? "staging" : "development"));
        auto result = index_.add_series(i, labels);
        ASSERT_TRUE(result.ok());
    }
    
    // Find all series where env != production
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "__name__", "metric"),
        LabelMatcher(MatcherType::NotEqual, "env", "production")
    };
    
    auto find_result = index_.find_series(matchers);
    ASSERT_TRUE(find_result.ok());
    EXPECT_EQ(find_result.value().size(), 20); // 30 - 10 production
}

TEST_F(IndexBasicTest, GetLabels) {
    Labels labels;
    labels.add("__name__", "test");
    labels.add("foo", "bar");
    
    auto add_result = index_.add_series(42, labels);
    ASSERT_TRUE(add_result.ok());
    
    auto get_result = index_.get_labels(42);
    ASSERT_TRUE(get_result.ok());
    EXPECT_EQ(get_result.value().size(), 2);
}

TEST_F(IndexBasicTest, RemoveSeries) {
    Labels labels;
    labels.add("__name__", "test");
    
    auto add_result = index_.add_series(1, labels);
    ASSERT_TRUE(add_result.ok());
    
    auto remove_result = index_.remove_series(1);
    ASSERT_TRUE(remove_result.ok());
    
    // Should not find removed series
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "__name__", "test")
    };
    
    auto find_result = index_.find_series(matchers);
    ASSERT_TRUE(find_result.ok());
    EXPECT_EQ(find_result.value().size(), 0);
}

// ============================================================================
// Performance Tests with Metrics Validation
// ============================================================================

class IndexPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        index_.get_metrics().reset();
    }
    
    Index index_;
};

TEST_F(IndexPerformanceTest, AddPerformance) {
    const int NUM_SERIES = 10000;
    Timer timer;
    
    timer.start();
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto labels = generate_labels(i);
        auto result = index_.add_series(i, labels);
        ASSERT_TRUE(result.ok());
    }
    double total_time_ms = timer.elapsed_ms();
    
    const auto& metrics = index_.get_metrics();
    
    std::cout << "\n=== Add Performance Test ===" << std::endl;
    std::cout << "Added " << NUM_SERIES << " series in " << total_time_ms << " ms" << std::endl;
    std::cout << "Rate: " << (NUM_SERIES / total_time_ms * 1000) << " series/sec" << std::endl;
    
    print_metrics_summary("Index Add Metrics", metrics);
    
    EXPECT_EQ(metrics.add_count.load(), NUM_SERIES);
    EXPECT_GT(metrics.add_time_us.load(), 0);
    
    double avg_add_us = static_cast<double>(metrics.add_time_us.load()) / NUM_SERIES;
    std::cout << "Average add time: " << avg_add_us << " µs/series" << std::endl;
    
    // Target: < 100 µs per add
    EXPECT_LT(avg_add_us, 100) << "Add performance is below target";
}

TEST_F(IndexPerformanceTest, LookupPerformance) {
    const int NUM_SERIES = 10000;
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto labels = generate_labels(i);
        index_.add_series(i, labels);
    }
    
    index_.get_metrics().reset();
    
    const int NUM_LOOKUPS = 1000;
    Timer timer;
    
    timer.start();
    for (int i = 0; i < NUM_LOOKUPS; ++i) {
        std::vector<LabelMatcher> matchers = {
            LabelMatcher(MatcherType::Equal, "job", "job_" + std::to_string(i % 10))
        };
        auto result = index_.find_series(matchers);
        ASSERT_TRUE(result.ok());
    }
    double total_time_ms = timer.elapsed_ms();
    
    const auto& metrics = index_.get_metrics();
    
    std::cout << "\n=== Lookup Performance Test ===" << std::endl;
    std::cout << "Performed " << NUM_LOOKUPS << " lookups in " << total_time_ms << " ms" << std::endl;
    std::cout << "Rate: " << (NUM_LOOKUPS / total_time_ms * 1000) << " lookups/sec" << std::endl;
    
    print_metrics_summary("Index Lookup Metrics", metrics);
    
    EXPECT_EQ(metrics.lookup_count.load(), NUM_LOOKUPS);
    
    double avg_lookup_us = static_cast<double>(metrics.lookup_time_us.load()) / NUM_LOOKUPS;
    std::cout << "Average lookup time: " << avg_lookup_us << " µs/lookup" << std::endl;
    
    // Target: < 500 µs per lookup
    EXPECT_LT(avg_lookup_us, 500) << "Lookup performance is below target";
}

TEST_F(IndexPerformanceTest, IntersectionPerformance) {
    const int NUM_SERIES = 10000;
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto labels = generate_labels(i);
        index_.add_series(i, labels);
    }
    
    index_.get_metrics().reset();
    
    const int NUM_QUERIES = 100;
    Timer timer;
    
    timer.start();
    for (int i = 0; i < NUM_QUERIES; ++i) {
        std::vector<LabelMatcher> matchers = {
            LabelMatcher(MatcherType::Equal, "job", "job_" + std::to_string(i % 10)),
            LabelMatcher(MatcherType::Equal, "env", "production"),
            LabelMatcher(MatcherType::Equal, "region", "region-" + std::to_string(i % 5))
        };
        auto result = index_.find_series(matchers);
        ASSERT_TRUE(result.ok());
    }
    double total_time_ms = timer.elapsed_ms();
    
    const auto& metrics = index_.get_metrics();
    
    std::cout << "\n=== Intersection Performance Test ===" << std::endl;
    std::cout << "Performed " << NUM_QUERIES << " 3-way intersections in " << total_time_ms << " ms" << std::endl;
    std::cout << "Rate: " << (NUM_QUERIES / total_time_ms * 1000) << " queries/sec" << std::endl;
    
    print_metrics_summary("Index Intersection Metrics", metrics);
    
    EXPECT_GT(metrics.intersect_count.load(), 0);
    
    double avg_query_ms = total_time_ms / NUM_QUERIES;
    std::cout << "Average query time: " << avg_query_ms << " ms/query" << std::endl;
    
    // Target: < 5 ms per complex query
    EXPECT_LT(avg_query_ms, 5.0) << "Intersection performance is below target";
}

// ============================================================================
// ShardedIndex Tests
// ============================================================================

class ShardedIndexTest : public ::testing::Test {
protected:
    ShardedIndex sharded_index_{16};
};

TEST_F(ShardedIndexTest, BasicFunctionality) {
    Labels labels;
    labels.add("__name__", "test");
    labels.add("job", "test_job");
    
    auto add_result = sharded_index_.add_series(1, labels);
    ASSERT_TRUE(add_result.ok());
    
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "__name__", "test")
    };
    
    auto find_result = sharded_index_.find_series(matchers);
    ASSERT_TRUE(find_result.ok());
    EXPECT_EQ(find_result.value().size(), 1);
}

TEST_F(ShardedIndexTest, AggregatedMetrics) {
    const int NUM_SERIES = 1000;
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto labels = generate_labels(i);
        sharded_index_.add_series(i, labels);
    }
    
    for (int i = 0; i < 100; ++i) {
        std::vector<LabelMatcher> matchers = {
            LabelMatcher(MatcherType::Equal, "job", "job_" + std::to_string(i % 10))
        };
        sharded_index_.find_series(matchers);
    }
    
    auto agg_metrics = sharded_index_.get_aggregated_metrics();
    
    std::cout << "\n=== Sharded Index Aggregated Metrics ===" << std::endl;
    std::cout << "Total Adds:       " << agg_metrics.total_add_count << std::endl;
    std::cout << "Total Lookups:    " << agg_metrics.total_lookup_count << std::endl;
    std::cout << "Total Intersects: " << agg_metrics.total_intersect_count << std::endl;
    std::cout << "Avg Add Time:     " << agg_metrics.avg_add_time_us() << " µs" << std::endl;
    std::cout << "Avg Lookup Time:  " << agg_metrics.avg_lookup_time_us() << " µs" << std::endl;
    
    EXPECT_EQ(agg_metrics.total_add_count, NUM_SERIES);
    EXPECT_GT(agg_metrics.total_lookup_count, 0);
}

TEST_F(ShardedIndexTest, LargeScalePerformance) {
    const int NUM_SERIES = 50000;
    Timer timer;
    
    std::cout << "\n=== Sharded Index Large Scale Test ===" << std::endl;
    
    timer.start();
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto labels = generate_labels(i);
        sharded_index_.add_series(i, labels);
    }
    double add_time_ms = timer.elapsed_ms();
    std::cout << "Added " << NUM_SERIES << " series in " << add_time_ms << " ms" << std::endl;
    std::cout << "Add rate: " << (NUM_SERIES / add_time_ms * 1000) << " series/sec" << std::endl;
    
    const int NUM_QUERIES = 100;
    std::vector<double> query_times;
    
    for (int i = 0; i < NUM_QUERIES; ++i) {
        std::vector<LabelMatcher> matchers = {
            LabelMatcher(MatcherType::Equal, "job", "job_" + std::to_string(i % 10)),
            LabelMatcher(MatcherType::Equal, "env", "production")
        };
        
        timer.start();
        auto result = sharded_index_.find_series(matchers);
        query_times.push_back(timer.elapsed_ms());
        ASSERT_TRUE(result.ok());
    }
    
    double total_query_time = std::accumulate(query_times.begin(), query_times.end(), 0.0);
    double avg_query_time = total_query_time / NUM_QUERIES;
    std::sort(query_times.begin(), query_times.end());
    double p50 = query_times[NUM_QUERIES / 2];
    double p99 = query_times[static_cast<int>(NUM_QUERIES * 0.99)];
    
    std::cout << "\nQuery Performance (ms):" << std::endl;
    std::cout << "  Avg: " << avg_query_time << std::endl;
    std::cout << "  P50: " << p50 << std::endl;
    std::cout << "  P99: " << p99 << std::endl;
    
    auto metrics = sharded_index_.get_aggregated_metrics();
    std::cout << "\nAggregated Metrics:" << std::endl;
    std::cout << "  Avg Add:       " << metrics.avg_add_time_us() << " µs" << std::endl;
    std::cout << "  Avg Lookup:    " << metrics.avg_lookup_time_us() << " µs" << std::endl;
    std::cout << "  Avg Intersect: " << metrics.avg_intersect_time_us() << " µs" << std::endl;
    
    EXPECT_LT(avg_query_time, 10.0) << "Avg query time exceeds 10ms target";
    EXPECT_LT(p99, 50.0) << "P99 query time exceeds 50ms target";
}

// ============================================================================
// Roaring Bitmap Specific Tests
// ============================================================================

#ifdef HAVE_ROARING
TEST(RoaringSpecificTest, LargePostingListPerformance) {
    Index index;
    
    const int NUM_SERIES = 100000;
    for (int i = 0; i < NUM_SERIES; ++i) {
        Labels labels;
        labels.add("__name__", "metric");
        labels.add("id", std::to_string(i));
        index.add_series(i, labels);
    }
    
    index.get_metrics().reset();
    Timer timer;
    
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "__name__", "metric")
    };
    
    timer.start();
    auto result = index.find_series(matchers);
    double query_time_ms = timer.elapsed_ms();
    
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), NUM_SERIES);
    
    std::cout << "\n=== Large Posting List Test ===" << std::endl;
    std::cout << "Found " << NUM_SERIES << " series in " << query_time_ms << " ms" << std::endl;
    
    EXPECT_LT(query_time_ms, 100) << "Large posting list query too slow";
}
#endif

// ============================================================================
// Regression Tests
// ============================================================================

TEST(IndexRegressionTest, EmptyMatcherResult) {
    Index index;
    
    Labels labels;
    labels.add("__name__", "test");
    index.add_series(1, labels);
    
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "__name__", "nonexistent")
    };
    
    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 0);
}

TEST(IndexRegressionTest, RegexMatcher) {
    Index index;
    
    for (int i = 0; i < 100; ++i) {
        Labels labels;
        labels.add("__name__", "http_requests_total");
        labels.add("method", i % 2 == 0 ? "GET" : "POST");
        labels.add("status", std::to_string(200 + (i % 5) * 100));
        index.add_series(i, labels);
    }
    
    std::vector<LabelMatcher> matchers = {
        LabelMatcher(MatcherType::Equal, "__name__", "http_requests_total"),
        LabelMatcher(MatcherType::RegexMatch, "status", "2.*")
    };
    
    auto result = index.find_series(matchers);
    ASSERT_TRUE(result.ok());
    EXPECT_GT(result.value().size(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
