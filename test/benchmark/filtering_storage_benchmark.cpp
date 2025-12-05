#include <benchmark/benchmark.h>
#include "tsdb/storage/storage.h"
#include "tsdb/storage/filtering_storage.h"
#include "tsdb/storage/rule_manager.h"
#include "tsdb/core/types.h"
#include <memory>
#include <vector>
#include <chrono>

using namespace tsdb;
using namespace tsdb::storage;

// Mock Storage for benchmarking overhead
class MockStorage : public Storage {
public:
    core::Result<void> init(const core::StorageConfig& config) override {
        return core::Result<void>();
    }
    
    core::Result<void> write(const core::TimeSeries& series) override {
        // No-op for benchmarking
        return core::Result<void>();
    }
    
    core::Result<core::TimeSeries> read(const core::Labels& labels, int64_t start, int64_t end) override {
        return core::Result<core::TimeSeries>(core::TimeSeries(labels));
    }
    
    core::Result<std::vector<core::TimeSeries>> query(const std::vector<core::LabelMatcher>& matchers, int64_t start, int64_t end) override {
        return core::Result<std::vector<core::TimeSeries>>(std::vector<core::TimeSeries>{});
    }
    
    core::Result<std::vector<std::string>> label_names() override {
        return core::Result<std::vector<std::string>>(std::vector<std::string>{});
    }
    
    core::Result<std::vector<std::string>> label_values(const std::string& label_name) override {
        return core::Result<std::vector<std::string>>(std::vector<std::string>{});
    }
    
    core::Result<void> delete_series(const std::vector<core::LabelMatcher>& matchers) override {
        return core::Result<void>();
    }
    
    core::Result<void> compact() override {
        return core::Result<void>();
    }
    
    core::Result<void> flush() override {
        return core::Result<void>();
    }
    
    core::Result<void> close() override {
        return core::Result<void>();
    }
    
    std::string stats() const override {
        return "mock";
    }
};

class FilteringStorageBenchmark : public benchmark::Fixture {
protected:
    std::shared_ptr<MockStorage> base_storage_;
    std::shared_ptr<RuleManager> rule_manager_;
    std::shared_ptr<FilteringStorage> filtering_storage_;
    std::vector<core::TimeSeries> test_series_;

    void SetUp(const ::benchmark::State& state) override {
        (void)state;
        
        base_storage_ = std::make_shared<MockStorage>();
        
        rule_manager_ = std::make_shared<RuleManager>();
        
        filtering_storage_ = std::make_shared<FilteringStorage>(
            std::static_pointer_cast<Storage>(base_storage_), 
            rule_manager_);
            
        // Generate test series
        int num_series = 1000;
        test_series_.reserve(num_series);
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        for (int i = 0; i < num_series; ++i) {
            core::Labels labels;
            labels.add("__name__", "metric_" + std::to_string(i));
            labels.add("job", "service_" + std::to_string(i % 10));
            labels.add("instance", "host_" + std::to_string(i % 100));
            
            core::TimeSeries series(labels);
            series.add_sample(now, static_cast<double>(i));
            test_series_.push_back(series);
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        (void)state;
        filtering_storage_.reset();
        base_storage_.reset();
    }
};

// Benchmark Direct Write (Baseline)
BENCHMARK_DEFINE_F(FilteringStorageBenchmark, DirectWrite)(benchmark::State& state) {
    int idx = 0;
    for (auto _ : state) {
        auto result = base_storage_->write(test_series_[idx % test_series_.size()]);
        if (!result.ok()) {
            state.SkipWithError(result.error().c_str());
            break;
        }
        idx++;
    }
}
BENCHMARK_REGISTER_F(FilteringStorageBenchmark, DirectWrite);

// Benchmark Filtering Write (No Rules - 0% Drop)
BENCHMARK_DEFINE_F(FilteringStorageBenchmark, FilteringWriteNoDrop)(benchmark::State& state) {
    int idx = 0;
    for (auto _ : state) {
        auto result = filtering_storage_->write(test_series_[idx % test_series_.size()]);
        if (!result.ok()) {
            state.SkipWithError(result.error().c_str());
            break;
        }
        idx++;
    }
}
BENCHMARK_REGISTER_F(FilteringStorageBenchmark, FilteringWriteNoDrop);

// Benchmark Filtering Write (All Rules - 100% Drop)
BENCHMARK_DEFINE_F(FilteringStorageBenchmark, FilteringWriteAllDrop)(benchmark::State& state) {
    // Add rule to drop everything (using exact match for stability check)
    // We'll add a rule that matches the first metric
    rule_manager_->add_drop_rule("metric_0");
    
    int idx = 0;
    for (auto _ : state) {
        auto result = filtering_storage_->write(test_series_[idx % test_series_.size()]);
        if (!result.ok()) {
            state.SkipWithError(result.error().c_str());
            break;
        }
        idx++;
    }
}
BENCHMARK_REGISTER_F(FilteringStorageBenchmark, FilteringWriteAllDrop);

BENCHMARK_MAIN();
