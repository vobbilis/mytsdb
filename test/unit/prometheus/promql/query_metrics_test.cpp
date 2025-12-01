#include <gtest/gtest.h>
#include "tsdb/prometheus/promql/query_metrics.h"
#include <thread>
#include <chrono>

namespace tsdb {
namespace prometheus {
namespace promql {
namespace {

class QueryMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset metrics before each test
        QueryMetrics::GetInstance().Reset();
    }
    
    void TearDown() override {
        // Clean up after each test
        QueryMetrics::GetInstance().Reset();
    }
};

TEST_F(QueryMetricsTest, InitialState) {
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_EQ(snapshot.query_count, 0);
    EXPECT_EQ(snapshot.query_errors, 0);
    EXPECT_EQ(snapshot.total_query_time_ns, 0);
    EXPECT_EQ(snapshot.total_parse_time_ns, 0);
    EXPECT_EQ(snapshot.total_eval_time_ns, 0);
    EXPECT_EQ(snapshot.total_exec_time_ns, 0);
    EXPECT_EQ(snapshot.total_storage_read_time_ns, 0);
    EXPECT_EQ(snapshot.samples_scanned, 0);
    EXPECT_EQ(snapshot.series_scanned, 0);
    EXPECT_EQ(snapshot.bytes_scanned, 0);
}

TEST_F(QueryMetricsTest, RecordQuery) {
    QueryMetrics::GetInstance().RecordQuery(1000000, false);  // 1ms
    QueryMetrics::GetInstance().RecordQuery(2000000, false);  // 2ms
    QueryMetrics::GetInstance().RecordQuery(3000000, true);   // 3ms, error
    
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_EQ(snapshot.query_count, 3);
    EXPECT_EQ(snapshot.query_errors, 1);
    EXPECT_EQ(snapshot.total_query_time_ns, 6000000);  // 6ms total
}

TEST_F(QueryMetricsTest, RecordParse) {
    QueryMetrics::GetInstance().RecordParse(500000);   // 0.5ms
    QueryMetrics::GetInstance().RecordParse(1500000);  // 1.5ms
    
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_EQ(snapshot.total_parse_time_ns, 2000000);  // 2ms total
}

TEST_F(QueryMetricsTest, RecordEval) {
    QueryMetrics::GetInstance().RecordEval(10000000);  // 10ms
    QueryMetrics::GetInstance().RecordEval(20000000);  // 20ms
    
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_EQ(snapshot.total_eval_time_ns, 30000000);  // 30ms total
}

TEST_F(QueryMetricsTest, RecordStorageRead) {
    QueryMetrics::GetInstance().RecordStorageRead(5000000, 100, 5, 1024);  // 5ms, 100 samples, 5 series, 1KB
    QueryMetrics::GetInstance().RecordStorageRead(3000000, 50, 2, 512);    // 3ms, 50 samples, 2 series, 512B
    
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_EQ(snapshot.total_storage_read_time_ns, 8000000);  // 8ms total
    EXPECT_EQ(snapshot.samples_scanned, 150);
    EXPECT_EQ(snapshot.series_scanned, 7);
    EXPECT_EQ(snapshot.bytes_scanned, 1536);
}

TEST_F(QueryMetricsTest, ScopedQueryTimer) {
    {
        ScopedQueryTimer timer(ScopedQueryTimer::Type::QUERY);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_EQ(snapshot.query_count, 1);
    EXPECT_GT(snapshot.total_query_time_ns, 9000000);  // At least 9ms
}

TEST_F(QueryMetricsTest, ScopedParseTimer) {
    {
        ScopedQueryTimer timer(ScopedQueryTimer::Type::PARSE);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_GT(snapshot.total_parse_time_ns, 4000000);  // At least 4ms
}

TEST_F(QueryMetricsTest, ScopedStorageReadTimer) {
    {
        ScopedQueryTimer timer(ScopedQueryTimer::Type::STORAGE_READ);
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        timer.Stop(100, 5, 1024);  // 100 samples, 5 series, 1KB
    }
    
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_GT(snapshot.total_storage_read_time_ns, 2000000);  // At least 2ms
    EXPECT_EQ(snapshot.samples_scanned, 100);
    EXPECT_EQ(snapshot.series_scanned, 5);
    EXPECT_EQ(snapshot.bytes_scanned, 1024);
}

TEST_F(QueryMetricsTest, Reset) {
    QueryMetrics::GetInstance().RecordQuery(1000000, false);
    QueryMetrics::GetInstance().RecordParse(500000);
    QueryMetrics::GetInstance().RecordStorageRead(2000000, 50, 3, 512);
    
    auto snapshot1 = QueryMetrics::GetInstance().GetSnapshot();
    EXPECT_GT(snapshot1.query_count, 0);
    
    QueryMetrics::GetInstance().Reset();
    
    auto snapshot2 = QueryMetrics::GetInstance().GetSnapshot();
    EXPECT_EQ(snapshot2.query_count, 0);
    EXPECT_EQ(snapshot2.total_query_time_ns, 0);
    EXPECT_EQ(snapshot2.total_parse_time_ns, 0);
    EXPECT_EQ(snapshot2.samples_scanned, 0);
}

TEST_F(QueryMetricsTest, ConcurrentAccess) {
    const int num_threads = 10;
    const int iterations_per_thread = 100;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([iterations_per_thread]() {
            for (int j = 0; j < iterations_per_thread; ++j) {
                QueryMetrics::GetInstance().RecordQuery(1000, false);
                QueryMetrics::GetInstance().RecordParse(500);
                QueryMetrics::GetInstance().RecordStorageRead(2000, 10, 1, 100);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto snapshot = QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_EQ(snapshot.query_count, num_threads * iterations_per_thread);
    EXPECT_EQ(snapshot.samples_scanned, num_threads * iterations_per_thread * 10);
    EXPECT_EQ(snapshot.series_scanned, num_threads * iterations_per_thread);
}

} // namespace
} // namespace promql
} // namespace prometheus
} // namespace tsdb
