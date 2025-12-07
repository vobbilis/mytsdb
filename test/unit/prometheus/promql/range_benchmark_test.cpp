#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>
#include <vector>
#include <iostream>

#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/storage/adapter.h"
#include "tsdb/prometheus/model/types.h"
#include "tsdb/prometheus/promql/parser.h"
#include "tsdb/prometheus/promql/lexer.h"

namespace tsdb {
namespace prometheus {
namespace promql {
namespace test {

// Mock storage adapter for benchmarking
class BenchmarkStorageAdapter : public storage::StorageAdapter {
public:
    BenchmarkStorageAdapter() {
        // Pre-generate some data
        for (int i = 0; i < 1000; i++) {
            Series s;
            s.metric.AddLabel("__name__", "test_metric");
            s.metric.AddLabel("instance", "inst-" + std::to_string(i));
            // Add samples for a 24h range
            int64_t base_ts = 1600000000000;
            for (int64_t t = 0; t < 24 * 60; t++) {
                s.samples.emplace_back(base_ts + t * 60000, 1.0);
            }
            data_.push_back(std::move(s));
        }
    }

    Matrix SelectSeries(const std::vector<model::LabelMatcher>& matchers, 
                        int64_t start, int64_t end) override {
        // Naive scan for benchmark
        Matrix result;
        for (const auto& s : data_) {
            bool matches = true;
            for (const auto& m : matchers) {
                if (m.name == "__name__" && m.value != "test_metric") {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                Series res_series;
                res_series.metric = s.metric;
                // Filter samples
                for (const auto& samp : s.samples) {
                    if (samp.timestamp() >= start && samp.timestamp() <= end) {
                        res_series.samples.push_back(samp);
                    }
                }
                if (!res_series.samples.empty()) {
                    result.push_back(std::move(res_series));
                }
            }
        }
        return result;
    }
    
    Matrix SelectAggregateSeries(const std::vector<model::LabelMatcher>&,
                                 int64_t, int64_t,
                                 const core::AggregationRequest&) override {
        return {};
    }
    
    std::vector<std::string> LabelNames() override { return {}; }
    std::vector<std::string> LabelValues(const std::string&) override { return {}; }

private:
    Matrix data_;
};

// Global setup
BenchmarkStorageAdapter* g_adapter = nullptr;

void SetupAdapter() {
    if (!g_adapter) {
        g_adapter = new BenchmarkStorageAdapter();
    }
}

// Old approach: Manual loop with aggregation
static void BM_RangeQuery_OldLoop(benchmark::State& state) {
    SetupAdapter();
    
    int64_t start = 1600000000000;
    int64_t end = start + 3600000; // 1 hour
    int64_t step = 60000; // 1 minute
    
    std::string query = "test_metric";
    Lexer lexer(query);
    Parser parser(lexer);
    auto ast = parser.ParseExpr();
    
    for (auto _ : state) {
        struct LabelSetComparator {
            bool operator()(const LabelSet& a, const LabelSet& b) const {
                return a.labels() < b.labels();
            }
        };
        std::map<LabelSet, Series, LabelSetComparator> seriesMap;

        // Mimic old engine loop
        for (int64_t t = start; t <= end; t += step) {
             Evaluator evaluator(t, 300000, g_adapter);
             Value val = evaluator.Evaluate(ast.get());
             
             if (val.isVector()) {
                const Vector& vec = val.getVector();
                for (const auto& sample : vec) {
                    auto& series = seriesMap[sample.metric];
                    if (series.metric.labels().empty()) {
                            series.metric = sample.metric;
                    }
                    series.samples.emplace_back(t, sample.value);
                }
             }
        }
        benchmark::DoNotOptimize(seriesMap);
    }
}
BENCHMARK(BM_RangeQuery_OldLoop);

// New approach: EvaluateRange
static void BM_RangeQuery_NewRange(benchmark::State& state) {
    SetupAdapter();
    
    int64_t start = 1600000000000;
    int64_t end = start + 3600000; // 1 hour
    int64_t step = 60000; // 1 minute
    
    std::string query = "test_metric";
    Lexer lexer(query);
    Parser parser(lexer);
    auto ast = parser.ParseExpr();
    
    for (auto _ : state) {
        Evaluator evaluator(start, end, step, 300000, g_adapter);
        Value v = evaluator.EvaluateRange(ast.get());
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_RangeQuery_NewRange);

static void BM_Rate_OldLoop(benchmark::State& state) {
    SetupAdapter();
    int64_t start = 1600000000000;
    int64_t end = start + 3600000; // 1h
    int64_t step = 60000;
    
    std::string query = "rate(test_metric[5m])";
    Lexer lexer(query);
    Parser parser(lexer);
    auto ast = parser.ParseExpr();
    
    for (auto _ : state) {
        std::map<LabelSet, Series> seriesMap;
        for (int64_t t = start; t <= end; t += step) {
             Evaluator evaluator(t, 300000, g_adapter);
             Value val = evaluator.Evaluate(ast.get());
             if (val.isVector()) {
                 benchmark::DoNotOptimize(val);
             }
        }
    }
}
BENCHMARK(BM_Rate_OldLoop);

static void BM_Rate_New(benchmark::State& state) {
    SetupAdapter();
    int64_t start = 1600000000000;
    int64_t end = start + 3600000;
    int64_t step = 60000;
    
    std::string query = "rate(test_metric[5m])";
    Lexer lexer(query);
    Parser parser(lexer);
    auto ast = parser.ParseExpr();
    
    for (auto _ : state) {
        Evaluator evaluator(start, end, step, 300000, g_adapter);
        Value v = evaluator.EvaluateRange(ast.get());
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_Rate_New);

static void BM_SumRate_OldLoop(benchmark::State& state) {
    SetupAdapter();
    int64_t start = 1600000000000;
    int64_t end = start + 3600000;
    int64_t step = 60000;
    
    std::string query = "sum(rate(test_metric[5m]))";
    Lexer lexer(query);
    Parser parser(lexer);
    auto ast = parser.ParseExpr();
    
    for (auto _ : state) {
        for (int64_t t = start; t <= end; t += step) {
             Evaluator evaluator(t, 300000, g_adapter);
             Value val = evaluator.Evaluate(ast.get());
             if (val.isVector()) { // sum returns vector
                 benchmark::DoNotOptimize(val);
             }
        }
    }
}
BENCHMARK(BM_SumRate_OldLoop);

static void BM_SumRate_New(benchmark::State& state) {
    SetupAdapter();
    int64_t start = 1600000000000;
    int64_t end = start + 3600000;
    int64_t step = 60000;
    
    std::string query = "sum(rate(test_metric[5m]))";
    Lexer lexer(query);
    Parser parser(lexer);
    auto ast = parser.ParseExpr();
    
    for (auto _ : state) {
        Evaluator evaluator(start, end, step, 300000, g_adapter);
        Value v = evaluator.EvaluateRange(ast.get());
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_SumRate_New);

} // namespace test
} // namespace promql
} // namespace prometheus
} // namespace tsdb

BENCHMARK_MAIN();
