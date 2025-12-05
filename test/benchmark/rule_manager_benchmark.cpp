#include <benchmark/benchmark.h>
#include "tsdb/storage/rule_manager.h"
#include "tsdb/core/types.h"
#include <vector>
#include <string>
#include <random>

using namespace tsdb;
using namespace tsdb::storage;

class RuleManagerBenchmark : public benchmark::Fixture {
protected:
    std::shared_ptr<RuleManager> rule_manager_;
    std::vector<core::TimeSeries> test_series_;

    void SetUp(const ::benchmark::State& state) override {
        rule_manager_ = std::make_shared<RuleManager>();
        
        // Generate test series
        int num_series = 1000;
        test_series_.reserve(num_series);
        for (int i = 0; i < num_series; ++i) {
            core::Labels labels;
            labels.add("__name__", "metric_" + std::to_string(i));
            labels.add("job", "service_" + std::to_string(i % 10));
            labels.add("instance", "host_" + std::to_string(i % 100));
            labels.add("env", (i % 2 == 0) ? "prod" : "dev");
            test_series_.emplace_back(labels);
        }
    }
};

// Benchmark No Rules
BENCHMARK_DEFINE_F(RuleManagerBenchmark, NoRules)(benchmark::State& state) {
    int idx = 0;
    for (auto _ : state) {
        auto rules = rule_manager_->get_current_rules();
        if (rules) {
            benchmark::DoNotOptimize(rules->should_drop(test_series_[idx % test_series_.size()]));
        }
        idx++;
    }
}
BENCHMARK_REGISTER_F(RuleManagerBenchmark, NoRules);

// Benchmark Exact Name Match
BENCHMARK_DEFINE_F(RuleManagerBenchmark, ExactNameMatch)(benchmark::State& state) {
    // Add some exact match rules
    for (int i = 0; i < 100; ++i) {
        rule_manager_->add_drop_rule("metric_" + std::to_string(i));
    }
    
    int idx = 0;
    for (auto _ : state) {
        auto rules = rule_manager_->get_current_rules();
        if (rules) {
            benchmark::DoNotOptimize(rules->should_drop(test_series_[idx % test_series_.size()]));
        }
        idx++;
    }
}
BENCHMARK_REGISTER_F(RuleManagerBenchmark, ExactNameMatch);

// Benchmark Regex Match
BENCHMARK_DEFINE_F(RuleManagerBenchmark, RegexMatch)(benchmark::State& state) {
    // Add regex rules
    rule_manager_->add_drop_rule("{__name__=~\"metric_1.*\"}"); // Matches metric_100, metric_101...
    
    int idx = 0;
    for (auto _ : state) {
        auto rules = rule_manager_->get_current_rules();
        if (rules) {
            benchmark::DoNotOptimize(rules->should_drop(test_series_[idx % test_series_.size()]));
        }
        idx++;
    }
}
BENCHMARK_REGISTER_F(RuleManagerBenchmark, RegexMatch);

// Benchmark Label Match
BENCHMARK_DEFINE_F(RuleManagerBenchmark, LabelMatch)(benchmark::State& state) {
    // Add label rules
    rule_manager_->add_drop_rule("{env=\"dev\"}");
    
    int idx = 0;
    for (auto _ : state) {
        auto rules = rule_manager_->get_current_rules();
        if (rules) {
            benchmark::DoNotOptimize(rules->should_drop(test_series_[idx % test_series_.size()]));
        }
        idx++;
    }
}
BENCHMARK_REGISTER_F(RuleManagerBenchmark, LabelMatch);

BENCHMARK_MAIN();
