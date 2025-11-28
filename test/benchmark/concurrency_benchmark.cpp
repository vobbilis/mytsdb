#include <benchmark/benchmark.h>
#include "tsdb/core/types.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include <filesystem>
#include <random>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

using namespace tsdb;

class ConcurrencyBenchmark : public benchmark::Fixture {
protected:
    static std::unique_ptr<storage::StorageImpl> storage_;
    static std::string test_dir_;
    static std::vector<core::TimeSeries> test_data_;
    static std::mutex mutex_;
    static int active_threads_;

    void SetUp(const ::benchmark::State& state) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_threads_ == 0) {
            test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_concurrency_bench_" + std::to_string(std::rand());
            std::filesystem::create_directories(test_dir_);

            core::StorageConfig config;
            config.data_dir = test_dir_;
            
            storage_ = std::make_unique<storage::StorageImpl>(config);
            storage_->init(config);

            // Pre-generate enough data
            generateData(100000); 
        }
        active_threads_++;
    }

    void TearDown(const ::benchmark::State& state) override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_threads_--;
        if (active_threads_ == 0) {
            storage_.reset();
            std::filesystem::remove_all(test_dir_);
            test_data_.clear();
        }
    }

    void generateData(int num_series) {
        if (!test_data_.empty()) return;
        
        test_data_.reserve(num_series);
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        for (int i = 0; i < num_series; ++i) {
            core::Labels labels;
            labels.add("metric", "concurrent_metric");
            labels.add("id", std::to_string(i));
            
            core::TimeSeries series(labels);
            series.add_sample(now + i, static_cast<double>(i));
            test_data_.push_back(series);
        }
    }
};

std::unique_ptr<storage::StorageImpl> ConcurrencyBenchmark::storage_;
std::string ConcurrencyBenchmark::test_dir_;
std::vector<core::TimeSeries> ConcurrencyBenchmark::test_data_;
std::mutex ConcurrencyBenchmark::mutex_;
int ConcurrencyBenchmark::active_threads_ = 0;

BENCHMARK_DEFINE_F(ConcurrencyBenchmark, ConcurrentWrites)(benchmark::State& state) {
    // Thread index is state.thread_index()
    // Items processed per thread
    int items_per_thread = test_data_.size() / state.threads();
    int start_idx = state.thread_index() * items_per_thread;
    
    int i = 0;
    for (auto _ : state) {
        int data_idx = start_idx + (i % items_per_thread);
        storage_->write(test_data_[data_idx]);
        i++;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(ConcurrencyBenchmark, ConcurrentWrites)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Unit(benchmark::kMillisecond);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
}

