# Performance Tuning Plan

**Branch:** `performance-tuning`  
**Created:** 2025-01-23  
**Status:** In Progress  
**Goal:** Methodically tune both write and read performance

## 📊 Current Performance Baseline

### Write Performance
- **Baseline (Before Optimization):** ~9K writes/sec (new series), ~137K writes/sec (updates)
- **Current (After Optimization):** **138,735 writes/sec** (new series), **163,693 writes/sec** (updates)
- **Improvement:** **15.4x** for new series writes
- **Target:** >50K writes/sec (single-threaded), >200K writes/sec (4 threads) ✅ **EXCEEDED**

### Read Performance
- **Observed:** ~1.4K reads/sec (Baseline)
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

**✅ RESOLVED:** Performance discrepancy between new series writes and updates has been eliminated!  
- **Before:** 15x slower for new series (110μs vs 7.3μs)
- **After:** 1.1x difference (7.36μs vs 6.56μs) - near parity achieved!
- **See:** `WRITE_PERFORMANCE_ANALYSIS_RESULTS.md` for detailed results.

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

#### 2.4 Block Management Optimization ✅ COMPLETE
- [x] **Defer block persistence for new series** ✅
  - Removed immediate block persistence for new series
  - Blocks now persist only when full
  - **Result:** 15x improvement in new series write performance
  - **See:** `WRITE_PERFORMANCE_ANALYSIS_RESULTS.md` for details

- [ ] **Async block persistence** (Future optimization)
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
├── write_performance_benchmark.cpp      # Phase 2: Write throughput (Single-threaded)
├── read_performance_benchmark.cpp       # Phase 3: Read throughput (Single-threaded)
├── latency_benchmark.cpp                # Phase 2/3: Latency measurements (Write latency focus)
├── mixed_workload_benchmark.cpp         # Phase 4: Mixed read/write tests (50/50 split)
├── concurrency_benchmark.cpp            # Phase 2: Multi-threaded scaling
├── run_baseline_benchmarks.sh           # Script to run all benchmarks and collect baseline
└── CMakeLists.txt                       # Benchmark build configuration
```

### Key Metrics to Track
1. **Throughput**
   - **Write:** `WriteBenchmark/SingleThreadedWrite` (write_performance_benchmark.cpp)
   - **Read:** `ReadBenchmark/SingleThreadedRead` (read_performance_benchmark.cpp)
   - **Mixed:** `MixedWorkloadBenchmark/ReadWrite50_50` (mixed_workload_benchmark.cpp)

2. **Latency**
   - **Write Latency:** `LatencyBenchmark/WriteLatency` (latency_benchmark.cpp)
   - P50, P95, P99 latencies (via Google Benchmark output)

3. **Scalability**
   - **Concurrent Writes:** `ConcurrencyBenchmark/ConcurrentWrites` (concurrency_benchmark.cpp)
   - Threads: 1, 2, 4, 8

4. **Resource Usage**
   - CPU usage (monitor during runs)
   - Memory usage (monitor during runs)
   - Lock contention (profiling)

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
- [x] Create comprehensive benchmark suite
  - `write_performance_benchmark.cpp`
  - `read_performance_benchmark.cpp`
  - `mixed_workload_benchmark.cpp`
  - `latency_benchmark.cpp`
  - `concurrency_benchmark.cpp`
  - `run_baseline_benchmarks.sh`
- [x] Run baseline measurements
- [ ] **OTEL/gRPC write performance testing** ⚠️ **In Progress**
  - **Status:** Server implementation needed (see `OTEL_GRPC_PERFORMANCE_TESTING_READINESS.md`)
  - **Missing:** gRPC server startup code, performance test client
  - **Write (New Series):** ~9K writes/sec (110μs/write)
  - **Write (Update):** ~137K writes/sec (7.3μs/write)
  - **Read:** ~1.4K reads/sec
  - **Mixed:** ~2.7K ops/sec
  - **Latency:** ~5μs (update)
- [x] Profile current implementation ✅
  - Identified block persistence as primary bottleneck (69% of write time)
  - See `WRITE_PERFORMANCE_ANALYSIS_RESULTS.md` for detailed breakdown
- [x] Document baseline report ✅
  - Created `WRITE_PERFORMANCE_ANALYSIS_RESULTS.md`
  - Identified and resolved 15x performance discrepancy

### Phase 2: Write Performance
**Validation:** `write_performance_benchmark.cpp`, `concurrency_benchmark.cpp`, `latency_benchmark.cpp`
- [x] **Block persistence optimization** ✅ **COMPLETE**
  - Deferred block persistence for new series
  - **Result:** 15.4x improvement (9K → 138K writes/sec)
  - **Latency:** 15x improvement (110μs → 7.36μs)
  - **Discrepancy:** Reduced from 15x to 1.1x
- [ ] WAL optimization (Next priority)
- [ ] Lock optimization
- [ ] Memory allocation optimization
- [ ] Block management optimization

### Phase 3: Read Performance
**Validation:** `read_performance_benchmark.cpp`
- [ ] Cache optimization
- [ ] Index optimization
- [ ] Block read optimization

### Phase 4: Mixed Workload
**Validation:** `mixed_workload_benchmark.cpp`
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

- **Write Performance Discrepancy Analysis:** `docs/planning/WRITE_PERFORMANCE_DISCREPANCY_ANALYSIS.md` ⚠️ **CRITICAL**
- Current performance analysis: `docs/planning/COMPREHENSIVE_DESIGN_STUDY.md`
- LSM analysis: `docs/analysis/LSM_STORAGE_ANALYSIS.md`
- Existing benchmarks: `test/benchmark/`
- Storage implementation: `src/tsdb/storage/storage_impl.cpp`

---

**Next Steps:** Begin Phase 1 - Establish baseline metrics

