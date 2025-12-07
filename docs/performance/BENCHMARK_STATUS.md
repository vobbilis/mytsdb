# Benchmark Status & Bottleneck Analysis
**Date**: December 6, 2025
**Status**: Mixed Results (High Throughput, High Latency)
**Context**: Full system load test (20M samples, mixed read/write)

## 1. Executive Summary

| Category | Metric | Result | Target | Status |
|----------|--------|--------|--------|--------|
| **Write** | Throughput | **1.58M/s** | >1M/s | ✅ PASS |
| **Write** | Latency P99 | **507ms** | <50ms | ❌ FAIL |
| **Read** | P50 Latency | **1.83ms** | <50ms | ✅ PASS |
| **Read** | P99 Latency | **715ms** | <50ms | ❌ FAIL |

**Conclusion**: The system achieves excellent throughput and low average latency but suffers from severe tail latency regressions under sustained load due to blocking I/O and index contention.

---

## 2. Bottleneck Analysis (Critical for Fix)

### A. Write Path: Synchronous Block Persistence
**Impact**: The primary cause of Write P99 regression (507ms).
**Evidence from Log**:
- `Block Persist`: **2907.07s** cumulative CPU time.
- `Sample Append`: 161.93s.
- `Mutex Wait`: 0.42s (negligible).

**Diagnosis**: 
The `Block Persist` time is massively disproportionate. With 16 workers over ~180s, ~100% of worker time is spent waiting for persistence. The persistence operation currently blocks the write thread, preventing it from handling new samples.

**Recommended Fix**:
- **Async Persistence**: Move block persistence to a background thread pool.
- **Double Buffering**: Allow writing to a new active block while the old one persists.

### B. Read Path: Index Search Contention
**Impact**: The primary cause of Read P99 regression (715ms).
**Evidence from Log**:
- `Index Search`: **1215.90s** cumulative CPU time.
- `Storage Read Time`: 10599s (Note: heavy concurrency impact).
- `Block Lookup`: 5.1s.

**Diagnosis**:
`Index Search` consumes the vast majority of CPU time on the read path. This likely corresponds to `series_.find(labels)` or iterator traversal. Under high read/write concurrency (8 read workers), contention on the `shared_mutex` or simply the cost of `std::map` lookups is becoming the limiting factor.

**Recommended Fix**:
- **Optimize Index**: Evaluate `tbb::concurrent_hash_map` for Series Index (similar to Block Manager fix).
- **Index Caching**: Cache recent index lookups to avoid map tree traversal.

---

## 3. Detailed Benchmark Data (20M Run)

### Benchmark Configuration
- **Samples**: 20,000,000 (Generated) + ~50M Warmup
- **Duration**: 180s
- **Concurrency**: 16 Writers, 8 Readers
- **Queries**: 48 types (Range, Aggregations, Instance)

### Write Performance
```text
Total Samples: 283,962,940
Throughput: 1,577,572 samples/sec
Latency p50: 0.23 ms
Latency p99: 507.32 ms
Latency max: 1934.06 ms
```

### Read Performance
```text
Total Queries: 24,731
Throughput: 137.4 queries/sec
Latency p50: 1.83 ms
Latency p99: 715.13 ms
Latency max: 1396.36 ms
```

### Server-Side Metrics (Component Breakdown)
| Component | Cumulative Time (s) | Notes |
|-----------|---------------------|-------|
| `Block Persist` | **2907.07** | 🚨 CRITICAL BOTTLENECK |
| `Index Search` | **1215.90** | 🚨 MAJOR BOTTLENECK |
| `Sample Append` | 161.93 | Efficient |
| `Index Insert` | 45.65 | Efficient |
| `Cache Update` | 41.77 | Efficient |
| `WAL Write` | 13.49 | Efficient |
| `Block Seal` | 12.08 | Efficient |

### Storage Stats
- **Total Bytes Written**: 5.68 GB
- **Total Writes**: 1.25M operations
- **Cache Hit Rate**: 1.9%
