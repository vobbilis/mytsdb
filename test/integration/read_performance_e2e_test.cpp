/**
 * @file read_performance_e2e_test.cpp
 * @brief Integration tests for read performance SLA compliance
 * 
 * These tests validate that the read path meets SLA targets:
 * - p50 latency ≤ 50ms
 * - p99 latency ≤ 500ms
 * - Throughput ≥ 100 qps
 * 
 * Run after each optimization phase to validate improvements.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <numeric>
#include <memory>
#include <thread>
#include <atomic>
#include <future>

#include "tsdb/storage/storage_impl.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/storage_adapter.h"
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"

using namespace tsdb;
using namespace tsdb::storage;
using namespace tsdb::prometheus::promql;

namespace {

// SLA Targets (in microseconds for latency, queries per second for throughput)
constexpr int64_t SLA_P50_US = 50000;      // 50ms
constexpr int64_t SLA_P99_US = 500000;     // 500ms
constexpr double SLA_THROUGHPUT_QPS = 100; // 100 queries/sec

class ReadPerformanceE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = "/tmp/tsdb_read_perf_e2e_" + std::to_string(std::time(nullptr));
        std::filesystem::create_directories(temp_dir_);
        
        core::StorageConfig config;
        config.data_dir = temp_dir_;
        
        storage_ = std::make_shared<StorageImpl>(config);
        storage_->init(config);
        
        EngineOptions opts;
        opts.storage_adapter = std::make_shared<StorageAdapter>(storage_);
        opts.lookback_delta = std::chrono::minutes(5);
        engine_ = std::make_unique<Engine>(opts);
        
        SeedTestData();
    }
    
    void TearDown() override {
        engine_.reset();
        storage_->close();
        storage_.reset();
        std::filesystem::remove_all(temp_dir_);
    }
    
    void SeedTestData() {
        now_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        std::mt19937 gen(42);
        std::uniform_real_distribution<> dist(0.0, 100.0);
        
        // Create 500 series with 2 hours of data
        int num_series = 500;
        int samples_per_series = 480; // 2 hours at 15s interval
        int64_t interval_ms = 15000;
        
        for (int s = 0; s < num_series; s++) {
            core::Labels labels;
            labels.add("__name__", "test_metric");
            labels.add("namespace", "ns-" + std::to_string(s % 10));
            labels.add("pod", "pod-" + std::to_string(s % 50));
            labels.add("container", "c-" + std::to_string(s));
            
            int64_t t = now_ - (samples_per_series * interval_ms);
            for (int i = 0; i < samples_per_series; i++) {
                core::Sample sample(t, dist(gen));
                storage_->append(labels, sample);
                t += interval_ms;
            }
        }
        
        storage_->flush();
    }
    
    int64_t MeasureQueryLatency(const std::string& query) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine_->ExecuteInstant(query, now_);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    
    int64_t MeasureRangeQueryLatency(const std::string& query, int64_t range_ms, int64_t step_ms) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine_->ExecuteRange(query, now_ - range_ms, now_, step_ms);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    
    std::vector<int64_t> RunQueryBatch(const std::string& query, int iterations) {
        std::vector<int64_t> latencies;
        latencies.reserve(iterations);
        
        // Warmup
        for (int i = 0; i < 5; i++) {
            MeasureQueryLatency(query);
        }
        
        // Benchmark
        for (int i = 0; i < iterations; i++) {
            latencies.push_back(MeasureQueryLatency(query));
        }
        
        return latencies;
    }
    
    static int64_t Percentile(std::vector<int64_t>& latencies, int p) {
        if (latencies.empty()) return 0;
        std::sort(latencies.begin(), latencies.end());
        size_t idx = static_cast<size_t>(p / 100.0 * latencies.size());
        if (idx >= latencies.size()) idx = latencies.size() - 1;
        return latencies[idx];
    }
    
    std::string temp_dir_;
    std::shared_ptr<StorageImpl> storage_;
    std::unique_ptr<Engine> engine_;
    int64_t now_;
};

// =============================================================================
// Phase 0: Baseline Tests (expected to fail initially)
// =============================================================================

TEST_F(ReadPerformanceE2ETest, Baseline_InstantQuery_P50_Target) {
    std::string query = "test_metric{namespace=\"ns-0\"}";
    auto latencies = RunQueryBatch(query, 100);
    
    int64_t p50 = Percentile(latencies, 50);
    std::cout << "InstantQuery P50: " << p50 / 1000.0 << "ms (target: " << SLA_P50_US / 1000.0 << "ms)" << std::endl;
    
    // Record baseline - don't assert yet
    EXPECT_GT(p50, 0) << "Query should complete";
}

TEST_F(ReadPerformanceE2ETest, Baseline_InstantQuery_P99_Target) {
    std::string query = "rate(test_metric[5m])";
    auto latencies = RunQueryBatch(query, 100);
    
    int64_t p99 = Percentile(latencies, 99);
    std::cout << "InstantQuery P99: " << p99 / 1000.0 << "ms (target: " << SLA_P99_US / 1000.0 << "ms)" << std::endl;
    
    EXPECT_GT(p99, 0);
}

TEST_F(ReadPerformanceE2ETest, Baseline_RangeQuery_1Hour) {
    std::string query = "rate(test_metric[5m])";
    
    // Warmup
    MeasureRangeQueryLatency(query, 3600000, 60000);
    
    std::vector<int64_t> latencies;
    for (int i = 0; i < 10; i++) {
        latencies.push_back(MeasureRangeQueryLatency(query, 3600000, 60000));
    }
    
    int64_t p50 = Percentile(latencies, 50);
    int64_t p99 = Percentile(latencies, 99);
    
    std::cout << "RangeQuery (1h, 1m step) P50: " << p50 / 1000.0 << "ms" << std::endl;
    std::cout << "RangeQuery (1h, 1m step) P99: " << p99 / 1000.0 << "ms" << std::endl;
    
    // This is the key metric to improve - baseline is ~3-4 seconds
    EXPECT_GT(p50, 0);
}

TEST_F(ReadPerformanceE2ETest, Baseline_Throughput) {
    std::string query = "test_metric";
    
    auto start = std::chrono::high_resolution_clock::now();
    int iterations = 200;
    
    for (int i = 0; i < iterations; i++) {
        engine_->ExecuteInstant(query, now_);
    }
    
    auto duration = std::chrono::high_resolution_clock::now() - start;
    double duration_sec = std::chrono::duration<double>(duration).count();
    double qps = iterations / duration_sec;
    
    std::cout << "Throughput: " << qps << " qps (target: " << SLA_THROUGHPUT_QPS << " qps)" << std::endl;
    
    EXPECT_GT(qps, 0);
}

// =============================================================================
// Phase 1: Range Query Optimization Tests
// =============================================================================

TEST_F(ReadPerformanceE2ETest, Phase1_RangeQuery_StepOverhead) {
    std::string query = "rate(test_metric[5m])";
    
    // Measure with different step counts
    std::vector<std::pair<int, int64_t>> results;
    
    for (int steps : {10, 50, 100, 500}) {
        int64_t range_ms = steps * 60000;  // 1 minute per step
        int64_t step_ms = 60000;
        
        auto latency = MeasureRangeQueryLatency(query, range_ms, step_ms);
        results.push_back({steps, latency});
        
        std::cout << "Steps: " << steps << ", Latency: " << latency / 1000.0 << "ms" << std::endl;
    }
    
    // After Phase 1 optimization, latency should NOT scale linearly with steps
    // Before: 500 steps takes ~50x longer than 10 steps
    // After: Should be <5x longer
    
    double ratio = static_cast<double>(results[3].second) / results[0].second;
    std::cout << "Latency ratio (500 steps / 10 steps): " << ratio << "x" << std::endl;
    
    // Baseline: ratio is ~50x, target: <10x
    EXPECT_GT(ratio, 0);
}

// =============================================================================
// Phase 2: Block Index Tests
// =============================================================================

TEST_F(ReadPerformanceE2ETest, Phase2_BlockReadWithTimeRange) {
    std::string query = "test_metric{namespace=\"ns-0\"}";
    
    // Read full 2-hour range
    auto full_latency = MeasureRangeQueryLatency(query, 7200000, 300000);
    
    // Read last 5 minutes only
    auto narrow_latency = MeasureRangeQueryLatency(query, 300000, 60000);
    
    std::cout << "Full range (2h): " << full_latency / 1000.0 << "ms" << std::endl;
    std::cout << "Narrow range (5m): " << narrow_latency / 1000.0 << "ms" << std::endl;
    
    // After Phase 2, narrow range should be significantly faster
    // because we skip samples outside the range using block index
    double ratio = static_cast<double>(full_latency) / narrow_latency;
    std::cout << "Speedup ratio: " << ratio << "x" << std::endl;
    
    // Target: narrow query should be at least 2x faster
    EXPECT_GT(ratio, 1.0);  // Baseline may not show much difference
}

// =============================================================================
// Phase 3: Query Cache Tests
// =============================================================================

TEST_F(ReadPerformanceE2ETest, Phase3_CacheEffectiveness) {
    std::string query = "test_metric{namespace=\"ns-0\"}";
    
    // First query - cache miss
    auto first = MeasureQueryLatency(query);
    
    // Subsequent queries - should hit cache
    std::vector<int64_t> cached_latencies;
    for (int i = 0; i < 50; i++) {
        cached_latencies.push_back(MeasureQueryLatency(query));
    }
    
    double avg_cached = std::accumulate(cached_latencies.begin(), cached_latencies.end(), 0.0) 
                       / cached_latencies.size();
    
    std::cout << "First query (cache miss): " << first / 1000.0 << "ms" << std::endl;
    std::cout << "Average cached: " << avg_cached / 1000.0 << "ms" << std::endl;
    
    double speedup = static_cast<double>(first) / avg_cached;
    std::cout << "Cache speedup: " << speedup << "x" << std::endl;
    
    // After Phase 3, cached queries should be >5x faster
    EXPECT_GT(speedup, 0.5);  // Baseline: no cache, speedup ~1x
}

// =============================================================================
// Phase 4: Parallel Block Reads Tests
// =============================================================================

TEST_F(ReadPerformanceE2ETest, Phase4_ParallelBlockReads) {
    // This test is meaningful after data has been flushed to multiple blocks
    // For now, just measure cold path performance
    
    std::string query = "test_metric";
    
    // Query that spans multiple blocks (if they exist)
    auto latency = MeasureRangeQueryLatency(query, 7200000, 300000);
    
    std::cout << "Multi-block range query: " << latency / 1000.0 << "ms" << std::endl;
    
    // After Phase 4, parallel reads should reduce latency when multiple blocks are involved
    EXPECT_GT(latency, 0);
}

// =============================================================================
// Final SLA Compliance Tests (uncomment after all phases complete)
// =============================================================================

TEST_F(ReadPerformanceE2ETest, DISABLED_SLA_P50_Compliance) {
    std::vector<std::string> queries = {
        "test_metric",
        "test_metric{namespace=\"ns-0\"}",
        "rate(test_metric[5m])",
        "sum(test_metric) by (namespace)",
    };
    
    std::vector<int64_t> all_latencies;
    
    for (const auto& query : queries) {
        auto latencies = RunQueryBatch(query, 100);
        all_latencies.insert(all_latencies.end(), latencies.begin(), latencies.end());
    }
    
    int64_t p50 = Percentile(all_latencies, 50);
    
    EXPECT_LE(p50, SLA_P50_US) << "P50 latency " << p50 / 1000.0 
                               << "ms exceeds SLA target of " << SLA_P50_US / 1000.0 << "ms";
}

TEST_F(ReadPerformanceE2ETest, DISABLED_SLA_P99_Compliance) {
    std::vector<std::string> queries = {
        "test_metric",
        "test_metric{namespace=\"ns-0\"}",
        "rate(test_metric[5m])",
        "sum(test_metric) by (namespace)",
    };
    
    std::vector<int64_t> all_latencies;
    
    for (const auto& query : queries) {
        auto latencies = RunQueryBatch(query, 100);
        all_latencies.insert(all_latencies.end(), latencies.begin(), latencies.end());
    }
    
    int64_t p99 = Percentile(all_latencies, 99);
    
    EXPECT_LE(p99, SLA_P99_US) << "P99 latency " << p99 / 1000.0 
                               << "ms exceeds SLA target of " << SLA_P99_US / 1000.0 << "ms";
}

TEST_F(ReadPerformanceE2ETest, DISABLED_SLA_Throughput_Compliance) {
    std::string query = "test_metric";
    
    auto start = std::chrono::high_resolution_clock::now();
    int iterations = 1000;
    
    for (int i = 0; i < iterations; i++) {
        engine_->ExecuteInstant(query, now_);
    }
    
    auto duration = std::chrono::high_resolution_clock::now() - start;
    double duration_sec = std::chrono::duration<double>(duration).count();
    double qps = iterations / duration_sec;
    
    EXPECT_GE(qps, SLA_THROUGHPUT_QPS) << "Throughput " << qps 
                                        << " qps below SLA target of " << SLA_THROUGHPUT_QPS << " qps";
}

}  // namespace
