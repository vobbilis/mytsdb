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

class LatencyBenchmark : public benchmark::Fixture {
protected:
    std::string test_dir_;
    std::unique_ptr<storage::StorageImpl> storage_;
    core::TimeSeries test_series_;
    core::Labels test_labels_;

    void SetUp(const ::benchmark::State& state) override {
        test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_latency_bench_" + std::to_string(std::rand());
        std::filesystem::create_directories(test_dir_);

        core::StorageConfig config;
        config.data_dir = test_dir_;
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        storage_->init(config);

        test_labels_.add("metric", "latency_test");
        test_series_ = core::TimeSeries(test_labels_);
        test_series_.add_sample(std::chrono::system_clock::now().time_since_epoch().count(), 42.0);
    }

    void TearDown(const ::benchmark::State& state) override {
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }
};

BENCHMARK_DEFINE_F(LatencyBenchmark, WriteLatency)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = storage_->write(test_series_);
        if (!result.ok()) {
            state.SkipWithError(result.error().c_str());
            break;
        }
    }
}

BENCHMARK_REGISTER_F(LatencyBenchmark, WriteLatency)
    ->Iterations(10000)
    ->Unit(benchmark::kMicrosecond);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
}

