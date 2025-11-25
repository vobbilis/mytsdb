#include <benchmark/benchmark.h>
#include "tsdb/core/types.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include <filesystem>
#include <random>
#include <vector>
#include <chrono>
#include <iostream>

using namespace tsdb;

// Fixture for Write Performance Benchmarks
class WriteBenchmark : public benchmark::Fixture {
protected:
    std::string test_dir_;
    std::unique_ptr<storage::StorageImpl> storage_;
    std::vector<core::TimeSeries> test_data_;

    void SetUp(const ::benchmark::State& state) override {
        test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_write_bench_" + std::to_string(std::rand());
        std::filesystem::create_directories(test_dir_);

        core::StorageConfig config;
        config.data_dir = test_dir_;
        // WAL configuration handled by defaults or internal logic
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        storage_->init(config);

        // Pre-generate data to avoid measuring generation time
        generateTestData(state.range(0));
    }

    void TearDown(const ::benchmark::State& state) override {
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }

    void generateTestData(int num_series) {
        test_data_.clear();
        test_data_.reserve(num_series);
        
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        for (int i = 0; i < num_series; ++i) {
            core::Labels labels;
            labels.add("metric", "benchmark_metric");
            labels.add("host", "host_" + std::to_string(i % 100));
            labels.add("region", "us-east-" + std::to_string(i % 5));
            
            core::TimeSeries series(labels);
            series.add_sample(now + i, static_cast<double>(i));
            test_data_.push_back(series);
        }
    }
};

// Benchmark Single Threaded Writes
BENCHMARK_DEFINE_F(WriteBenchmark, SingleThreadedWrite)(benchmark::State& state) {
    int idx = 0;
    for (auto _ : state) {
        // Write data cyclically
        auto result = storage_->write(test_data_[idx % test_data_.size()]);
        if (!result.ok()) {
            state.SkipWithError(result.error().c_str());
            break;
        }
        idx++;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(WriteBenchmark, SingleThreadedWrite)
    ->Range(1000, 100000) // Number of pre-generated series
    ->Unit(benchmark::kMillisecond);

// Benchmark Batch Writes (simulated by calling write in a loop within the measured block, 
// but we want to measure throughput of individual writes here as the API is single-write)
// Note: Real batch write API might be added later.

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
}

