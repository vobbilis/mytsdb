# Write Performance Discrepancy Analysis

**Created:** 2025-01-23  
**Status:** Investigation Phase  
**Branch:** `performance-tuning`

## 📊 Problem Statement

Baseline measurements revealed a **24x performance discrepancy** between two write scenarios:

| Scenario | Throughput | Latency |
|----------|-----------|---------|
| **New Series Write** | ~10.5K writes/sec | ~95μs per write |
| **Update Existing Series** | ~250K writes/sec | ~4μs per write |

**Expected:** New series writes should be much higher (target: >50K writes/sec).  
**Reality:** New series writes are only 4% of update performance.

## 🔍 Hypothesis: Root Causes

### 1. **Series Index Lookup/Insertion Overhead**
**Likelihood:** HIGH  
**Impact:** HIGH

When writing a new series:
- Must calculate series ID (hash of labels)
- Must check if series exists in index
- Must insert new series into index (hash map insert)
- Must allocate memory for series metadata

When updating existing series:
- Series already in index (cache hit)
- Direct pointer to series object
- Just append sample to existing series

**Investigation:**
- Profile `StorageImpl::write()` to measure time spent in:
  - Series ID calculation
  - Index lookup (`series_index_` access)
  - Index insertion
  - Series metadata allocation

### 2. **WAL (Write-Ahead Log) Overhead for New Series**
**Likelihood:** HIGH  
**Impact:** MEDIUM-HIGH

New series might trigger:
- Full label serialization to WAL
- WAL entry creation overhead
- WAL buffer flush if buffer is full

Existing series updates might:
- Use cached WAL entry format
- Reuse WAL buffer space
- Batch better with other updates

**Investigation:**
- Profile WAL write path for new vs existing series
- Measure WAL serialization time
- Check if WAL batching differs between scenarios

### 3. **Memory Allocation Overhead**
**Likelihood:** MEDIUM  
**Impact:** MEDIUM

New series requires:
- Allocate `Series` object
- Allocate label storage
- Allocate initial sample buffer
- Update object pools

Existing series:
- Reuses allocated objects
- Just grows sample buffer if needed

**Investigation:**
- Profile memory allocations during new series write
- Check object pool hit/miss rates
- Measure allocation time vs write time

### 4. **Lock Contention**
**Likelihood:** MEDIUM  
**Impact:** MEDIUM

New series write might:
- Hold exclusive lock longer (index insertion)
- Cause lock contention with other operations
- Trigger cache updates

**Investigation:**
- Profile lock acquisition/release times
- Check for lock contention in concurrent scenarios
- Measure critical section duration

### 5. **Block Manager Overhead**
**Likelihood:** LOW-MEDIUM  
**Impact:** MEDIUM

New series might:
- Create new block entry
- Initialize block metadata
- Trigger block allocation

**Investigation:**
- Profile block manager operations
- Check if block creation is synchronous
- Measure block allocation overhead

## 🎯 Investigation Plan

### Phase 1: Profiling and Measurement
**Goal:** Identify where time is spent in new series write path

#### 1.1 Instrument `StorageImpl::write()`
- [ ] Add timing instrumentation to `write()` method
- [ ] Measure time spent in:
  - Series ID calculation
  - Index lookup
  - Index insertion
  - WAL write
  - Block manager operations
  - Memory allocations
  - Lock acquisition/release

#### 1.2 Create Detailed Benchmark
- [ ] Create `write_new_series_benchmark.cpp`:
  - Write 10K unique series (new series scenario)
  - Measure per-operation breakdown
  - Track allocations
- [ ] Create `write_update_series_benchmark.cpp`:
  - Write 10K updates to same series (update scenario)
  - Measure per-operation breakdown
  - Compare with new series

#### 1.3 Profile with Tools
- [ ] Use `perf` (Linux) or `Instruments` (macOS) to profile:
  - CPU hotspots
  - Memory allocations
  - Lock contention
- [ ] Use `valgrind --tool=callgrind` for call graph analysis
- [ ] Use `gprof` for function-level profiling

### Phase 2: Root Cause Analysis
**Goal:** Identify the primary bottleneck(s)

#### 2.1 Analyze Profiling Data
- [ ] Identify top 5 time-consuming operations
- [ ] Calculate percentage of time spent in each operation
- [ ] Compare new series vs update series profiles
- [ ] Document findings

#### 2.2 Hypothesis Validation
- [ ] Test each hypothesis with targeted micro-benchmarks
- [ ] Measure impact of each suspected bottleneck
- [ ] Rank bottlenecks by impact

### Phase 3: Optimization Strategy
**Goal:** Design fixes for identified bottlenecks

#### 3.1 Index Optimization
**If index lookup/insertion is the bottleneck:**
- [ ] Optimize series ID hash calculation
- [ ] Use faster hash map implementation
- [ ] Pre-allocate index capacity
- [ ] Consider lock-free index for reads
- [ ] Batch index updates

#### 3.2 WAL Optimization
**If WAL is the bottleneck:**
- [ ] Batch WAL writes for new series
- [ ] Use async WAL writes
- [ ] Optimize label serialization
- [ ] Cache WAL entry formats
- [ ] Reduce WAL flush frequency

#### 3.3 Memory Allocation Optimization
**If allocations are the bottleneck:**
- [ ] Pre-allocate series objects
- [ ] Use object pools more aggressively
- [ ] Reduce temporary allocations
- [ ] Use stack allocation where possible
- [ ] Batch allocations

#### 3.4 Lock Optimization
**If locks are the bottleneck:**
- [ ] Reduce lock scope
- [ ] Use lock-free data structures
- [ ] Implement per-series locking
- [ ] Use read-write locks more effectively
- [ ] Batch operations to reduce lock acquisitions

#### 3.5 Block Manager Optimization
**If block manager is the bottleneck:**
- [ ] Lazy block creation
- [ ] Batch block operations
- [ ] Async block allocation
- [ ] Pre-allocate block metadata

### Phase 4: Implementation and Validation
**Goal:** Implement fixes and measure improvement

#### 4.1 Implement Optimizations
- [ ] Implement fixes for top bottlenecks
- [ ] Ensure backward compatibility
- [ ] Add unit tests for optimizations

#### 4.2 Measure Impact
- [ ] Run baseline benchmarks again
- [ ] Compare before/after metrics
- [ ] Validate target achievement (>50K writes/sec for new series)

#### 4.3 Regression Testing
- [ ] Run full test suite
- [ ] Ensure no functionality regressions
- [ ] Verify correctness of optimizations

## 📈 Success Criteria

### Immediate Goals
- **New Series Write:** >50K writes/sec (5x improvement from baseline)
- **Update Write:** Maintain >200K writes/sec (no regression)
- **Latency:** P99 < 20μs for new series writes

### Long-term Goals
- **New Series Write:** >100K writes/sec (10x improvement)
- **Update Write:** >300K writes/sec
- **Latency:** P99 < 10μs for new series writes

## 🔬 Detailed Investigation Steps

### Step 1: Add Instrumentation to `StorageImpl::write()`

```cpp
// In storage_impl.cpp
core::Result<void> StorageImpl::write(const core::TimeSeries& series) {
    auto start_total = std::chrono::high_resolution_clock::now();
    
    // 1. Series ID calculation
    auto start_id = std::chrono::high_resolution_clock::now();
    uint64_t series_id = calculate_series_id(series.labels());
    auto end_id = std::chrono::high_resolution_clock::now();
    // Log: time_id_calc = end_id - start_id
    
    // 2. Index lookup
    auto start_lookup = std::chrono::high_resolution_clock::now();
    // ... index lookup code ...
    auto end_lookup = std::chrono::high_resolution_clock::now();
    // Log: time_lookup = end_lookup - start_lookup
    
    // 3. Index insertion (if new)
    if (is_new_series) {
        auto start_insert = std::chrono::high_resolution_clock::now();
        // ... index insertion code ...
        auto end_insert = std::chrono::high_resolution_clock::now();
        // Log: time_insert = end_insert - start_insert
    }
    
    // 4. WAL write
    auto start_wal = std::chrono::high_resolution_clock::now();
    // ... WAL write code ...
    auto end_wal = std::chrono::high_resolution_clock::now();
    // Log: time_wal = end_wal - start_wal
    
    // ... rest of write path ...
}
```

### Step 2: Create Comparison Benchmarks

**New Series Benchmark:**
- Write 10,000 unique series (different labels)
- Measure total time and per-operation breakdown
- Track allocations

**Update Benchmark:**
- Write 10,000 updates to the same series
- Measure total time and per-operation breakdown
- Compare with new series

### Step 3: Profile with External Tools

**macOS (Instruments):**
```bash
instruments -t "Time Profiler" -D trace.trace ./build/test/benchmark/write_new_series_bench
```

**Linux (perf):**
```bash
perf record -g ./build/test/benchmark/write_new_series_bench
perf report
```

**Valgrind (callgrind):**
```bash
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./build/test/benchmark/write_new_series_bench
kcachegrind callgrind.out
```

## 📝 Expected Findings

Based on the 24x discrepancy, we expect to find:

1. **Index operations taking 50-70% of time** for new series writes
2. **WAL operations taking 20-30% of time** for new series writes
3. **Memory allocations taking 10-20% of time** for new series writes
4. **Lock contention minimal** (single-threaded benchmark)

## 🎯 Next Steps

1. **Immediate:** Add instrumentation to `StorageImpl::write()`
2. **Short-term:** Create detailed benchmarks and profile
3. **Medium-term:** Implement optimizations based on findings
4. **Long-term:** Achieve >50K writes/sec for new series

---

**Related Documents:**
- `PERFORMANCE_TUNING_PLAN.md` - Overall performance tuning strategy
- `src/tsdb/storage/storage_impl.cpp` - Implementation to instrument
- `test/benchmark/write_performance_benchmark.cpp` - Current benchmark

