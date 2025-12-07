# Read Performance Optimization Plan

**Created**: December 6, 2025  
**Status**: Planning  
**Priority**: P0 - Critical  

## Executive Summary

Benchmark results show read latency p99 at **~3.9 seconds** - approximately 100x slower than production TSDBs. This document outlines a systematic approach to optimize read performance with measurable before/after metrics.

---

## Current State Analysis

### Benchmark Baseline (December 5, 2025)

| Metric | Quick (12 pods) | Small (150 pods) | Medium (2,400 pods) |
|--------|-----------------|------------------|---------------------|
| Query Throughput | 10.4 qps | 9.4 qps | 6.8 qps |
| p50 Latency | 53 ms | 60 ms | 76 ms |
| p99 Latency | 3,894 ms | 3,775 ms | 3,919 ms |
| Max Latency | 3,922 ms | 3,975 ms | 4,319 ms |

### Target Performance (Industry Standard)

| Metric | Target | Rationale |
|--------|--------|-----------|
| Query Throughput | ≥100 qps | Prometheus baseline |
| p50 Latency | ≤50 ms | Grafana dashboard responsiveness |
| p99 Latency | ≤500 ms | SLA compliance |
| Max Latency | ≤2,000 ms | Worst-case acceptable |

### Identified Root Causes

1. **Range Query Step Loop** - O(N) AST evaluations for N time steps
2. **No Time-Indexed Block Reads** - Full block scan for overlapping blocks
3. **No Query Result Cache** - Identical queries hit storage every time
4. **Sequential Block I/O** - No parallel Parquet reads

---

## Optimization Phases

### Phase 0: Establish Ground Truth Testing Framework

**Goal**: Create reproducible benchmarks to measure improvements

#### 0.1 Micro-Benchmark Suite

Create targeted unit tests that isolate each component:

```cpp
// File: test/benchmark/read_performance_benchmark.cpp

// Test 1: Raw storage read latency (no PromQL)
TEST(ReadPerformanceBenchmark, StorageReadLatency) {
    // Measures: storage_->read(labels, start, end)
    // Baseline target: Record current p50/p99
}

// Test 2: Block scan performance
TEST(ReadPerformanceBenchmark, BlockScanLatency) {
    // Measures: read_from_blocks_nolock() with varying block counts
    // Baseline: 10, 100, 1000 blocks
}

// Test 3: Range query step overhead
TEST(ReadPerformanceBenchmark, RangeQueryStepOverhead) {
    // Measures: ExecuteRange() with varying step counts
    // Baseline: 10, 50, 100, 500 steps
}

// Test 4: Parquet read cold path
TEST(ReadPerformanceBenchmark, ParquetReadLatency) {
    // Measures: parquet_block->read() vs parquet_block->query()
    // Baseline: 1MB, 10MB, 100MB files
}

// Test 5: Cache hit ratio impact
TEST(ReadPerformanceBenchmark, CacheHitRatioImpact) {
    // Measures: 0%, 50%, 100% cache hit scenarios
}
```

#### 0.2 Integration Benchmark Suite

```cpp
// File: test/benchmark/e2e_query_benchmark.cpp

class E2EQueryBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        // Load 100K time series with 1 hour of data
        // 10 samples per series = 1M total samples
    }
    
    // Benchmark instant queries
    void BenchmarkInstantQuery(const std::string& query, int iterations);
    
    // Benchmark range queries with varying parameters
    void BenchmarkRangeQuery(const std::string& query, 
                             int64_t range_seconds, 
                             int64_t step_seconds,
                             int iterations);
};

TEST_F(E2EQueryBenchmark, InstantQuery_SimpleSelector) {
    // Query: container_cpu_usage_seconds_total{namespace="kube-system"}
    // Expected: <10ms p99
}

TEST_F(E2EQueryBenchmark, InstantQuery_RateFunction) {
    // Query: rate(container_cpu_usage_seconds_total[5m])
    // Expected: <50ms p99
}

TEST_F(E2EQueryBenchmark, RangeQuery_1Hour_1MinStep) {
    // 60 steps, measures per-step overhead
    // Expected: <500ms total
}

TEST_F(E2EQueryBenchmark, RangeQuery_24Hour_30MinStep) {
    // 48 steps, cold data
    // Expected: <2000ms total
}

TEST_F(E2EQueryBenchmark, RangeQuery_24Hour_1MinStep) {
    // 1440 steps - stress test
    // Expected: <10000ms total (before optimization)
}
```

#### 0.3 Baseline Recording Script

```bash
#!/bin/bash
# File: scripts/record_read_baseline.sh

echo "=== Read Performance Baseline Recording ==="
echo "Date: $(date)"
echo "Git SHA: $(git rev-parse HEAD)"

# Build with release optimizations
cmake -DCMAKE_BUILD_TYPE=Release -B build_release
cmake --build build_release -j$(nproc)

# Run micro-benchmarks
./build_release/test/read_performance_benchmark \
    --benchmark_out=benchmark_results/baseline_micro_$(date +%Y%m%d).json \
    --benchmark_out_format=json

# Run E2E benchmarks
./build_release/test/e2e_query_benchmark \
    --benchmark_out=benchmark_results/baseline_e2e_$(date +%Y%m%d).json \
    --benchmark_out_format=json

# Run K8s combined benchmark
./build_release/tools/k8s_combined_benchmark --preset=small \
    > benchmark_results/baseline_k8s_$(date +%Y%m%d).log 2>&1

echo "Baseline recorded to benchmark_results/"
```

---

### Phase 1: Range Query Optimization (P0)

**Expected Improvement**: 10-50x for range queries

#### 1.1 Problem Statement

Current implementation evaluates AST per time step:

```cpp
// Current: O(steps * AST_eval_cost)
for (int64_t t = start; t <= end; t += step) {
    Evaluator evaluator(t, ...);
    Value val = evaluator.Evaluate(ast.get());
}
```

#### 1.2 Solution: Range-Aware Evaluation

```cpp
// New: O(AST_eval_cost) with range-aware functions
class RangeEvaluator {
public:
    RangeEvaluator(int64_t start, int64_t end, int64_t step, StorageAdapter* storage);
    
    // Returns Matrix directly instead of looping
    Matrix EvaluateRange(ASTNode* ast);
    
private:
    // Bulk fetch all data once
    void PrefetchData(const std::vector<SelectorContext>& selectors);
    
    // Apply function to all steps in one pass
    Matrix ApplyRangeFunction(const std::string& func, const Matrix& input);
};
```

#### 1.3 Implementation Steps

| Step | Description | Files Modified | Test Coverage |
|------|-------------|----------------|---------------|
| 1.1.1 | Create RangeEvaluator class | `src/tsdb/prometheus/promql/range_evaluator.cpp` | Unit test |
| 1.1.2 | Implement bulk data prefetch | `src/tsdb/prometheus/promql/range_evaluator.cpp` | Unit test |
| 1.1.3 | Implement range-aware rate() | `src/tsdb/prometheus/promql/functions/rate.cpp` | Unit test |
| 1.1.4 | Integrate with ExecuteRange | `src/tsdb/prometheus/promql/engine.cpp` | Integration test |
| 1.1.5 | Add fallback for unsupported functions | `src/tsdb/prometheus/promql/engine.cpp` | Unit test |

#### 1.4 Tests

```cpp
// File: test/unit/prometheus/promql/range_evaluator_test.cpp

TEST(RangeEvaluatorTest, BulkPrefetch_SingleSelector) {
    // Verify single storage call for entire range
    MockStorageAdapter mock;
    EXPECT_CALL(mock, Query(_, start, end, _)).Times(1);
    
    RangeEvaluator eval(start, end, step, &mock);
    eval.EvaluateRange(ast);
}

TEST(RangeEvaluatorTest, RangeRate_CorrectValues) {
    // Verify rate calculation matches step-by-step evaluation
    // Compare: new RangeEvaluator vs old step loop
}

TEST(RangeEvaluatorTest, Performance_1440Steps) {
    // 24h with 1m step
    auto start = std::chrono::high_resolution_clock::now();
    eval.EvaluateRange(ast);
    auto duration = std::chrono::high_resolution_clock::now() - start;
    
    // Must complete in <1s (vs ~60s with old approach)
    EXPECT_LT(duration, std::chrono::seconds(1));
}
```

---

### Phase 2: Time-Indexed Block Reads (P1)

**Expected Improvement**: 2-5x for block reads

#### 2.1 Problem Statement

Current block read scans all samples:

```cpp
// Current: O(N) where N = total samples in block
for (const auto& sample : block_series.samples()) {
    if (sample.timestamp() >= start && sample.timestamp() <= end) {
        result.push_back(sample);
    }
}
```

#### 2.2 Solution: Skip-List Index

```cpp
class BlockImpl {
    // New: Sparse timestamp index (every 128 samples)
    std::vector<std::pair<int64_t, size_t>> timestamp_index_;
    
public:
    std::vector<Sample> ReadRange(const Labels& labels, 
                                   int64_t start, int64_t end) {
        // Binary search to find start offset
        auto it = std::lower_bound(timestamp_index_.begin(), 
                                   timestamp_index_.end(),
                                   std::make_pair(start, 0));
        size_t start_offset = (it != timestamp_index_.begin()) 
                             ? (it - 1)->second : 0;
        
        // Read only from start_offset
        return ReadFromOffset(labels, start_offset, start, end);
    }
};
```

#### 2.3 Implementation Steps

| Step | Description | Files Modified | Test Coverage |
|------|-------------|----------------|---------------|
| 2.1.1 | Add timestamp_index_ to BlockImpl | `include/tsdb/storage/internal/block_impl.h` | Unit test |
| 2.1.2 | Build index during append/seal | `src/tsdb/storage/internal/block_impl.cpp` | Unit test |
| 2.1.3 | Implement ReadRange with binary search | `src/tsdb/storage/internal/block_impl.cpp` | Unit test |
| 2.1.4 | Persist index in block header | `src/tsdb/storage/internal/block_impl.cpp` | Unit test |
| 2.1.5 | Update read_from_blocks_nolock | `src/tsdb/storage/storage_impl.cpp` | Integration test |

#### 2.4 Tests

```cpp
// File: test/unit/storage/block_index_test.cpp

TEST(BlockIndexTest, BinarySearchCorrectness) {
    // Create block with 10K samples spanning 1 hour
    // Query middle 10 minutes
    // Verify only ~1.6K samples scanned (vs 10K)
}

TEST(BlockIndexTest, IndexPersistence) {
    // Seal block, reload from disk
    // Verify index reconstructed correctly
}

TEST(BlockIndexTest, Performance_LargeBlock) {
    BlockImpl block;
    // Insert 1M samples
    for (int i = 0; i < 1000000; i++) {
        block.append(labels, Sample(base_time + i * 1000, i));
    }
    block.seal();
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = block.ReadRange(labels, 
                                   base_time + 500000 * 1000,
                                   base_time + 510000 * 1000);
    auto duration = std::chrono::high_resolution_clock::now() - start;
    
    // Must complete in <1ms (vs ~50ms for full scan)
    EXPECT_LT(duration, std::chrono::milliseconds(1));
    EXPECT_EQ(result.size(), 10000);
}
```

---

### Phase 3: Query Result Cache (P2)

**Expected Improvement**: 2-10x for repeated queries

#### 3.1 Problem Statement

Identical queries within short windows hit storage every time:

```cpp
// Grafana auto-refresh every 5s sends identical query
// Each request: Parse → Evaluate → Storage I/O → Serialize
```

#### 3.2 Solution: LRU Result Cache

```cpp
class QueryResultCache {
public:
    struct CacheEntry {
        QueryResult result;
        int64_t expires_at;
        size_t size_bytes;
    };
    
    QueryResultCache(size_t max_size_bytes, int64_t default_ttl_ms);
    
    std::optional<QueryResult> Get(const std::string& query, int64_t time);
    void Put(const std::string& query, int64_t time, QueryResult result);
    
private:
    // Time bucketing: queries within same 15s window share cache
    std::string MakeCacheKey(const std::string& query, int64_t time) {
        int64_t bucket = time / 15000;
        return query + ":" + std::to_string(bucket);
    }
    
    std::unordered_map<std::string, CacheEntry> cache_;
    std::list<std::string> lru_list_;
    size_t current_size_ = 0;
    size_t max_size_;
    int64_t default_ttl_;
};
```

#### 3.3 Implementation Steps

| Step | Description | Files Modified | Test Coverage |
|------|-------------|----------------|---------------|
| 3.1.1 | Create QueryResultCache class | `src/tsdb/prometheus/promql/query_cache.cpp` | Unit test |
| 3.1.2 | Implement LRU eviction | `src/tsdb/prometheus/promql/query_cache.cpp` | Unit test |
| 3.1.3 | Integrate with Engine::ExecuteInstant | `src/tsdb/prometheus/promql/engine.cpp` | Unit test |
| 3.1.4 | Add cache invalidation on write | `src/tsdb/storage/storage_impl.cpp` | Integration test |
| 3.1.5 | Add cache metrics (hit rate, size) | `src/tsdb/prometheus/promql/engine.cpp` | Unit test |

#### 3.4 Tests

```cpp
// File: test/unit/prometheus/promql/query_cache_test.cpp

TEST(QueryCacheTest, CacheHit_SameTimeBucket) {
    QueryResultCache cache(1024 * 1024, 15000);
    
    QueryResult result1 = engine.ExecuteInstant("up", 1000000);
    cache.Put("up", 1000000, result1);
    
    // Query at 1005000 (same 15s bucket)
    auto cached = cache.Get("up", 1005000);
    EXPECT_TRUE(cached.has_value());
}

TEST(QueryCacheTest, CacheMiss_DifferentTimeBucket) {
    // Query at 1000000, then 1020000 (different bucket)
    // Should miss
}

TEST(QueryCacheTest, LRUEviction) {
    QueryResultCache cache(1000, 60000); // 1KB max
    
    // Insert entries until eviction
    for (int i = 0; i < 100; i++) {
        cache.Put("query" + std::to_string(i), 0, large_result);
    }
    
    // First entries should be evicted
    EXPECT_FALSE(cache.Get("query0", 0).has_value());
}

TEST(QueryCacheTest, InvalidationOnWrite) {
    // Write new sample for metric "up"
    // Cache entries matching "up" should be invalidated
}
```

---

### Phase 4: Parallel Block Reads (P3)

**Expected Improvement**: 2-4x for cold queries spanning multiple blocks

#### 4.1 Problem Statement

Sequential Parquet reads for multi-block queries:

```cpp
// Current: O(N * block_read_time) where N = overlapping blocks
for (const auto& block : parquet_blocks_) {
    auto samples = block->read(labels, start, end);  // Sequential
}
```

#### 4.2 Solution: Async Parallel Reads

```cpp
std::vector<Sample> ReadFromParquetBlocksParallel(
    const std::vector<ParquetBlock*>& blocks,
    const Labels& labels,
    int64_t start, int64_t end) {
    
    std::vector<std::future<std::vector<Sample>>> futures;
    
    for (const auto& block : blocks) {
        if (block->end_time() < start || block->start_time() > end) continue;
        
        futures.push_back(std::async(std::launch::async, [&]() {
            return block->query(labels, start, end);
        }));
    }
    
    // Merge results
    std::vector<Sample> result;
    for (auto& f : futures) {
        auto samples = f.get();
        result.insert(result.end(), samples.begin(), samples.end());
    }
    
    // Sort and dedup
    std::sort(result.begin(), result.end(), ...);
    return result;
}
```

#### 4.3 Implementation Steps

| Step | Description | Files Modified | Test Coverage |
|------|-------------|----------------|---------------|
| 4.1.1 | Create thread pool for block reads | `src/tsdb/storage/block_read_pool.cpp` | Unit test |
| 4.1.2 | Implement parallel read in storage_impl | `src/tsdb/storage/storage_impl.cpp` | Unit test |
| 4.1.3 | Add configurable parallelism | `include/tsdb/storage/storage_config.h` | Config test |
| 4.1.4 | Handle partial failures gracefully | `src/tsdb/storage/storage_impl.cpp` | Unit test |

#### 4.4 Tests

```cpp
// File: test/unit/storage/parallel_block_read_test.cpp

TEST(ParallelBlockReadTest, CorrectMerge) {
    // 4 blocks with overlapping data
    // Verify merged result is sorted and deduped
}

TEST(ParallelBlockReadTest, Performance_4Blocks) {
    // 4 blocks, each takes 100ms to read sequentially
    auto start = std::chrono::high_resolution_clock::now();
    auto result = ReadFromParquetBlocksParallel(blocks, labels, start, end);
    auto duration = std::chrono::high_resolution_clock::now() - start;
    
    // Should complete in ~100ms (parallel) vs 400ms (sequential)
    EXPECT_LT(duration, std::chrono::milliseconds(150));
}

TEST(ParallelBlockReadTest, PartialFailure) {
    // One block read throws exception
    // Verify other blocks still returned
}
```

---

## Integration Test Suite

### Full E2E Performance Test

```cpp
// File: test/integration/read_performance_e2e_test.cpp

class ReadPerformanceE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // 1. Start server
        // 2. Ingest 100K series with 1 hour of data
        // 3. Wait for flush to blocks
    }
    
    void TearDown() override {
        // Collect and log performance metrics
        LogPerformanceSummary();
    }
};

TEST_F(ReadPerformanceE2ETest, SLACompliance) {
    // Run 1000 queries of mixed types
    // Assert SLA targets met
    
    LatencyTracker tracker;
    
    for (int i = 0; i < 1000; i++) {
        auto start = Now();
        auto result = client.Query(GetRandomQuery());
        tracker.Record(Now() - start);
    }
    
    EXPECT_LE(tracker.P50(), 50);    // p50 ≤ 50ms
    EXPECT_LE(tracker.P99(), 500);   // p99 ≤ 500ms
    EXPECT_GE(tracker.QPS(), 100);   // ≥100 qps
}

TEST_F(ReadPerformanceE2ETest, RangeQueryPerformance) {
    // 24-hour range query should complete in <2s
    auto start = Now();
    auto result = client.QueryRange(
        "rate(container_cpu_usage_seconds_total[5m])",
        now - 86400, now, 1800  // 24h, 30m step
    );
    auto duration = Now() - start;
    
    EXPECT_LT(duration, 2000);
}

TEST_F(ReadPerformanceE2ETest, CacheEffectiveness) {
    // Run same query 100 times
    // First should be slow, rest should be fast
    
    std::vector<int64_t> latencies;
    for (int i = 0; i < 100; i++) {
        auto start = Now();
        client.Query("up");
        latencies.push_back(Now() - start);
    }
    
    // First query: cache miss
    // Subsequent: cache hits, should be <5ms
    double avg_cached = Average(latencies.begin() + 1, latencies.end());
    EXPECT_LT(avg_cached, 5);
}
```

---

## Validation Checklist

### Before Each Phase

- [ ] Run baseline benchmark script
- [ ] Record metrics to `benchmark_results/`
- [ ] Git tag: `baseline-phase-N`

### After Each Phase

- [ ] Run same benchmark script
- [ ] Compare with baseline
- [ ] Document improvement in this file
- [ ] Git tag: `optimized-phase-N`

### Final Validation

| Metric | Baseline | After Phase 1 | After Phase 2 | After Phase 3 | After Phase 4 | Target |
|--------|----------|---------------|---------------|---------------|---------------|--------|
| p50 Latency | 60ms | TBD | TBD | TBD | TBD | ≤50ms |
| p99 Latency | 3,900ms | TBD | TBD | TBD | TBD | ≤500ms |
| Throughput | 9.4 qps | TBD | TBD | TBD | TBD | ≥100 qps |
| Cache Hit Rate | 0% | 0% | 0% | TBD | TBD | ≥50% |

---

## Implementation Schedule

| Phase | Description | Estimated Effort | Dependencies |
|-------|-------------|------------------|--------------|
| Phase 0 | Ground Truth Testing | 2 days | None |
| Phase 1 | Range Query Optimization | 3 days | Phase 0 |
| Phase 2 | Time-Indexed Block Reads | 2 days | Phase 0 |
| Phase 3 | Query Result Cache | 2 days | Phase 1 |
| Phase 4 | Parallel Block Reads | 2 days | Phase 2 |

**Total Estimated Effort**: 11 days

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Range evaluator breaks edge cases | Medium | High | Comprehensive unit tests, fallback to step loop |
| Block index increases memory usage | Low | Medium | Sparse indexing (1 entry per 128 samples) |
| Cache invalidation race conditions | Medium | Medium | Use read-write locks, atomic operations |
| Parallel reads increase memory pressure | Low | Low | Limit parallelism, streaming merge |

---

## Appendix: Files to Create/Modify

### New Files

```
test/benchmark/read_performance_benchmark.cpp
test/benchmark/e2e_query_benchmark.cpp
test/unit/prometheus/promql/range_evaluator_test.cpp
test/unit/storage/block_index_test.cpp
test/unit/prometheus/promql/query_cache_test.cpp
test/unit/storage/parallel_block_read_test.cpp
test/integration/read_performance_e2e_test.cpp

src/tsdb/prometheus/promql/range_evaluator.h
src/tsdb/prometheus/promql/range_evaluator.cpp
src/tsdb/prometheus/promql/query_cache.h
src/tsdb/prometheus/promql/query_cache.cpp
src/tsdb/storage/block_read_pool.h
src/tsdb/storage/block_read_pool.cpp

scripts/record_read_baseline.sh
scripts/compare_benchmarks.py
```

### Modified Files

```
src/tsdb/prometheus/promql/engine.cpp
src/tsdb/storage/storage_impl.cpp
src/tsdb/storage/internal/block_impl.h
src/tsdb/storage/internal/block_impl.cpp
include/tsdb/storage/storage_config.h
CMakeLists.txt (test targets)
```
