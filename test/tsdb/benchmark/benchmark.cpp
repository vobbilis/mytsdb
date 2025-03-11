#include <benchmark/benchmark.h>
#include "tsdb/storage/storage.h"
#include "tsdb/histogram/histogram.h"
#include <random>
#include <filesystem>

namespace tsdb {
namespace benchmark {

// Helper function to generate random samples
std::vector<core::Sample> GenerateRandomSamples(
    size_t count,
    core::Timestamp start_time,
    core::Timestamp interval) {
    std::vector<core::Sample> samples;
    samples.reserve(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> value_dist(100.0, 10.0);
    
    core::Timestamp ts = start_time;
    for (size_t i = 0; i < count; i++) {
        samples.emplace_back(ts, value_dist(gen));
        ts += interval;
    }
    
    return samples;
}

// Benchmark write performance
static void BM_StorageWrite(benchmark::State& state) {
    // Set up storage
    std::filesystem::path test_dir = 
        std::filesystem::temp_directory_path() / "tsdb_benchmark";
    std::filesystem::create_directories(test_dir);
    
    storage::StorageOptions options;
    options.data_dir = test_dir.string();
    auto storage = storage::CreateStorage(options);
    
    // Create a series
    core::Labels labels = {{"name", "test_metric"}, {"host", "localhost"}};
    auto series_id = storage->CreateSeries(
        labels,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    
    // Generate test data
    const size_t batch_size = state.range(0);
    auto samples = GenerateRandomSamples(batch_size, 0, 1000);
    
    for (auto _ : state) {
        auto result = storage->Write(series_id.value(), samples);
        if (!result.ok()) {
            state.SkipWithError("Write failed");
            break;
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
    std::filesystem::remove_all(test_dir);
}

// Benchmark read performance
static void BM_StorageRead(benchmark::State& state) {
    // Set up storage
    std::filesystem::path test_dir = 
        std::filesystem::temp_directory_path() / "tsdb_benchmark";
    std::filesystem::create_directories(test_dir);
    
    storage::StorageOptions options;
    options.data_dir = test_dir.string();
    auto storage = storage::CreateStorage(options);
    
    // Create a series and write test data
    core::Labels labels = {{"name", "test_metric"}, {"host", "localhost"}};
    auto series_id = storage->CreateSeries(
        labels,
        core::MetricType::GAUGE,
        core::Granularity::Normal());
    
    const size_t total_samples = state.range(0);
    auto samples = GenerateRandomSamples(total_samples, 0, 1000);
    storage->Write(series_id.value(), samples);
    
    for (auto _ : state) {
        auto result = storage->Read(series_id.value(), 0, total_samples * 1000);
        if (!result.ok()) {
            state.SkipWithError("Read failed");
            break;
        }
    }
    
    state.SetItemsProcessed(state.iterations() * total_samples);
    std::filesystem::remove_all(test_dir);
}

// Benchmark histogram operations
static void BM_HistogramOperations(benchmark::State& state) {
    const size_t num_samples = state.range(0);
    
    // Create histogram
    auto hist = histogram::CreateExponentialHistogram(2.0, 2);
    
    // Generate test data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::lognormal_distribution<> value_dist(2.0, 1.0);
    
    std::vector<double> values;
    values.reserve(num_samples);
    for (size_t i = 0; i < num_samples; i++) {
        values.push_back(value_dist(gen));
    }
    
    for (auto _ : state) {
        for (double value : values) {
            hist->Record(value);
        }
        benchmark::DoNotOptimize(hist->Quantile(0.99));
    }
    
    state.SetItemsProcessed(state.iterations() * num_samples);
}

// Benchmark query performance
static void BM_StorageQuery(benchmark::State& state) {
    // Set up storage
    std::filesystem::path test_dir = 
        std::filesystem::temp_directory_path() / "tsdb_benchmark";
    std::filesystem::create_directories(test_dir);
    
    storage::StorageOptions options;
    options.data_dir = test_dir.string();
    auto storage = storage::CreateStorage(options);
    
    // Create multiple series with test data
    const size_t num_series = 100;
    const size_t samples_per_series = state.range(0);
    
    for (size_t i = 0; i < num_series; i++) {
        core::Labels labels = {
            {"name", "test_metric"},
            {"host", "host" + std::to_string(i)}
        };
        auto series_id = storage->CreateSeries(
            labels,
            core::MetricType::GAUGE,
            core::Granularity::Normal());
        
        auto samples = GenerateRandomSamples(
            samples_per_series, 0, 1000);
        storage->Write(series_id.value(), samples);
    }
    
    // Query parameters
    core::Labels query_labels = {{"name", "test_metric"}};
    
    for (auto _ : state) {
        auto result = storage->Query(
            query_labels,
            0,
            samples_per_series * 1000);
        if (!result.ok()) {
            state.SkipWithError("Query failed");
            break;
        }
    }
    
    state.SetItemsProcessed(
        state.iterations() * num_series * samples_per_series);
    std::filesystem::remove_all(test_dir);
}

BENCHMARK(BM_StorageWrite)
    ->RangeMultiplier(10)
    ->Range(1000, 1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_StorageRead)
    ->RangeMultiplier(10)
    ->Range(1000, 1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_HistogramOperations)
    ->RangeMultiplier(10)
    ->Range(1000, 1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_StorageQuery)
    ->RangeMultiplier(10)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMicrosecond);

} // namespace benchmark
} // namespace tsdb

BENCHMARK_MAIN(); 