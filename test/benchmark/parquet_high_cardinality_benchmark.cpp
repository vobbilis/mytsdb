#include <benchmark/benchmark.h>
#include <filesystem>
#include <chrono>
#include <random>
#include <iostream>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"

namespace tsdb {
namespace storage {

class HighCardinalityFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        test_dir_ = "benchmark_data/high_cardinality_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        core::StorageConfig config;
        config.data_dir = test_dir_;
        config.retention_period = 24 * 3600 * 1000;
        
        storage_ = std::make_unique<StorageImpl>(config);
        auto init_res = storage_->init(config);
        if (!init_res.ok()) {
            std::cerr << "Failed to init storage: " << init_res.error() << std::endl;
            std::exit(1);
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        storage_->close();
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    std::unique_ptr<StorageImpl> storage_;
};

BENCHMARK_DEFINE_F(HighCardinalityFixture, SchemaEvolution10M)(benchmark::State& state) {
    // 10M samples, changing dimensions every 10k samples
    const int total_samples = 10000000;
    const int change_interval = 10000;
    const int batch_size = 1000;
    
    core::Labels labels({{"metric", "benchmark_10m"}, {"host", "bench_host"}});
    int64_t start_time = 1000;
    
    for (auto _ : state) {
        core::TimeSeries series(labels);
        int current_dim_idx = 0;
        
        for (int i = 0; i < total_samples; ++i) {
            core::Fields fields;
            fields["trace_id"] = "trace_" + std::to_string(i);
            
            // Change schema every 10k samples
            if (i % change_interval == 0) {
                current_dim_idx++;
            }
            fields["dynamic_dim_" + std::to_string(current_dim_idx)] = "val_" + std::to_string(i);
            
            series.add_sample(start_time + i * 10, 1.0 + i, fields);
            
            // Write in batches
            if (series.samples().size() >= batch_size) {
                auto res = storage_->write(series);
                if (!res.ok()) {
                    state.SkipWithError(res.error().c_str());
                    break;
                }
                series = core::TimeSeries(labels);
            }
            
            // Progress reporting
            if (i > 0 && i % 100000 == 0) {
                std::cout << "\rProcessed " << i << " samples (" << (i * 100 / total_samples) << "%)" << std::flush;
            }
        }
        
        // Write remaining
        if (!series.empty()) {
            auto res = storage_->write(series);
            if (!res.ok()) {
                state.SkipWithError(res.error().c_str());
            }
        }
        
        // Flush to Parquet
        auto flush_res = storage_->flush();
        if (!flush_res.ok()) state.SkipWithError(flush_res.error().c_str());
        
        auto bg_flush_res = storage_->execute_background_flush(0);
        if (!bg_flush_res.ok()) state.SkipWithError(bg_flush_res.error().c_str());
        
        // Verify Parquet files
        std::string parquet_dir = test_dir_ + "/2"; // Cold tier
        size_t file_count = 0;
        size_t total_size = 0;
        
        if (std::filesystem::exists(parquet_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(parquet_dir)) {
                if (entry.path().extension() == ".parquet") {
                    file_count++;
                    total_size += entry.file_size();
                }
            }
        }
        
        std::cout << "\nBenchmark Result Verification:" << std::endl;
        std::cout << "  Parquet Files Created: " << file_count << std::endl;
        std::cout << "  Total Parquet Size: " << (total_size / 1024 / 1024) << " MB" << std::endl;
        std::cout << "  Average File Size: " << (file_count > 0 ? total_size / file_count / 1024 : 0) << " KB" << std::endl;
        
        if (file_count == 0) {
            state.SkipWithError("No Parquet files were created!");
        }
    }
    
    state.SetItemsProcessed(state.iterations() * total_samples);
}

// Register the benchmark
BENCHMARK_REGISTER_F(HighCardinalityFixture, SchemaEvolution10M)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1); // Run once as it takes time

} // namespace storage
} // namespace tsdb

BENCHMARK_MAIN();
