#include <benchmark/benchmark.h>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <algorithm>
#include <iostream>

#include "tsdb/prometheus/model/types.h"
// #include "tsdb/prometheus/promql/types.h" // Removed

// Mock dependencies
namespace tsdb {
namespace prometheus {
namespace promql {
    using Matrix = std::vector<int>; // Dummy
}
namespace storage {
    class StorageAdapter {
    public:
        virtual ~StorageAdapter() = default;
        virtual promql::Matrix SelectSeries(const std::vector<model::LabelMatcher>&, int64_t, int64_t) { return {}; }
    };
}
}
}

using namespace tsdb::prometheus;

// Helper to serialize matchers (same as in engine.cpp)
std::string SerializeMatchers(const std::vector<model::LabelMatcher>& matchers) {
    std::string s;
    for (const auto& m : matchers) {
        s += std::to_string(static_cast<int>(m.type)) + ":" + m.name + "=" + m.value + ";";
    }
    return s;
}

// Legacy Implementation (Linear Scan)
class LegacyCache {
public:
    struct CacheEntry {
        int64_t start;
        int64_t end;
        std::vector<model::LabelMatcher> matchers; // Need to store matchers for linear scan
        // Data omitted for benchmark
    };

    void Buffer(const std::vector<model::LabelMatcher>& matchers, int64_t start, int64_t end) {
        // Linear scan to check if exists
        std::string key = SerializeMatchers(matchers);
        for (const auto& entry : entries_) {
            if (SerializeMatchers(entry.matchers) == key) {
                // Simplified check
                return;
            }
        }
        entries_.push_back({start, end, matchers});
    }

    bool Find(const std::vector<model::LabelMatcher>& matchers, int64_t start, int64_t end) {
        std::string key = SerializeMatchers(matchers);
        for (const auto& entry : entries_) {
            // In the real legacy implementation, it reconstructed the key or compared matchers
            // Here we simulate the O(N) string comparison
            if (SerializeMatchers(entry.matchers) == key) {
                if (entry.start <= start && entry.end >= end) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    std::vector<CacheEntry> entries_;
};

// Optimized Implementation (Two-Level Map)
class OptimizedCache {
public:
    struct CacheEntry {
        int64_t start;
        int64_t end;
    };

    void Buffer(const std::vector<model::LabelMatcher>& matchers, int64_t start, int64_t end) {
        std::string key = SerializeMatchers(matchers);
        cache_[key].push_back({start, end});
    }

    bool Find(const std::vector<model::LabelMatcher>& matchers, int64_t start, int64_t end) {
        std::string key = SerializeMatchers(matchers);
        auto it = cache_.find(key);
        if (it == cache_.end()) return false;

        for (const auto& entry : it->second) {
            if (entry.start <= start && entry.end >= end) {
                return true;
            }
        }
        return false;
    }

private:
    std::map<std::string, std::vector<CacheEntry>> cache_;
};

// Benchmark Fixture
class CacheBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) {
        // Generate N distinct matcher sets
        int N = state.range(0);
        for (int i = 0; i < N; ++i) {
            std::vector<model::LabelMatcher> m;
            m.push_back({model::MatcherType::EQUAL, "__name__", "metric_" + std::to_string(i)});
            m.push_back({model::MatcherType::EQUAL, "job", "job_" + std::to_string(i % 10)});
            matchers_list_.push_back(m);
        }
    }

    std::vector<std::vector<model::LabelMatcher>> matchers_list_;
};

// Benchmark Legacy Lookup
BENCHMARK_DEFINE_F(CacheBenchmark, LegacyLookup)(benchmark::State& state) {
    LegacyCache cache;
    // Populate cache
    for (const auto& m : matchers_list_) {
        cache.Buffer(m, 1000, 2000);
    }

    // Measure lookup time
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, matchers_list_.size() - 1);

    for (auto _ : state) {
        int idx = dis(gen);
        bool found = cache.Find(matchers_list_[idx], 1500, 1600);
        benchmark::DoNotOptimize(found);
    }
}

// Benchmark Optimized Lookup
BENCHMARK_DEFINE_F(CacheBenchmark, OptimizedLookup)(benchmark::State& state) {
    OptimizedCache cache;
    // Populate cache
    for (const auto& m : matchers_list_) {
        cache.Buffer(m, 1000, 2000);
    }

    // Measure lookup time
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, matchers_list_.size() - 1);

    for (auto _ : state) {
        int idx = dis(gen);
        bool found = cache.Find(matchers_list_[idx], 1500, 1600);
        benchmark::DoNotOptimize(found);
    }
}

// Register benchmarks with varying N (number of cache entries)
BENCHMARK_REGISTER_F(CacheBenchmark, LegacyLookup)->Range(10, 10000);
BENCHMARK_REGISTER_F(CacheBenchmark, OptimizedLookup)->Range(10, 10000);

BENCHMARK_MAIN();
