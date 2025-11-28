# Write Path Optimization Analysis

## 1. Current Architecture Analysis

### 1.1 `StorageImpl::write`
The write path involves the following synchronous steps:
1.  **WAL Logging**: `wal_->log(series)` (Global Lock)
2.  **Series ID Calculation**: `calculate_series_id` (CPU bound)
3.  **Index Lookup**: `active_series_.find` (Concurrent Hash Map - Good)
4.  **New Series Creation**:
    *   `index_->add_series` (Global Lock)
    *   `active_series_.insert` (Concurrent Hash Map - Good)
5.  **Append**: `series->append` (Per-Series Lock)
6.  **Cache Update**: `cache_hierarchy_->put` (Internal Locking)
7.  **Block Persistence**: `block_manager_->seal_and_persist_block` (Synchronous I/O)

### 1.2 Identified Bottlenecks

#### Critical Bottlenecks (Global Locks)
1.  **WAL Logging**: `WriteAheadLog::log` uses a `std::lock_guard<std::mutex>` which serializes ALL writes to the system. This is the single biggest bottleneck.
2.  **Index Updates**: `Index::add_series` uses a `std::lock_guard<std::mutex>` which serializes all NEW series creation.

#### Secondary Bottlenecks
1.  **Synchronous I/O**: Both WAL logging and Block Persistence happen synchronously in the write path.
2.  **Lock Contention**: `StorageImpl::mutex_` is used for `series_blocks_` updates.

## 2. Proposed Optimizations

### 2.1 High Impact: Sharded WAL & Index
To remove the global locks, we should shard the WAL and Index.
*   **Sharded WAL**: Instead of one `WriteAheadLog`, use `N` instances (e.g., one per shard).
*   **Sharded Index**: Instead of one `Index`, use `N` instances.

### 2.2 High Impact: Asynchronous WAL
Move WAL writing to a background thread.
*   **Ring Buffer**: Use a lock-free ring buffer to pass data to the background thread.
*   **Batching**: The background thread can batch writes to disk, reducing syscalls.

### 2.3 Medium Impact: Fine-Grained Locking
*   **Read-Write Lock for Index**: Use `std::shared_mutex` for the Index to allow concurrent reads.
*   **Sharded StorageImpl Mutex**: Shard the protection for `series_blocks_`.

## 3. Implementation Plan (Draft)

1.  **Refactor WAL**:
    *   Implement `AsyncWriteAheadLog` using a ring buffer.
    *   Support batching in `log()`.
2.  **Refactor Index**:
    *   Change `std::mutex` to `std::shared_mutex`.
    *   Implement sharding support (optional, if R/W lock isn't enough).
3.  **Update StorageImpl**:
    *   Integrate `AsyncWriteAheadLog`.
    *   Update `write()` to use async logging.

## 4. Expected Gains
*   **Throughput**: Significant increase due to removal of global serialization.
*   **Latency**: Reduced P99 latency by moving I/O off the critical path.
