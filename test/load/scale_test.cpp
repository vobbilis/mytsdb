#include <benchmark/benchmark.h>
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/storage/filtering_storage.h"
#include "tsdb/storage/derived_metrics.h"
#include "tsdb/storage/rule_manager.h"
#include "tsdb/storage/atomic_metrics.h"
#include "tsdb/core/types.h"
#include "tsdb/common/logger.h"
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <filesystem>
#include <iostream>

using namespace tsdb;
using namespace tsdb::storage;

class MockStorage : public Storage {
public:
    core::Result<void> init(const core::StorageConfig& config) override { return core::Result<void>(); }
    core::Result<void> write(const core::TimeSeries& series) override { return core::Result<void>(); }
    core::Result<core::TimeSeries> read(const core::Labels& labels, int64_t start, int64_t end) override { return core::Result<core::TimeSeries>(core::TimeSeries(labels)); }
    core::Result<std::vector<core::TimeSeries>> query(const std::vector<core::LabelMatcher>& matchers, int64_t start, int64_t end) override { return core::Result<std::vector<core::TimeSeries>>(std::vector<core::TimeSeries>{}); }
    core::Result<std::vector<std::string>> label_names() override { return core::Result<std::vector<std::string>>(std::vector<std::string>{}); }
    core::Result<std::vector<std::string>> label_values(const std::string& label_name) override { return core::Result<std::vector<std::string>>(std::vector<std::string>{}); }
    core::Result<void> delete_series(const std::vector<core::LabelMatcher>& matchers) override { return core::Result<void>(); }
    core::Result<void> compact() override { return core::Result<void>(); }
    core::Result<void> flush() override { return core::Result<void>(); }
    core::Result<void> close() override { return core::Result<void>(); }
    std::string stats() const override { return "mock"; }
    core::Result<std::vector<core::TimeSeries>> query_aggregate(const std::vector<core::LabelMatcher>& matchers, int64_t start, int64_t end, const core::AggregationRequest& aggregation) override { return core::Result<std::vector<core::TimeSeries>>(std::vector<core::TimeSeries>{}); }
};

class ScaleTest : public benchmark::Fixture {
protected:
    std::string test_dir_;
    std::shared_ptr<MockStorage> base_storage_;
    std::shared_ptr<RuleManager> rule_manager_;
    std::shared_ptr<DerivedMetricManager> derived_manager_;
    std::shared_ptr<FilteringStorage> filtering_storage_;
    std::vector<core::TimeSeries> test_series_;

    void SetUp(const ::benchmark::State& state) override {
        (void)state;
        test_dir_ = std::filesystem::temp_directory_path().string() + "/tsdb_scale_test_" + std::to_string(std::rand());
        std::filesystem::create_directories(test_dir_);

        // 1. Configure Storage
        base_storage_ = std::make_shared<MockStorage>();
        
        // 2. Setup Managers
        rule_manager_ = std::make_shared<RuleManager>();
        derived_manager_ = std::make_shared<DerivedMetricManager>(std::static_pointer_cast<Storage>(base_storage_), nullptr);
        
        // 3. Setup Filtering Storage (wraps base)
        filtering_storage_ = std::make_shared<FilteringStorage>(
            std::static_pointer_cast<Storage>(base_storage_), 
            rule_manager_);

        // 4. Enable Metrics
        internal::GlobalMetrics::initialize();
        internal::GlobalMetrics::getInstance().updateConfig({true, true, true, true, 1000, true});
        internal::GlobalMetrics::reset();
    }

    void TearDown(const ::benchmark::State& state) override {
        (void)state;
        if (base_storage_) {
            base_storage_->close();
        }
        filtering_storage_.reset();
        derived_manager_.reset();
        rule_manager_.reset();
        base_storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }

    void GenerateData(int num_series) {
        test_series_.clear();
        test_series_.reserve(num_series);
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        for (int i = 0; i < num_series; ++i) {
            core::Labels labels;
            // Mix of metrics to test filtering
            if (i % 10 == 0) {
                labels.add("__name__", "debug_metric_" + std::to_string(i)); // Candidate for dropping
            } else {
                labels.add("__name__", "http_request_duration_seconds");
            }
            labels.add("job", "service_" + std::to_string(i % 10));
            labels.add("instance", "host_" + std::to_string(i % 100));
            labels.add("status", (i % 2 == 0) ? "200" : "500");
            
            core::TimeSeries series(labels);
            series.add_sample(now, static_cast<double>(i));
            test_series_.push_back(series);
        }
    }
};

// Scale Test: High Throughput Ingestion with Rules
BENCHMARK_DEFINE_F(ScaleTest, IngestionWithRules)(benchmark::State& state) {
    // Setup Rules
    // 1. Drop all "debug_*" metrics (approx 10% of data)
    rule_manager_->add_drop_rule("{__name__=~\"debug_.*\"}");
    
    // 2. Derived Metric: Calculate error rate (placeholder logic for now)
    // In a real scenario, we would register a derived metric rule here.
    // For now, we simulate the overhead by writing to derived_manager manually if needed,
    // but the architecture integrates it via separate path or hook.
    // Since DerivedMetricManager listens to storage or is called explicitly, 
    // let's assume standard write path for now.
    
    GenerateData(state.range(0));
    
    for (auto _ : state) {
        // Simulate batch write
        int count = 0;
        for (const auto& series : test_series_) {
            auto result = filtering_storage_->write(series);
            if (!result.ok()) {
                state.SkipWithError(result.error().c_str());
                break;
            }
            count++;
        }
        // std::cout << "Wrote " << count << " series" << std::endl;
    }
    
    // Report Metrics
    auto snapshot = internal::GlobalMetrics::getSnapshot();
    state.counters["Dropped"] = snapshot.dropped_samples;
    state.counters["Written"] = snapshot.write_count;
    state.counters["DropRate"] = static_cast<double>(snapshot.dropped_samples) / (snapshot.dropped_samples + snapshot.write_count);
}

// Register Benchmark with different batch sizes
BENCHMARK_REGISTER_F(ScaleTest, IngestionWithRules)
    ->Range(10, 100) // Reduced range for debugging
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
