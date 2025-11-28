# Walkthrough: Asynchronous WAL Implementation

## Overview
We implemented an Asynchronous Write Ahead Log (WAL) to decouple client write latency from disk I/O. This involved wrapping the existing `WriteAheadLog` with a new `AsyncWALShard` class that uses a background thread and a producer-consumer queue.

## Changes

### 1. `AsyncWALShard`
*   **Location**: `src/tsdb/storage/async_wal_shard.cpp`, `include/tsdb/storage/async_wal_shard.h`
*   **Design**:
    *   Maintains a `std::queue<core::TimeSeries>` protected by a mutex.
    *   `log()` pushes to the queue and returns immediately.
    *   Background thread consumes the queue, batches writes, and flushes to disk.
    *   Destructor ensures the queue is drained and flushed.

### 2. `WriteAheadLog` Modifications
*   **Location**: `src/tsdb/storage/wal.cpp`
*   **Changes**:
    *   Added `flush_now` parameter to `log()` (default `true`).
    *   Added public `flush()` method.
    *   Fixed a critical deserialization bug (duplicate `label_count` read).

### 3. `ShardedWAL` Integration
*   **Location**: `src/tsdb/storage/sharded_wal.cpp`
*   **Changes**: Replaced `WriteAheadLog` with `AsyncWALShard`.

## Verification

### Unit Tests
*   **`test/storage/async_wal_test.cpp`**: Verified basic logging, batching, and concurrent writes.
*   **`test/storage/sharded_wal_test.cpp`**: Verified integration and clean shutdown.

### Benchmarks

#### Direct Storage Performance (`write_perf_bench`)
*   **Sync WAL**: ~233,000 items/sec
*   **Async WAL**: ~333,000 items/sec
*   **Improvement**: **+43%**

#### Concurrent Performance (`concurrency_perf_bench`)
*   **Sync WAL**: ~38,000 items/sec
*   **Async WAL**: ~43,600 items/sec
*   **Improvement**: **+15%**

#### End-to-End OTEL Ingestion (`otel_write_perf_bench`)
*   **Throughput**: ~18,900 items/sec (unchanged)
*   **Analysis**: The bottleneck for OTEL ingestion is likely in the gRPC layer or request processing, masking the storage improvements.

## Conclusion
The Asynchronous WAL significantly improves the core storage engine's write throughput and latency. While end-to-end OTEL performance is currently limited by other factors, the storage layer is now much more scalable.
