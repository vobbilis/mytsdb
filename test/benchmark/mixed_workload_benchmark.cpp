#include <benchmark/benchmark.h>
#include "tsdb/core/types.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include <filesystem>
#include <random>
#include <vector>
#include <chrono>
#include <thread>

using namespace tsdb;

class MixedWorkloadBenchmark : public benchmark::Fixture {
protected:
    std::string test_dir_;
    std::unique_ptr<storage::StorageImpl> storage_;
    std::vector<core::TimeSeries> write_data_;
    std::vector<core::Labels> read_labels_;
    int64_t start_time_;
    int64_t end_time_;

    void SetUp(const ::benchmark::State& state) override {
        test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_mixed_bench_" + std::to_string(std::rand());
        std::filesystem::create_directories(test_dir_);

        core::StorageConfig config;
        config.data_dir = test_dir_;
        
        storage_ = std::make_unique<storage::StorageImpl>(config);
        storage_->init(config);

        prepareData(state.range(0));
    }

    void TearDown(const ::benchmark::State& state) override {
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }

    void prepareData(int num_series) {
        write_data_.clear();
        read_labels_.clear();
        
        start_time_ = std::chrono::system_clock::now().time_since_epoch().count();
        end_time_ = start_time_ + 100000;

        for (int i = 0; i < num_series; ++i) {
            core::Labels labels;
            labels.add("metric", "mixed_metric");
            labels.add("id", std::to_string(i));
            
            core::TimeSeries series(labels);
            series.add_sample(start_time_, static_cast<double>(i));
            
            write_data_.push_back(series);
            read_labels_.push_back(labels);
            
            // Seed some initial data
            if (i < num_series / 2) {
                storage_->write(series);
            }
        }
    }
};

BENCHMARK_DEFINE_F(MixedWorkloadBenchmark, ReadWrite50_50)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        if (i % 2 == 0) {
            // Write
            storage_->write(write_data_[i % write_data_.size()]);
        } else {
            // Read
            storage_->read(read_labels_[i % read_labels_.size()], start_time_, end_time_);
        }
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(MixedWorkloadBenchmark, ReadWrite50_50)
    ->Range(100, 1000)
    ->Unit(benchmark::kMillisecond);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
}

