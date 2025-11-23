# Performance Tuning Plan

**Branch:** `performance-tuning`  
**Created:** 2025-01-23  
**Status:** In Progress  
**Goal:** Methodically tune both write and read performance

## 📊 Current Performance Baseline

### Write Performance
- **Observed:** ~10K writes/sec
- **Theoretical Max (single-threaded):** ~83K writes/sec
- **Theoretical Max (4 threads):** ~300K writes/sec
- **Target:** >50K writes/sec (single-threaded), >200K writes/sec (4 threads)

### Read Performance
- **Observed:** Not yet measured
- **Target:** >100K reads/sec (single-threaded), >500K reads/sec (4 threads)
- **Latency Target:** P99 < 10ms for cache hits, P99 < 100ms for disk reads

### Known Bottlenecks (from analysis)
1. **WAL I/O:** ~10μs per write (file write)
2. **Lock contention:** shared_mutex may cause contention
3. **Memory allocation:** Despite pools, may still be a bottleneck
4. **Cache misses:** Unknown impact
5. **Block management:** Block append operations

## 🎯 Tuning Strategy

### Phase 1: Establish Baseline Metrics
**Goal:** Measure current performance accurately before any changes

1. **Create comprehensive benchmark suite**
   - Single-threaded write throughput
   - Multi-threaded write throughput (1, 2, 4, 8 threads)
   - Single-threaded read throughput
   - Multi-threaded read throughput (1, 2, 4, 8 threads)
   - Mixed read/write workloads
   - Latency measurements (P50, P95, P99)
   - Memory usage profiling

2. **Profile current implementation**
   - CPU profiling (perf, gprof, or similar)
   - Memory profiling (valgrind massif, or similar)
   - Lock contention analysis
   - I/O profiling

3. **Document baseline metrics**
   - Create baseline report with all measurements
   - Identify top 5 bottlenecks from profiling

### Phase 2: Write Performance Tuning
**Goal:** Improve write throughput by 5x (from 10K to 50K+ ops/sec)

#### 2.1 WAL Optimization
- [ ] **Batch WAL writes**
  - Buffer multiple writes before flushing
  - Use async I/O for WAL writes
  - Reduce fsync frequency (with configurable durability)
  
- [ ] **WAL file management**
  - Pre-allocate WAL files
  - Use memory-mapped I/O for WAL
  - Optimize WAL segment rotation

#### 2.2 Lock Optimization
- [ ] **Reduce lock scope**
  - Minimize critical sections
  - Use lock-free data structures where possible
  - Implement per-series locking instead of global lock

- [ ] **Lock-free writes**
  - Use lock-free queues for write batching
  - Implement lock-free hash map for series index
  - Use atomic operations for counters

#### 2.3 Memory Allocation Optimization
- [ ] **Object pool improvements**
  - Pre-allocate pools at startup
  - Use thread-local pools to reduce contention
  - Optimize pool sizing based on workload

- [ ] **Reduce allocations**
  - Reuse buffers
  - Use stack allocation where possible
  - Minimize temporary object creation

#### 2.4 Block Management Optimization
- [ ] **Batch block operations**
  - Accumulate writes before flushing to blocks
  - Use write buffers
  - Optimize block append operations

- [ ] **Async block persistence**
  - Move block writes to background thread
  - Use async I/O for block writes
  - Implement write coalescing

### Phase 3: Read Performance Tuning
**Goal:** Achieve >100K reads/sec with low latency

#### 3.1 Cache Optimization
- [ ] **Improve cache hit ratio**
  - Optimize cache eviction policy (LRU → ARC or similar)
  - Increase cache size if memory allows
  - Implement predictive prefetching

- [ ] **Cache structure optimization**
  - Use lock-free cache structures
  - Implement multi-level caching (L1/L2/L3)
  - Optimize cache lookup algorithms

#### 3.2 Index Optimization
- [ ] **Index structure improvements**
  - Optimize label index structure
  - Use more efficient data structures (B-tree, hash map)
  - Implement index partitioning

- [ ] **Query optimization**
  - Optimize label matching algorithms
  - Implement query result caching
  - Use SIMD for label comparisons

#### 3.3 Block Read Optimization
- [ ] **Block access optimization**
  - Use memory-mapped files for block reads
  - Implement block prefetching
  - Optimize decompression algorithms

- [ ] **Parallel reads**
  - Read from multiple blocks in parallel
  - Use async I/O for block reads
  - Implement read-ahead strategies

### Phase 4: Mixed Workload Optimization
**Goal:** Optimize for realistic mixed read/write workloads

- [ ] **Workload-aware scheduling**
  - Prioritize reads over writes when needed
  - Implement adaptive batching based on load
  - Balance read/write resource allocation

- [ ] **Resource management**
  - Implement backpressure mechanisms
  - Optimize thread pool sizing
  - Balance CPU vs I/O bound operations

### Phase 5: Validation and Regression Testing
**Goal:** Ensure improvements don't break functionality

- [ ] **Run full test suite**
  - Ensure all tests pass
  - Verify no regressions

- [ ] **Performance regression tests**
  - Ensure improvements are maintained
  - Document performance improvements
  - Create performance benchmarks as part of CI

## 📈 Measurement and Validation

### Benchmark Suite Structure
```
test/benchmark/
├── write_performance_benchmark.cpp      # Write throughput tests
├── read_performance_benchmark.cpp       # Read throughput tests
├── latency_benchmark.cpp                # Latency measurements
├── mixed_workload_benchmark.cpp         # Mixed read/write tests
├── memory_profiling_benchmark.cpp       # Memory usage tests
└── concurrency_benchmark.cpp            # Multi-threaded tests
```

### Key Metrics to Track
1. **Throughput**
   - Operations per second (writes/sec, reads/sec)
   - Throughput vs thread count
   - Throughput vs data size

2. **Latency**
   - P50, P95, P99 latencies
   - Latency distribution
   - Tail latency analysis

3. **Resource Usage**
   - CPU usage
   - Memory usage
   - I/O bandwidth
   - Lock contention

4. **Scalability**
   - Performance vs number of threads
   - Performance vs data size
   - Performance vs number of series

## 🔄 Iterative Process

For each optimization:

1. **Measure baseline** - Run benchmarks before change
2. **Implement optimization** - Make targeted change
3. **Measure impact** - Run benchmarks after change
4. **Validate** - Ensure tests still pass
5. **Document** - Record improvement (or regression)
6. **Commit** - Commit with clear message and metrics

### Commit Message Format
```
perf: [write/read/mixed] Optimize [component]

Before: [baseline metrics]
After: [new metrics]
Improvement: [X% improvement or X ops/sec increase]

Changes:
- [specific changes made]
- [rationale]

Tested: [test results]
```

## 📝 Progress Tracking

### Phase 1: Baseline Establishment
- [ ] Create comprehensive benchmark suite
- [ ] Run baseline measurements
- [ ] Profile current implementation
- [ ] Document baseline report

### Phase 2: Write Performance
- [ ] WAL optimization
- [ ] Lock optimization
- [ ] Memory allocation optimization
- [ ] Block management optimization

### Phase 3: Read Performance
- [ ] Cache optimization
- [ ] Index optimization
- [ ] Block read optimization

### Phase 4: Mixed Workload
- [ ] Workload-aware scheduling
- [ ] Resource management

### Phase 5: Validation
- [ ] Full test suite validation
- [ ] Performance regression tests
- [ ] Documentation

## 🎯 Success Criteria

### Write Performance
- ✅ Single-threaded: >50K writes/sec (5x improvement)
- ✅ 4 threads: >200K writes/sec (20x improvement)
- ✅ P99 latency: <10ms

### Read Performance
- ✅ Single-threaded: >100K reads/sec
- ✅ 4 threads: >500K reads/sec
- ✅ P99 latency: <10ms (cache hits), <100ms (disk reads)

### Overall
- ✅ All tests pass
- ✅ No memory leaks
- ✅ No performance regressions
- ✅ Clear documentation of improvements

## 📚 References

- Current performance analysis: `docs/planning/COMPREHENSIVE_DESIGN_STUDY.md`
- LSM analysis: `docs/analysis/LSM_STORAGE_ANALYSIS.md`
- Existing benchmarks: `test/benchmark/`
- Storage implementation: `src/tsdb/storage/storage_impl.cpp`

---

**Next Steps:** Begin Phase 1 - Establish baseline metrics

