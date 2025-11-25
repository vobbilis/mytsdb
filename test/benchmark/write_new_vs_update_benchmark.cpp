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

// Benchmark to compare new series writes vs updates
class NewVsUpdateBenchmark : public benchmark::Fixture {
protected:
    std::string test_dir_;
    std::unique_ptr<storage::StorageImpl> storage_;
    std::vector<core::TimeSeries> new_series_data_;
    core::TimeSeries update_series_;

    void SetUp(const ::benchmark::State& state) override {
        test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_new_vs_update_" + std::to_string(std::rand());
        std::filesystem::create_directories(test_dir_);

        core::StorageConfig config;
        config.data_dir = test_dir_;
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        storage_->init(config);

        // Pre-generate new series data (each with unique labels)
        int num_new_series = 10000;
        new_series_data_.reserve(num_new_series);
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        for (int i = 0; i < num_new_series; ++i) {
            core::Labels labels;
            labels.add("metric", "new_series_metric");
            labels.add("unique_id", std::to_string(i));
            
            core::TimeSeries series(labels);
            series.add_sample(now + i, static_cast<double>(i));
            new_series_data_.push_back(series);
        }
        
        // Create one series for updates
        core::Labels update_labels;
        update_labels.add("metric", "update_series");
        update_labels.add("id", "update_target");
        update_series_ = core::TimeSeries(update_labels);
        update_series_.add_sample(now, 1.0);
        
        // Write the update series once to make it "existing"
        storage_->write(update_series_);
    }

    void TearDown(const ::benchmark::State& state) override {
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }
};

// Benchmark new series writes
BENCHMARK_DEFINE_F(NewVsUpdateBenchmark, NewSeriesWrites)(benchmark::State& state) {
    int idx = 0;
    for (auto _ : state) {
        auto result = storage_->write(new_series_data_[idx % new_series_data_.size()]);
        if (!result.ok()) {
            state.SkipWithError(result.error().c_str());
            break;
        }
        idx++;
    }
    state.SetItemsProcessed(state.iterations());
}

// Benchmark update writes (same series, different samples)
BENCHMARK_DEFINE_F(NewVsUpdateBenchmark, UpdateWrites)(benchmark::State& state) {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    int sample_idx = 0;
    
    // Get labels from the update series
    core::Labels update_labels = update_series_.labels();
    
    for (auto _ : state) {
        // Create new TimeSeries with same labels but new sample
        core::TimeSeries update_series(update_labels);
        update_series.add_sample(now + sample_idx, static_cast<double>(sample_idx));
        
        auto result = storage_->write(update_series);
        if (!result.ok()) {
            state.SkipWithError(result.error().c_str());
            break;
        }
        sample_idx++;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(NewVsUpdateBenchmark, NewSeriesWrites)
    ->Iterations(1000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(NewVsUpdateBenchmark, UpdateWrites)
    ->Iterations(1000)
    ->Unit(benchmark::kMicrosecond);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
}

