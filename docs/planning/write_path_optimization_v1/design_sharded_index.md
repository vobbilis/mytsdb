# Sharded Index Design & Implementation Plan

## 1. Problem Statement
The current `Index` uses a single global `std::mutex` to protect the inverted index and series map. This serializes all new series creation (`StorageImpl::write` path) and queries, creating a bottleneck.

## 2. Proposed Solution: Sharded Index
Replace the single `Index` with a `ShardedIndex` that manages multiple independent `IndexShard` instances.
-   **Sharding Strategy**: Partition based on `SeriesID` (or `Labels` hash).
-   **Independence**: Each shard has its own mutex (or R/W lock), allowing concurrent insertions and lookups.
-   **Read Optimization**: Use `std::shared_mutex` for Read-Write locking (multiple readers, single writer per shard).

## 3. Detailed Design

### 3.1 Class Structure

```cpp
// Forward declaration
class IndexShard;

class ShardedIndex {
public:
    ShardedIndex(size_t num_shards = 16);
    
    // Add series to appropriate shard
    core::Result<void> add_series(core::SeriesID id, const core::Labels& labels);
    
    // Find series matching matchers (scatter-gather)
    core::Result<std::vector<core::SeriesID>> find_series(
        const std::vector<std::pair<std::string, std::string>>& matchers);
        
    // Get labels for a series ID
    core::Result<core::Labels> get_labels(core::SeriesID id);

private:
    const size_t num_shards_;
    std::vector<std::unique_ptr<IndexShard>> shards_;
    
    size_t get_shard_index(core::SeriesID id) const;
};

class IndexShard {
public:
    core::Result<void> add_series(core::SeriesID id, const core::Labels& labels);
    core::Result<std::vector<core::SeriesID>> find_series(
        const std::vector<std::pair<std::string, std::string>>& matchers);
    core::Result<core::Labels> get_labels(core::SeriesID id);

private:
    mutable std::shared_mutex mutex_; // R/W lock
    std::unordered_map<core::SeriesID, core::Labels> series_labels_;
    std::map<std::pair<std::string, std::string>, std::vector<core::SeriesID>> postings_;
};
```

### 3.2 Pseudocode Implementation

#### `ShardedIndex::add_series`
```cpp
core::Result<void> ShardedIndex::add_series(core::SeriesID id, const core::Labels& labels) {
    size_t shard_idx = get_shard_index(id);
    return shards_[shard_idx]->add_series(id, labels);
}
```

#### `ShardedIndex::find_series` (Scatter-Gather)
```cpp
core::Result<std::vector<core::SeriesID>> ShardedIndex::find_series(
    const std::vector<std::pair<std::string, std::string>>& matchers) {
    
    std::vector<core::SeriesID> result;
    std::vector<std::future<core::Result<std::vector<core::SeriesID>>>> futures;
    
    // Scatter: Query all shards in parallel (or sequentially if simple)
    // For simplicity, sequential iteration is often fast enough for in-memory index
    // unless shard count is huge. Parallelism adds overhead.
    // Let's start with sequential iteration over shards, but each shard access is independent.
    
    for (auto& shard : shards_) {
        auto shard_result = shard->find_series(matchers);
        if (shard_result.ok()) {
            result.insert(result.end(), shard_result.value().begin(), shard_result.value().end());
        }
    }
    
    return core::Result<std::vector<core::SeriesID>>(result);
}
```

#### `IndexShard::add_series`
```cpp
core::Result<void> IndexShard::add_series(core::SeriesID id, const core::Labels& labels) {
    std::unique_lock lock(mutex_); // Writer lock
    
    // ... existing add logic ...
    
    return core::Result<void>();
}
```

#### `IndexShard::find_series`
```cpp
core::Result<std::vector<core::SeriesID>> IndexShard::find_series(...) {
    std::shared_lock lock(mutex_); // Reader lock
    
    // ... existing find logic ...
}
```

## 4. Instrumentation Plan

### 4.1 Metrics to Track
1.  **Index Write Latency**: Time to add a series.
2.  **Index Read Latency**: Time to find series.
3.  **Lock Contention**: Wait time for locks (Reader vs Writer).

### 4.2 Implementation
Add `IndexInstrumentation` similar to `WALInstrumentation`.

## 5. Testing Strategy

### 5.1 Unit Tests (`ShardedIndexTest`)
1.  **Distribution**: Verify series are distributed across shards.
2.  **Concurrency**: Multiple writers adding series. Multiple readers querying.
3.  **Correctness**: `find_series` returns correct results across all shards.

### 5.2 Performance Benchmarks (`IndexBenchmark`)
1.  **Baseline**: Existing `Index` benchmark.
2.  **Sharded**: `ShardedIndex` with 1, 4, 16 shards.
3.  **Workload**: 
    *   Write-heavy (new series creation).
    *   Read-heavy (queries).
    *   Mixed.

## 6. Step-by-Step Implementation
1.  **Refactor**: Extract `Index` logic into `IndexShard`.
2.  **Implement**: Create `ShardedIndex` wrapper.
3.  **Optimize**: Switch `std::mutex` to `std::shared_mutex` in `IndexShard`.
4.  **Integrate**: Replace `std::unique_ptr<Index>` in `StorageImpl` with `std::unique_ptr<ShardedIndex>`.
5.  **Test**: Run existing tests + new benchmarks.
