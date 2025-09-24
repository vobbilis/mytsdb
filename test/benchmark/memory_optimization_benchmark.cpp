#include <benchmark/benchmark.h>
#include "tsdb/storage/enhanced_pools/enhanced_time_series_pool.h"
#include "tsdb/storage/memory_optimization/sequential_layout_optimizer.h"
#include "tsdb/storage/memory_optimization/cache_alignment_utils.h"
#include "tsdb/storage/memory_optimization/access_pattern_optimizer.h"
#include "tsdb/storage/memory_optimization/adaptive_memory_integration.h"
#include "tsdb/storage/memory_optimization/tiered_memory_integration.h"
#include "tsdb/common/time.h"
#include "tsdb/common/labels.h"
#include "tsdb/common/sample.h"

// Helper function to generate a TimeSeries
tsdb::core::TimeSeries generateTestTimeSeries(int num_samples) {
    tsdb::core::TimeSeries series;
    series.add_label({"__name__", "test_metric"});
    series.add_label({"instance", "test_instance"});
    for (int i = 0; i < num_samples; ++i) {
        series.add_sample(tsdb::common::Sample(1000 + i * 100, static_cast<double>(i)));
    }
    return series;
}

// Benchmark for EnhancedTimeSeriesPool allocation
static void BM_EnhancedTimeSeriesPoolAllocation(benchmark::State& state) {
    EnhancedTimeSeriesPool pool;
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<std::unique_ptr<tsdb::core::TimeSeries>> series_vec;
        series_vec.reserve(state.range(0));
        state.ResumeTiming();

        for (int i = 0; i < state.range(0); ++i) {
            series_vec.push_back(pool.acquire_aligned());
        }
        state.PauseTiming();
        for (auto& s : series_vec) {
            pool.release(std::move(s));
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_EnhancedTimeSeriesPoolAllocation)->Range(1, 1024);

// Benchmark for SequentialLayoutOptimizer
static void BM_SequentialLayoutOptimization(benchmark::State& state) {
    tsdb::core::TimeSeries original_series = generateTestTimeSeries(state.range(0));
    SequentialLayoutOptimizer optimizer;
    for (auto _ : state) {
        // Simulate optimizing layout
        optimizer.optimize_time_series_layout(original_series);
    }
}
BENCHMARK(BM_SequentialLayoutOptimization)->Range(1, 1024);

// Benchmark for CacheAlignmentUtils
static void BM_CacheAlignment(benchmark::State& state) {
    std::vector<char> data(state.range(0));
    for (auto _ : state) {
        cache_alignment_utils::align_to_cache_line(data.data());
    }
}
BENCHMARK(BM_CacheAlignment)->Range(64, 4096);

// Benchmark for AccessPatternOptimizer (simplified)
static void BM_AccessPatternOptimization(benchmark::State& state) {
    AccessPatternOptimizer optimizer;
    std::vector<uint64_t> addresses;
    for (int i = 0; i < state.range(0); ++i) {
        addresses.push_back(i * 64); // Simulate cache-line aligned addresses
    }

    for (auto _ : state) {
        optimizer.record_bulk_access(addresses);
        optimizer.analyze_access_patterns();
    }
}
BENCHMARK(BM_AccessPatternOptimization)->Range(1, 1024);

// Benchmark for AdaptiveMemoryIntegration
static void BM_AdaptiveMemoryIntegration(benchmark::State& state) {
    AdaptiveMemoryIntegration integration;
    for (auto _ : state) {
        auto result = integration.allocate_optimized(state.range(0));
        if (result.ok()) {
            integration.deallocate_optimized(result.value());
        }
    }
}
BENCHMARK(BM_AdaptiveMemoryIntegration)->Range(64, 4096);

// Benchmark for TieredMemoryIntegration
static void BM_TieredMemoryIntegration(benchmark::State& state) {
    TieredMemoryIntegration integration;
    tsdb::core::TimeSeries series = generateTestTimeSeries(10);
    
    for (auto _ : state) {
        integration.promote_series(&series);
        integration.demote_series(&series);
    }
}
BENCHMARK(BM_TieredMemoryIntegration)->Range(1, 100);

// Main entry point for benchmarks
BENCHMARK_MAIN();