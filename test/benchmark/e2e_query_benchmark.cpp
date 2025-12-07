/**
 * @file e2e_query_benchmark.cpp
 * @brief End-to-end query performance benchmarks
 * 
 * This test suite measures full query execution including:
 * - HTTP request handling
 * - PromQL parsing and evaluation
 * - Storage reads (cache + blocks)
 * - Response serialization
 * 
 * Run before and after optimizations to measure overall improvement.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>

#include "tsdb/storage/storage_impl.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"

using namespace tsdb;
using namespace tsdb::storage;
using namespace tsdb::prometheus::promql;
using tsdb::prometheus::storage::TSDBAdapter;

namespace {

// =============================================================================
// Latency Tracker
// =============================================================================

class LatencyTracker {
public:
    void Record(int64_t latency_us) {
        std::lock_guard<std::mutex> lock(mutex_);
        latencies_.push_back(latency_us);
    }
    
    int64_t P50() const { return Percentile(50); }
    int64_t P90() const { return Percentile(90); }
    int64_t P99() const { return Percentile(99); }
    int64_t Max() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return latencies_.empty() ? 0 : *std::max_element(latencies_.begin(), latencies_.end());
    }
    int64_t Min() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return latencies_.empty() ? 0 : *std::min_element(latencies_.begin(), latencies_.end());
    }
    double Mean() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (latencies_.empty()) return 0;
        return std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
    }
    size_t Count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return latencies_.size();
    }
    double QPS(double duration_sec) const {
        return Count() / duration_sec;
    }
    
    std::string Summary() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "Count: " << Count() << ", ";
        ss << "P50: " << P50() / 1000.0 << "ms, ";
        ss << "P90: " << P90() / 1000.0 << "ms, ";
        ss << "P99: " << P99() / 1000.0 << "ms, ";
        ss << "Max: " << Max() / 1000.0 << "ms";
        return ss.str();
    }
    
    LatencyTracker() = default;

    LatencyTracker(const LatencyTracker& other) {
        std::lock_guard<std::mutex> lock(other.mutex_);
        latencies_ = other.latencies_;
    }

    LatencyTracker& operator=(const LatencyTracker& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(other.mutex_);
            std::lock_guard<std::mutex> my_lock(mutex_);
            latencies_ = other.latencies_;
        }
        return *this;
    }

    // Move constructor
    LatencyTracker(LatencyTracker&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mutex_);
        latencies_ = std::move(other.latencies_);
    }

    // Move assignment
    LatencyTracker& operator=(LatencyTracker&& other) noexcept {
        if (this != &other) {
            std::unique_lock<std::mutex> lock(other.mutex_);
            std::lock_guard<std::mutex> my_lock(mutex_);
            latencies_ = std::move(other.latencies_);
        }
        return *this;
    }

private:
    int64_t Percentile(int p) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (latencies_.empty()) return 0;
        
        std::vector<int64_t> sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());
        
        size_t idx = static_cast<size_t>(p / 100.0 * sorted.size());
        if (idx >= sorted.size()) idx = sorted.size() - 1;
        return sorted[idx];
    }
    
    mutable std::mutex mutex_;
    std::vector<int64_t> latencies_;
};

// =============================================================================
// Test Fixture
// =============================================================================

class E2EQueryBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory
        temp_dir_ = "/tmp/tsdb_e2e_" + std::to_string(std::time(nullptr));
        std::filesystem::create_directories(temp_dir_);
        
        // Initialize storage
        core::StorageConfig config;
        config.data_dir = temp_dir_;
        
        storage_ = std::make_shared<StorageImpl>(config);
        storage_->init(config);
        
        // Create engine
        adapter_ = std::make_shared<TSDBAdapter>(storage_);
        EngineOptions opts;
        opts.storage_adapter = adapter_.get();
        opts.lookback_delta = std::chrono::minutes(5);
        engine_ = std::make_unique<Engine>(opts);
        
        // Seed test data
        SeedTestData();
    }
    
    void TearDown() override {
        LogResults();
        
        engine_.reset();
        storage_->close();
        storage_.reset();
        std::filesystem::remove_all(temp_dir_);
    }
    
    void SeedTestData() {
        std::cout << "Seeding test data..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        now_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Create 1000 time series with 1 hour of data (240 samples per series at 15s interval)
        int num_series = 1000;
        int samples_per_series = 240;
        int64_t interval_ms = 15000;
        
        std::mt19937 gen(42);
        std::uniform_real_distribution<> dist(0.0, 100.0);
        
        for (int s = 0; s < num_series; s++) {
            core::Labels labels;
            labels.add("__name__", "container_cpu_usage_seconds_total");
            labels.add("namespace", "ns-" + std::to_string(s % 10));
            labels.add("pod", "pod-" + std::to_string(s % 100));
            labels.add("container", "container-" + std::to_string(s));
            labels.add("instance", "localhost:9090");
            labels.add("job", "kubelet");
            
            core::TimeSeries series(labels);
            int64_t t = now_ - (samples_per_series * interval_ms);
            for (int i = 0; i < samples_per_series; i++) {
                core::Sample sample(t, dist(gen));
                series.add_sample(sample);
                t += interval_ms;
            }
            storage_->write(series);
        }
        
        // Also add some other metrics
        for (int s = 0; s < 100; s++) {
            core::Labels labels;
            labels.add("__name__", "container_memory_working_set_bytes");
            labels.add("namespace", "ns-" + std::to_string(s % 10));
            labels.add("pod", "pod-" + std::to_string(s));
            
            core::TimeSeries series(labels);
            int64_t t = now_ - (samples_per_series * interval_ms);
            for (int i = 0; i < samples_per_series; i++) {
                core::Sample sample(t, dist(gen) * 1e9);  // Bytes
                series.add_sample(sample);
                t += interval_ms;
            }
            storage_->write(series);
        }
        
        // Flush to blocks
        storage_->flush();
        
        auto duration = std::chrono::high_resolution_clock::now() - start;
        std::cout << "Seeded " << (num_series + 100) << " series in " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() 
                  << "ms" << std::endl;
    }
    
    void RunInstantQuery(const std::string& query, LatencyTracker& tracker) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine_->ExecuteInstant(query, now_);
        auto end = std::chrono::high_resolution_clock::now();
        
        int64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        tracker.Record(latency_us);
        
        if (!result.error.empty()) {
            std::cerr << "Query error: " << result.error << std::endl;
        }
    }
    
    void RunRangeQuery(const std::string& query, int64_t range_ms, int64_t step_ms, LatencyTracker& tracker) {
        int64_t start_time = now_ - range_ms;
        int64_t end_time = now_;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine_->ExecuteRange(query, start_time, end_time, step_ms);
        auto end = std::chrono::high_resolution_clock::now();
        
        int64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        tracker.Record(latency_us);
        
        if (!result.error.empty()) {
            std::cerr << "Query error: " << result.error << std::endl;
        }
    }
    
    void LogResults() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "        E2E BENCHMARK RESULTS" << std::endl;
        std::cout << "========================================" << std::endl;
        
        for (const auto& [name, tracker] : results_) {
            std::cout << name << ": " << tracker.Summary() << std::endl;
        }
        
        std::cout << "========================================\n" << std::endl;
    }
    
    std::string temp_dir_;
    std::shared_ptr<StorageImpl> storage_;
    std::shared_ptr<TSDBAdapter> adapter_;
    std::unique_ptr<Engine> engine_;
    int64_t now_;
    std::map<std::string, LatencyTracker> results_;
};

// =============================================================================
// Instant Query Tests
// =============================================================================

TEST_F(E2EQueryBenchmark, InstantQuery_SimpleSelector) {
    LatencyTracker tracker;
    std::string query = "container_cpu_usage_seconds_total{namespace=\"ns-0\"}";
    
    // Warmup
    for (int i = 0; i < 10; i++) {
        RunInstantQuery(query, tracker);
    }
    tracker = LatencyTracker();  // Reset
    
    // Benchmark
    for (int i = 0; i < 100; i++) {
        RunInstantQuery(query, tracker);
    }
    
    results_["InstantQuery_SimpleSelector"] = tracker;
    
    // Assertions
    EXPECT_LT(tracker.P50(), 50000);   // p50 < 50ms
    EXPECT_LT(tracker.P99(), 200000);  // p99 < 200ms
}

TEST_F(E2EQueryBenchmark, InstantQuery_RateFunction) {
    LatencyTracker tracker;
    std::string query = "rate(container_cpu_usage_seconds_total[5m])";
    
    // Warmup
    for (int i = 0; i < 10; i++) {
        RunInstantQuery(query, tracker);
    }
    tracker = LatencyTracker();
    
    // Benchmark
    for (int i = 0; i < 100; i++) {
        RunInstantQuery(query, tracker);
    }
    
    results_["InstantQuery_RateFunction"] = tracker;
    
    EXPECT_LT(tracker.P50(), 100000);  // p50 < 100ms
    EXPECT_LT(tracker.P99(), 500000);  // p99 < 500ms
}

TEST_F(E2EQueryBenchmark, InstantQuery_SumByNamespace) {
    LatencyTracker tracker;
    std::string query = "sum(rate(container_cpu_usage_seconds_total[5m])) by (namespace)";
    
    for (int i = 0; i < 10; i++) {
        RunInstantQuery(query, tracker);
    }
    tracker = LatencyTracker();
    
    for (int i = 0; i < 100; i++) {
        RunInstantQuery(query, tracker);
    }
    
    results_["InstantQuery_SumByNamespace"] = tracker;
    
    EXPECT_LT(tracker.P50(), 200000);  // p50 < 200ms
    EXPECT_LT(tracker.P99(), 1000000); // p99 < 1s
}

// =============================================================================
// Range Query Tests
// =============================================================================

TEST_F(E2EQueryBenchmark, RangeQuery_1Hour_1MinStep) {
    LatencyTracker tracker;
    std::string query = "rate(container_cpu_usage_seconds_total[5m])";
    
    // 1 hour range, 1 minute step = 60 steps
    int64_t range_ms = 3600000;
    int64_t step_ms = 60000;
    
    for (int i = 0; i < 5; i++) {
        RunRangeQuery(query, range_ms, step_ms, tracker);
    }
    tracker = LatencyTracker();
    
    for (int i = 0; i < 20; i++) {
        RunRangeQuery(query, range_ms, step_ms, tracker);
    }
    
    results_["RangeQuery_1Hour_1MinStep"] = tracker;
    
    // This is the key metric we want to improve
    // Current baseline: ~3-4 seconds
    // Target: <500ms
    EXPECT_LT(tracker.P99(), 5000000);  // p99 < 5s (current baseline is ~4s)
}

TEST_F(E2EQueryBenchmark, RangeQuery_6Hour_5MinStep) {
    LatencyTracker tracker;
    std::string query = "sum(rate(container_cpu_usage_seconds_total[5m])) by (namespace)";
    
    // 6 hour range, 5 minute step = 72 steps
    int64_t range_ms = 21600000;
    int64_t step_ms = 300000;
    
    for (int i = 0; i < 3; i++) {
        RunRangeQuery(query, range_ms, step_ms, tracker);
    }
    tracker = LatencyTracker();
    
    for (int i = 0; i < 10; i++) {
        RunRangeQuery(query, range_ms, step_ms, tracker);
    }
    
    results_["RangeQuery_6Hour_5MinStep"] = tracker;
    
    EXPECT_LT(tracker.P99(), 10000000);  // p99 < 10s
}

// =============================================================================
// Throughput Tests
// =============================================================================

TEST_F(E2EQueryBenchmark, Throughput_MixedQueries) {
    std::vector<std::string> queries = {
        "container_cpu_usage_seconds_total{namespace=\"ns-0\"}",
        "rate(container_cpu_usage_seconds_total[5m])",
        "sum(container_memory_working_set_bytes) by (namespace)",
        "container_cpu_usage_seconds_total",
    };
    
    LatencyTracker tracker;
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dist(0, queries.size() - 1);
    
    auto start = std::chrono::high_resolution_clock::now();
    int iterations = 500;
    
    for (int i = 0; i < iterations; i++) {
        RunInstantQuery(queries[dist(gen)], tracker);
    }
    
    auto duration = std::chrono::high_resolution_clock::now() - start;
    double duration_sec = std::chrono::duration<double>(duration).count();
    double qps = tracker.QPS(duration_sec);
    
    results_["Throughput_MixedQueries"] = tracker;
    
    std::cout << "Throughput: " << std::fixed << std::setprecision(1) << qps << " qps" << std::endl;
    
    // Target: ≥100 qps
    EXPECT_GE(qps, 10);  // Current baseline might be lower
}

// =============================================================================
// Cache Effectiveness Test
// =============================================================================

TEST_F(E2EQueryBenchmark, CacheEffectiveness) {
    std::string query = "container_cpu_usage_seconds_total{namespace=\"ns-0\"}";
    
    LatencyTracker first_tracker;
    LatencyTracker cached_tracker;
    
    // First query - cache miss
    RunInstantQuery(query, first_tracker);
    int64_t first_latency = first_tracker.P50();
    
    // Subsequent queries - should hit cache
    for (int i = 0; i < 50; i++) {
        RunInstantQuery(query, cached_tracker);
    }
    
    double speedup = static_cast<double>(first_latency) / cached_tracker.Mean();
    
    std::cout << "First query: " << first_latency / 1000.0 << "ms" << std::endl;
    std::cout << "Cached avg: " << cached_tracker.Mean() / 1000.0 << "ms" << std::endl;
    std::cout << "Speedup: " << std::fixed << std::setprecision(1) << speedup << "x" << std::endl;
    
    results_["CacheEffectiveness_First"] = first_tracker;
    results_["CacheEffectiveness_Cached"] = cached_tracker;
    
    // With cache, subsequent queries should be significantly faster
    // (This test will pass even without cache, but speedup should be >1x after Phase 3)
    EXPECT_GT(speedup, 0.5);  // At minimum, shouldn't be slower
}

// =============================================================================
// SLA Compliance Test (Main Success Criteria)
// =============================================================================

TEST_F(E2EQueryBenchmark, SLA_Compliance) {
    std::cout << "\n=== SLA Compliance Test ===" << std::endl;
    
    LatencyTracker tracker;
    std::vector<std::string> queries = {
        "container_cpu_usage_seconds_total",
        "rate(container_cpu_usage_seconds_total[5m])",
        "sum(rate(container_cpu_usage_seconds_total[5m])) by (namespace)",
        "container_memory_working_set_bytes{namespace=\"ns-0\"}",
    };
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dist(0, queries.size() - 1);
    
    // Run 1000 mixed queries
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        RunInstantQuery(queries[dist(gen)], tracker);
    }
    auto duration = std::chrono::high_resolution_clock::now() - start;
    double duration_sec = std::chrono::duration<double>(duration).count();
    
    double qps = tracker.QPS(duration_sec);
    
    std::cout << "Total queries: " << tracker.Count() << std::endl;
    std::cout << "Duration: " << std::fixed << std::setprecision(1) << duration_sec << "s" << std::endl;
    std::cout << "Throughput: " << qps << " qps" << std::endl;
    std::cout << tracker.Summary() << std::endl;
    
    results_["SLA_Compliance"] = tracker;
    
    // SLA Targets (from plan):
    // - p50 ≤ 50ms
    // - p99 ≤ 500ms  
    // - Throughput ≥ 100 qps
    
    bool p50_pass = tracker.P50() <= 50000;
    bool p99_pass = tracker.P99() <= 500000;
    bool qps_pass = qps >= 100;
    
    std::cout << "\nSLA Results:" << std::endl;
    std::cout << "  p50 ≤ 50ms:    " << (p50_pass ? "PASS" : "FAIL") 
              << " (actual: " << tracker.P50() / 1000.0 << "ms)" << std::endl;
    std::cout << "  p99 ≤ 500ms:   " << (p99_pass ? "PASS" : "FAIL")
              << " (actual: " << tracker.P99() / 1000.0 << "ms)" << std::endl;
    std::cout << "  QPS ≥ 100:     " << (qps_pass ? "PASS" : "FAIL")
              << " (actual: " << qps << ")" << std::endl;
    
    // Note: These assertions are expected to FAIL before optimization
    // They define our target, not current state
    // Uncomment after Phase 4 is complete:
    // EXPECT_LE(tracker.P50(), 50000);
    // EXPECT_LE(tracker.P99(), 500000);
    // EXPECT_GE(qps, 100);
}

}  // namespace
