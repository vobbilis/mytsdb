# Sharded WAL Design & Implementation Plan

## 1. Problem Statement
The current `WriteAheadLog` uses a single global `std::mutex` to protect file access. This serializes all write operations (`StorageImpl::write`), creating a significant bottleneck as concurrency increases.

## 2. Proposed Solution: Sharded WAL
Replace the single `WriteAheadLog` with a `ShardedWAL` that manages multiple independent `WALShard` instances.
-   **Sharding Strategy**: Partition writes based on `SeriesID` (or `Labels` hash).
-   **Independence**: Each shard has its own mutex and file(s), allowing `N` concurrent writes where `N` is the number of shards.
-   **Transparency**: `StorageImpl` interacts with `ShardedWAL` using the same interface (mostly).

## 3. Detailed Design

### 3.1 Class Structure

```cpp
// Forward declaration
class WALShard;

class ShardedWAL {
public:
    // Initialize with number of shards (e.g., usually 2x-4x CPU cores)
    ShardedWAL(const std::string& base_dir, size_t num_shards = 16);
    
    // Log series to the appropriate shard
    core::Result<void> log(const core::TimeSeries& series);
    
    // Replay all shards (can be parallelized)
    core::Result<void> replay(std::function<void(const core::TimeSeries&)> callback);
    
    // Get stats for instrumentation
    WALStats get_stats() const;

private:
    const size_t num_shards_;
    std::string base_dir_;
    std::vector<std::unique_ptr<WALShard>> shards_;
    
    // Helper to determine shard index
    size_t get_shard_index(const core::TimeSeries& series) const;
};

class WALShard {
public:
    WALShard(const std::string& shard_dir, size_t shard_id);
    
    core::Result<void> log(const core::TimeSeries& series);
    core::Result<void> replay(std::function<void(const core::TimeSeries&)> callback);

private:
    std::mutex mutex_; // Protects this shard only
    std::ofstream current_file_;
    // ... existing WAL logic ...
};
```

### 3.2 Pseudocode Implementation

#### `ShardedWAL::log`
```cpp
core::Result<void> ShardedWAL::log(const core::TimeSeries& series) {
    // 1. Calculate hash / shard index
    // We can use the existing SeriesID calculation or re-hash labels
    // Assuming we have access to SeriesID or calculate it cheaply
    size_t shard_idx = get_shard_index(series);
    
    // 2. Delegate to specific shard
    // No global lock here!
    return shards_[shard_idx]->log(series);
}
```

#### `WALShard::log`
```cpp
core::Result<void> WALShard::log(const core::TimeSeries& series) {
    // 1. Lock ONLY this shard
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 2. Measure wait time (Instrumentation)
    auto start = std::chrono::high_resolution_clock::now();
    
    // 3. Serialize and Write (Existing logic)
    // ... serialization ...
    // ... file write ...
    
    // 4. Update metrics
    stats_.write_count++;
    stats_.write_latency += (std::chrono::high_resolution_clock::now() - start);
    
    return core::Result<void>();
}
```

## 4. Instrumentation Plan
We need to measure the impact of sharding.

### 4.1 Metrics to Track
1.  **Global Write Throughput**: Writes per second (across all shards).
2.  **Shard Contention**:
    *   `lock_wait_time`: Time spent waiting for the shard mutex.
    *   If `lock_wait_time` is high, we need more shards.
3.  **Write Latency**: P50, P90, P99 latency for `log()` calls.

### 4.2 Implementation
Add a `WALInstrumentation` singleton or member.
```cpp
struct WALMetrics {
    std::atomic<uint64_t> total_writes{0};
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<uint64_t> total_lock_wait_ns{0};
};
```

## 5. Testing Strategy

### 5.1 Unit Tests (`ShardedWALTest`)
1.  **Basic Functionality**: Write data, replay, verify all data comes back.
2.  **Sharding Logic**: Verify different series go to different files/directories.
3.  **Concurrency**: Spawn `N` threads writing to `M` shards. Verify no data races and data integrity.

### 5.2 Performance Benchmarks (`WALBenchmark`)
1.  **Baseline**: Run existing `WAL` benchmark (single mutex).
2.  **Sharded**: Run `ShardedWAL` benchmark with 1, 4, 8, 16 shards.
3.  **Scenario**: High concurrency (e.g., 64 threads) writing random series.
4.  **Expectation**: Throughput should scale almost linearly with shards until disk I/O saturates.

## 6. Step-by-Step Implementation
1.  **Refactor**: Extract core file writing logic from `WriteAheadLog` into `WALShard` (or keep `WriteAheadLog` as the shard class).
2.  **Implement**: Create `ShardedWAL` wrapper.
3.  **Instrument**: Add metrics to `WALShard`.
4.  **Integrate**: Replace `std::unique_ptr<WriteAheadLog>` in `StorageImpl` with `std::unique_ptr<ShardedWAL>`.
5.  **Test**: Run existing tests + new concurrency tests.
