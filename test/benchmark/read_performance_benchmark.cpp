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

class ReadBenchmark : public benchmark::Fixture {
protected:
    std::string test_dir_;
    std::unique_ptr<storage::StorageImpl> storage_;
    std::vector<core::Labels> test_labels_;
    int64_t start_time_;
    int64_t end_time_;

    void SetUp(const ::benchmark::State& state) override {
        test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_read_bench_" + std::to_string(std::rand());
        std::filesystem::create_directories(test_dir_);

        core::StorageConfig config;
        config.data_dir = test_dir_;
        // WAL configuration handled by defaults or internal logic
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        storage_->init(config);

        // Pre-populate data
        populateData(state.range(0));
    }

    void TearDown(const ::benchmark::State& state) override {
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }

    void populateData(int num_series) {
        test_labels_.clear();
        test_labels_.reserve(num_series);
        
        start_time_ = std::chrono::system_clock::now().time_since_epoch().count();
        end_time_ = start_time_ + 100000;
        
        for (int i = 0; i < num_series; ++i) {
            core::Labels labels;
            labels.add("metric", "benchmark_metric");
            labels.add("host", "host_" + std::to_string(i));
            
            core::TimeSeries series(labels);
            // Add some samples
            for(int j=0; j<100; ++j) {
                series.add_sample(start_time_ + j * 1000, static_cast<double>(i + j));
            }
            
            storage_->write(series);
            test_labels_.push_back(labels);
        }
    }
};

BENCHMARK_DEFINE_F(ReadBenchmark, SingleThreadedRead)(benchmark::State& state) {
    int idx = 0;
    for (auto _ : state) {
        auto result = storage_->read(test_labels_[idx % test_labels_.size()], start_time_, end_time_);
        if (!result.ok()) {
            state.SkipWithError(result.error().c_str());
            break;
        }
        idx++;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(ReadBenchmark, SingleThreadedRead)
    ->Range(100, 10000) // Number of series to read from
    ->Unit(benchmark::kMillisecond);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
}

