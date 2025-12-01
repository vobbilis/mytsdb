# Read Path Optimization Plan (Black Box Edition)

**Status:** Draft v2
**Goal:** Drastically reduce read latency and improve consistency, verified strictly via production interfaces (gRPC/PromQL).

## Constraints
-   **Ingestion**: ALWAYS use gRPC / OTEL Write Request. No direct `storage->write()`.
-   **Querying**: ALWAYS use PromQL via HTTP API. No direct `storage->read()`.
-   **Verification**: Success is defined by external metrics (latency, error rate) observed from the client side.

## Phase 1: Instrumentation & Baseline (The "Black Box" View)
*Goal: See what's happening inside without opening the box.*

- [x] **Task 1.1: Read Path Metrics**
    -   **Action**: Implement `ReadPerformanceInstrumentation` (as planned).
    -   **Requirement**: Expose these metrics via the `/metrics` HTTP endpoint.
    -   **Key Metrics**:
        -   `mytsdb_read_latency_seconds_bucket` (Histogram)
        -   `mytsdb_index_search_duration_seconds`
        -   `mytsdb_block_scan_duration_seconds`
        -   `mytsdb_cache_hits_total`

- [x] **Task 1.2: Baseline Benchmark**
    -   **Action**: Create a `benchmark_tool` (Go or C++) that:
        1.  Sends 1M samples via gRPC (OTEL format).
        2.  Sends 10k PromQL queries (concurrently).
    -   **Output**: Record P50, P90, P99 latency before any optimizations.

### Baseline Results (2025-11-30)
-   **Setup**: Local MacBook, 1000 series, 10 samples/series (10k total), 10 concurrent query threads.
-   **Write Performance**: ~57,471 samples/sec.
-   **Read Latency**:
    -   **P50**: 7.76 ms
    -   **P90**: 7.85 ms
    -   **P99**: 8.28 ms
-   **Observation**: Latency is low for small in-memory datasets.

### Stress Test Results (2025-11-30)
-   **Setup**: 100,000 series, 10 samples/series (1M total), 20 concurrent query threads.
-   **Write Performance**: ~98,270 samples/sec (improved due to batching).
-   **Read Latency**:
    -   **P50**: 16.51 ms
    -   **P90**: 16.84 ms
    -   **P99**: 38.57 ms
    -   **Max**: 52.71 ms
-   **Analysis**: 
    -   P99 latency degraded by **4.5x** (8ms -> 38ms) when increasing series count by 100x.
    -   This confirms that even in-memory lookups (Active Head) slow down with cardinality.
    -   As data flushes to disk (which happens at >64MB or time intervals), this latency will spike further without the L2 Cache.

### Stress Test Results (2025-11-30) - Logs Disabled
-   **Setup**: 100,000 series, 10 samples/series (1M total), 20 concurrent query threads.
-   **Write Performance**: ~99,443 samples/sec (Slight improvement).
-   **Read Latency**:
    -   **P50**: 16.00 ms
    -   **P90**: 16.33 ms
    -   **P99**: 16.49 ms (Improved from 38ms!)
    -   **Max**: 114.64 ms
-   **Analysis**: 
    -   Disabling logs significantly improved P99 latency (38ms -> 16.5ms), showing that I/O contention from logging was a factor.
    -   However, P50 latency remains high (~16ms vs ~8ms baseline), confirming the cardinality impact.
    -   The **Max latency of 114ms** indicates occasional stalls, likely due to GC or lock contention, which L2 Cache should help mitigate by reducing lock holding times on the main storage.

### Realistic K8s Benchmark Results (2025-11-30) - Logs Disabled
-   **Setup**: Simulated 50 nodes, ~2000 pods, 3 metrics per pod (CPU, Mem, HTTP), 10 samples/series. Total ~60,000 series.
-   **Write Performance**: ~99,966 samples/sec.
-   **Read Latency**:
    -   **P50**: 2.44 ms
    -   **P90**: 5.33 ms
    -   **P99**: 10.86 ms
    -   **Max**: 38.42 ms
-   **Analysis**:
    -   Performance is surprisingly *better* than the random string benchmark.
    -   **Reason**: Real-world labels (node, namespace, service) have **lower cardinality** than random strings. The index compresses them better, and lookups are faster because of shared prefixes/values.
    -   However, the **Max latency (38ms)** still shows that we have occasional slow queries, likely when hitting unique combinations or during minor GC/lock contention.
    -   This confirms that while our index handles "normal" K8s data well in-memory, we still need the L2 Cache to maintain this speed as data grows and flushes to disk.

## Phase 2: Fix the Cache (L2)
*Goal: Re-enable L2 Cache to drop P99 latency and prevent spikes.*

- [x] **Task 2.1: Debug & Fix**
    -   **Action**: Fix the `CacheHierarchy` segfault.
    -   **Root Cause**: Use-after-move bug in `MemoryMappedCache::put`. The `series` shared_ptr was moved into the map before being dereferenced to calculate size.
    -   **Fix**: Calculated size before moving the pointer.
    -   **Verification**: `tsdb_cache_hierarchy_repro_test` passes.
- [x] **Task 2.2: Tuning**
    -   **Action**: Increased default cache sizes (L1: 1k->50k, L2: 10k->500k) to prevent thrashing.
    -   **Result**: Segfault fixed. System is stable under load.
    -   **Performance**:
        -   P50: ~2.11 ms (Improved)
        -   P99: ~31.86 ms (Higher than baseline due to double-locking overhead in current in-memory L2 implementation).
    -   **Insight**: The current "L2 Cache" is actually an in-memory map protected by a mutex. Since `StorageImpl` already has a fast `concurrent_hash_map` for all series, the L2 cache adds locking overhead without saving memory. Real performance gains will come in Phase 3/4 when we optimize the storage layout and concurrency.

## Phase 3: Optimize Index (Roaring Bitmaps)
*Goal: Speed up high-cardinality queries.*

- [ ] **Task 3.1: Implement Roaring Bitmaps**
    -   **Action**: Replace `std::vector` in `Index`.
    -   **Verification**: Run `benchmark_tool` with complex regex queries (e.g., `test_metric{instance=~"prod.*"}`).
    -   **Expectation**: `mytsdb_index_search_duration_seconds` should drop by 10x+.

## Phase 4: Block Concurrency
*Goal: Improve throughput under load.*

- [ ] **Task 4.1: Shared Locks**
    -   **Action**: Switch `BlockImpl` to `shared_mutex`.
    -   **Verification**: Run `benchmark_tool` with high concurrency (50+ threads).
    -   **Expectation**: Total query throughput (QPS) should increase.

## Phase 5: Consistency Verification
*Goal: Ensure "Read Your Writes".*

- [ ] **Task 5.1: Consistency Test Suite**
    -   **Action**: Create a test that:
        1.  Writes a value $V$ at time $T$ via gRPC.
        2.  Immediately queries for $V$ at $T$ via PromQL.
        3.  Asserts that the result is exactly $V$.
    -   **Loop**: Repeat 10k times while forcing flushes/compactions.
