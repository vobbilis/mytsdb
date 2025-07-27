#include <benchmark/benchmark.h>
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include <random>

namespace tsdb {
namespace benchmark {
namespace {

// Benchmark TimeSeries creation
static void BM_TimeSeriesCreation(benchmark::State& state) {
    for (auto _ : state) {
        core::Labels labels;
        labels.add("__name__", "test_metric");
        labels.add("instance", "localhost");
        labels.add("job", "test");
        
        core::TimeSeries series(labels);
        benchmark::DoNotOptimize(series);
    }
}

// Benchmark Labels operations
static void BM_LabelsOperations(benchmark::State& state) {
    for (auto _ : state) {
        core::Labels labels;
        
        // Add labels
        labels.add("name", "test");
        labels.add("type", "gauge");
        labels.add("instance", "localhost");
        
        // Query labels
        benchmark::DoNotOptimize(labels.has("name"));
        benchmark::DoNotOptimize(labels.get("name"));
        benchmark::DoNotOptimize(labels.map().size());
    }
}

// Benchmark Sample operations
static void BM_SampleOperations(benchmark::State& state) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> value_dist(0.0, 1000.0);
    std::uniform_int_distribution<> time_dist(0, 1000000);
    
    for (auto _ : state) {
        core::Timestamp timestamp = time_dist(gen);
        core::Value value = value_dist(gen);
        
        core::Sample sample(timestamp, value);
        benchmark::DoNotOptimize(sample.timestamp());
        benchmark::DoNotOptimize(sample.value());
    }
}

// Benchmark Configuration operations
static void BM_ConfigurationOperations(benchmark::State& state) {
    for (auto _ : state) {
        core::StorageConfig config;
        config.data_dir = "/tmp/test";
        config.block_size = 8192;
        config.max_blocks_per_series = 1000;
        config.cache_size_bytes = 1024 * 1024;
        config.block_duration = 3600 * 1000;
        config.retention_period = 7 * 24 * 3600 * 1000;
        config.enable_compression = true;
        
        benchmark::DoNotOptimize(config.data_dir);
        benchmark::DoNotOptimize(config.block_size);
        benchmark::DoNotOptimize(config.enable_compression);
    }
}

// Register benchmarks
BENCHMARK(BM_TimeSeriesCreation);
BENCHMARK(BM_LabelsOperations);
BENCHMARK(BM_SampleOperations);
BENCHMARK(BM_ConfigurationOperations);

} // namespace
} // namespace benchmark
} // namespace tsdb
