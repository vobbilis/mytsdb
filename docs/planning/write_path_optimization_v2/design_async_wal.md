# Design: Asynchronous Write Ahead Log (WAL)

## 1. Problem Statement
The current `WriteAheadLog` implementation is synchronous. Every `log()` call performs the following steps under a lock:
1.  Acquire Mutex.
2.  Serialize `TimeSeries`.
3.  Write to `std::ofstream`.
4.  `flush()` to disk.
5.  Release Mutex.

While sharding reduced lock contention, the I/O latency (especially `flush()`) still blocks the client thread. This limits the maximum write throughput to the disk's IOPS capacity.

## 2. Goals
*   **Minimize Write Latency**: The `log()` call should return immediately after placing data in a memory buffer.
*   **Increase Throughput**: Batch multiple writes into a single disk I/O operation.
*   **Configurable Durability**: Allow users to trade off between strict durability (fsync every write) and performance (periodic fsync).

## 3. Proposed Architecture

### 3.1. `AsyncWALShard`
We will introduce a new class `AsyncWALShard` (or modify `WALShard`/`WriteAheadLog`) that wraps the synchronous file writing logic with a queue and a background thread.

**Components:**
*   **Write Queue**: A thread-safe queue (e.g., `moodycamel::ConcurrentQueue` or a simple `std::deque` with `std::mutex` and `std::condition_variable`) to hold incoming `TimeSeries` data.
*   **Background Worker**: A dedicated `std::thread` that consumes items from the queue.
*   **Batch Buffer**: A local buffer in the worker thread to accumulate serialized data before writing to disk.

### 3.2. Data Flow
1.  **Client Thread**: Calls `ShardedWAL::log(series)`.
2.  **ShardedWAL**: Hashes series to a specific `AsyncWALShard`.
3.  **AsyncWALShard**:
    *   Acquires a queue lock (or uses lock-free queue).
    *   Pushes `series` (or serialized data) to the queue.
    *   Notifies the worker thread (CV).
    *   Returns `success` immediately.
4.  **Worker Thread**:
    *   Wakes up when queue is not empty or timeout occurs.
    *   Dequeues a batch of items (e.g., up to 1000 items or until queue empty).
    *   Serializes items (if not already done).
    *   Writes batch to `WriteAheadLog`.
    *   Calls `flush()` once per batch.

## 4. Implementation Details

### 4.1. Queue Strategy
*   **Option A**: Queue of `core::TimeSeries` objects.
    *   *Pros*: Simple.
    *   *Cons*: Memory overhead of objects. Serialization happens in background thread (good for client latency, but consumes CPU in background).
*   **Option B**: Queue of `std::vector<uint8_t>` (serialized data).
    *   *Pros*: Serialization happens in client thread (distributed CPU load). Queue stores compact bytes.
    *   *Cons*: Client thread pays serialization cost.
*   **Decision**: **Option A** is preferred for lowest client latency. The background thread can handle serialization.

### 4.2. Synchronization
*   `std::mutex` + `std::condition_variable` for the queue.
*   `std::atomic<bool> running_` for shutdown signaling.

### 4.3. Shutdown
*   On destructor, set `running_ = false`, notify CV, and `join()` the worker thread.
*   Worker thread must drain the queue before exiting to ensure no data loss on clean shutdown.

## 5. Interface Changes

### `include/tsdb/storage/sharded_wal.h`
No public API changes needed for `ShardedWAL`. It will internally instantiate `AsyncWALShard` instead of `WriteAheadLog` (or `WriteAheadLog` will be updated to support async mode).

To keep it clean, we might modify `WriteAheadLog` to have an `AsyncMode` flag or subclass it. Given `WriteAheadLog` is currently a single file manager, it's better to wrap it.

Let's modify `ShardedWAL` to manage `AsyncWALShard`s.

```cpp
class AsyncWALShard {
public:
    AsyncWALShard(const std::string& dir);
    ~AsyncWALShard();
    void log(const core::TimeSeries& series);
private:
    void worker_loop();
    
    std::unique_ptr<WriteAheadLog> wal_;
    std::queue<core::TimeSeries> queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread worker_;
    std::atomic<bool> running_;
};
```

## 6. Risks & Mitigations
*   **Data Loss**: If the process crashes (SEGFAULT/KILL) before the queue is drained, data in memory is lost.
    *   *Mitigation*: This is an acceptable trade-off for "Async" mode. We can add a "Sync" mode flag if needed.
*   **Memory Usage**: If the disk is too slow, the queue might grow unbounded.
    *   *Mitigation*: Implement a bounded queue. If full, block the client or drop data (blocking is safer for DBs).

## 7. Plan
1.  Create `AsyncWALShard` class.
2.  Update `ShardedWAL` to use `AsyncWALShard`.
3.  Add unit tests for async behavior (ensure queue draining on shutdown).
4.  Benchmark.
