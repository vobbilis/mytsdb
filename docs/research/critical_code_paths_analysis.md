# MyTSDB Critical Code Paths Analysis

**Generated:** 2025-11-30  
**Purpose:** Deep analysis of read and write paths for performance optimization  
**Scope:** Complete code path tracing from API to storage

---

## 🎯 Executive Summary

This document provides a thorough analysis of the critical code paths in MyTSDB, focusing on both **write** and **read** operations. Understanding these paths is essential for:
- Performance optimization
- Bottleneck identification  
- Latency reduction
- Throughput improvement

### Key Findings

**Write Path:**
- **Latency:** ~12μs per write (theoretical), ~100μs (observed)
- **Bottlenecks:** WAL I/O, lock contention, block persistence
- **Optimization Potential:** 8-10x improvement possible

**Read Path:**
- **Latency:** ~10-100ns (L1 cache hit), ~1-10ms (disk)
- **Bottlenecks:** Cache misses, block decompression, index lookups
- **Optimization Potential:** Cache hit ratio improvements

---

## 📝 WRITE PATH ANALYSIS

### Overview

The write path handles ingestion of time series data from various sources (Prometheus Remote Write, OTEL, direct API calls) and persists it durably to storage.

### Complete Write Path Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                    WRITE PATH (Hot Path)                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  1. API Entry Point                                              │
│     └─ StorageImpl::write(TimeSeries)                           │
│        ├─ Input validation (~100ns)                             │
│        ├─ Empty series check                                     │
│        └─ Label validation                                       │
│                                                                   │
│  2. WAL (Write-Ahead Log) - DURABILITY                          │
│     └─ ShardedWAL::log(series) [~10μs]                          │
│        ├─ Shard selection (hash-based)                          │
│        ├─ Serialize series (~1μs)                               │
│        │  ├─ Label count + key/value pairs                      │
│        │  └─ Sample count + timestamp/value pairs               │
│        ├─ Write to segment file (~8μs)                          │
│        │  ├─ Data length (uint32_t)                             │
│        │  ├─ Serialized data                                    │
│        │  └─ Optional flush (configurable)                      │
│        └─ Segment rotation check (>64MB)                        │
│                                                                   │
│  3. Series ID Calculation [~100ns]                              │
│     └─ calculate_series_id(labels)                              │
│        ├─ Hash labels to SeriesID (uint64_t)                    │
│        └─ Deterministic hash function                           │
│                                                                   │
│  4. Concurrent Map Lookup/Insert [~500ns]                       │
│     └─ active_series_.insert(accessor, series_id)               │
│        ├─ TBB concurrent_hash_map                               │
│        ├─ Lock-free for readers                                 │
│        └─ Returns accessor to series                            │
│                                                                   │
│  5. NEW SERIES PATH (if created)                                │
│     ├─ Index Update [~500ns]                                    │
│     │  └─ ShardedIndex::add_series(id, labels)                  │
│     │     ├─ Shard selection (hash-based)                       │
│     │     ├─ Forward index: id → labels                         │
│     │     └─ Inverted index: (key,val) → [ids]                  │
│     │                                                             │
│     └─ Series Creation [~1μs]                                   │
│        └─ new Series(id, labels, type, granularity)             │
│           ├─ Initialize metadata                                │
│           └─ Create empty block list                            │
│                                                                   │
│  6. Sample Append [~1μs per sample]                             │
│     └─ Series::append(sample)                                   │
│        ├─ Acquire series mutex (exclusive)                      │
│        ├─ Create block if needed                                │
│        │  └─ BlockImpl with compressors                         │
│        ├─ Buffer sample (uncompressed)                          │
│        │  ├─ timestamps_uncompressed.push_back()                │
│        │  └─ values_uncompressed.push_back()                    │
│        ├─ Update time range                                     │
│        └─ Check if block full (120 samples)                     │
│                                                                   │
│  7. Cache Update [~100ns]                                       │
│     └─ CacheHierarchy::put(series_id, series)                   │
│        ├─ Try L1 cache first                                    │
│        ├─ LRU eviction if full                                  │
│        └─ Update access metadata                                │
│                                                                   │
│  8. BLOCK PERSISTENCE (if block full)                           │
│     ├─ Series::seal_block() [~10μs]                             │
│     │  ├─ Compress buffered data                                │
│     │  │  ├─ Timestamp compression (Delta-of-Delta)             │
│     │  │  └─ Value compression (Gorilla)                        │
│     │  ├─ Clear uncompressed buffers                            │
│     │  └─ Mark as sealed                                        │
│     │                                                             │
│     └─ BlockManager::seal_and_persist_block() [~90-260μs]       │
│        ├─ Serialize block                                       │
│        ├─ Write to disk                                         │
│        └─ Update block index                                    │
│                                                                   │
│  9. Metadata Update [~10ns]                                     │
│     └─ series_blocks_[series_id].push_back(block)               │
│        └─ Acquire StorageImpl mutex (brief)                     │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Detailed Component Analysis

#### 1. WAL (Write-Ahead Log)

**Purpose:** Crash recovery and durability

**Implementation:** `ShardedWAL` with 16 shards

**Performance:**
```cpp
// Serialization format (binary)
[label_count:uint32]
  [key_len:uint32][key:bytes][val_len:uint32][val:bytes] ...
[sample_count:uint32]
  [timestamp:int64][value:double] ...

// Write operation
1. Serialize: ~1μs
2. File write: ~8μs  
3. Optional flush: +2-5μs
Total: ~10μs per write
```

**Optimizations:**
- Sharded design (16 shards) reduces contention
- Buffered writes (flush configurable)
- Segment rotation at 64MB
- Binary format (no parsing overhead)

**Bottleneck:** File I/O is the primary cost (~80% of WAL time)

#### 2. Index Updates

**Purpose:** Fast label-based lookups

**Implementation:** `ShardedIndex` with 16 shards

**Data Structures:**
```cpp
// Forward index: O(1) lookup
std::map<SeriesID, Labels> series_labels_;

// Inverted index: O(1) lookup per label pair
std::map<pair<string,string>, vector<SeriesID>> postings_;
```

**Performance:**
```
New series: ~500ns (2 map inserts)
Existing series: 0ns (no index update)
```

**Optimizations:**
- Sharded design reduces lock contention
- Shared mutex (multiple readers, single writer)
- Sorted posting lists for fast intersection

#### 3. Series Append

**Purpose:** Buffer samples in memory blocks

**Implementation:** `Series::append()` with `BlockImpl`

**Performance:**
```cpp
// Per-sample cost
1. Mutex acquisition: ~50ns
2. Vector push_back: ~20ns (amortized)
3. Time range update: ~10ns
4. Block full check: ~5ns
Total: ~85ns per sample
```

**Block Management:**
```
- Samples buffered uncompressed
- Block sealed at 120 samples (configurable)
- Compression happens on seal
- Uncompressed buffers cleared after compression
```

**Optimizations:**
- Deferred compression (batch at seal time)
- Vector pre-allocation
- Lock-free reads (shared_mutex)

#### 4. Cache Hierarchy

**Purpose:** Fast access to hot data

**Implementation:** 3-level cache (L1/L2/L3)

**Write Path:**
```
L1 Cache (WorkingSetCache):
- Size: 500 entries (default)
- Access: ~10-100ns
- Eviction: LRU
- Status: ✅ ACTIVE

L2 Cache (MemoryMappedCache):
- Size: 10,000 entries (default)
- Access: ~1-10μs
- Status: ❌ DISABLED (segfaults)

L3 Cache (Disk):
- Size: Unlimited
- Access: ~1-10ms
- Via BlockManager
```

**Performance:**
```
Cache put: ~100ns (L1 hit)
Cache put: ~1μs (L1 miss, L2 hit)
Cache eviction: ~200ns (LRU update)
```

#### 5. Block Persistence

**Purpose:** Durable storage of sealed blocks

**Implementation:** `BlockManager::seal_and_persist_block()`

**Performance:**
```
Compression:
- Delta-of-Delta (timestamps): ~2μs for 120 samples
- Gorilla (values): ~3μs for 120 samples
- Total compression: ~5μs

Serialization:
- Header: ~100ns
- Compressed data: ~500ns
- Total: ~600ns

Disk I/O:
- Write system call: ~50-200μs
- fsync (if enabled): +1-5ms
- Total: ~90-260μs (without fsync)
```

**Bottleneck:** Disk I/O dominates (~95% of persistence time)

**Optimization Opportunity:**
- Batch multiple blocks
- Async I/O
- Write coalescing
- Deferred persistence (WAL provides durability)

### Write Path Performance Breakdown

**Theoretical Minimum (Hot Path):**
```
Component                    Time (μs)    % of Total
─────────────────────────────────────────────────────
Input validation             0.1          0.8%
WAL write                    10.0         83.3%
Series ID calculation        0.1          0.8%
Map insert/lookup            0.5          4.2%
Index update (new series)    0.5          4.2%
Sample append                0.085        0.7%
Cache update                 0.1          0.8%
Metadata update              0.01         0.1%
─────────────────────────────────────────────────────
TOTAL (no block persist)     12.0         100%
```

**With Block Persistence (every 120 samples):**
```
Base write path              12.0 μs
Block seal (compression)     5.0 μs
Block persistence (I/O)      150.0 μs (avg)
─────────────────────────────────────────────────────
TOTAL (with persistence)     167.0 μs
Amortized per sample         1.4 μs
```

**Observed Performance:**
```
Current: ~10K writes/sec = ~100μs per write
Gap: 8-10x slower than theoretical
```

**Likely Bottlenecks:**
1. **WAL I/O** - File system overhead
2. **Lock Contention** - Mutex waits
3. **Block Persistence** - Synchronous I/O
4. **Memory Allocation** - Despite object pools

---

## 📖 READ PATH ANALYSIS

### Overview

The read path retrieves time series data for queries, supporting both single-series reads and multi-series queries with label matchers.

### Complete Read Path Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                     READ PATH (Hot Path)                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  1. API Entry Point                                              │
│     └─ StorageImpl::read(labels, start, end)                    │
│        ├─ Validation (~50ns)                                    │
│        │  ├─ Initialized check                                  │
│        │  └─ Time range check                                   │
│        └─ Acquire shared mutex (read lock)                      │
│                                                                   │
│  2. Series ID Calculation [~100ns]                              │
│     └─ calculate_series_id(labels)                              │
│        └─ Hash labels to SeriesID                               │
│                                                                   │
│  3. CACHE HIERARCHY LOOKUP                                      │
│     └─ CacheHierarchy::get(series_id)                           │
│        │                                                          │
│        ├─ L1 Cache (WorkingSetCache) [~10-100ns]                │
│        │  ├─ Hash table lookup                                  │
│        │  ├─ LRU update                                         │
│        │  └─ ✅ CACHE HIT → Return immediately                  │
│        │                                                          │
│        ├─ L2 Cache (MemoryMappedCache) [~1-10μs]                │
│        │  ├─ Memory-mapped file access                          │
│        │  ├─ ❌ DISABLED (segfaults)                            │
│        │  └─ Consider L1 promotion                              │
│        │                                                          │
│        └─ L3 Miss → Continue to active series                   │
│                                                                   │
│  4. ACTIVE SERIES LOOKUP [~500ns]                               │
│     └─ active_series_.find(accessor, series_id)                 │
│        ├─ TBB concurrent_hash_map                               │
│        ├─ Lock-free for concurrent readers                      │
│        └─ Returns accessor to Series object                     │
│                                                                   │
│  5. IN-MEMORY READ (if found) [~1-5μs]                          │
│     └─ Series::Read(start, end)                                 │
│        ├─ Acquire shared mutex (read lock)                      │
│        ├─ Iterate sealed blocks                                 │
│        │  ├─ Time range overlap check                           │
│        │  ├─ BlockImpl::read(labels)                            │
│        │  │  ├─ Check if compressed                             │
│        │  │  ├─ Decompress if needed                            │
│        │  │  │  ├─ Delta-of-Delta (timestamps)                  │
│        │  │  │  └─ Gorilla (values)                             │
│        │  │  └─ Filter to time range                            │
│        │  └─ Collect samples                                    │
│        ├─ Read current active block                             │
│        │  └─ Uncompressed buffer access                         │
│        ├─ Sort samples chronologically                          │
│        └─ Populate cache (if not empty)                         │
│                                                                   │
│  6. BLOCK-BASED READ (if not in active series) [~1-10ms]        │
│     └─ read_from_blocks_nolock(labels, start, end)              │
│        ├─ Lookup in series_blocks_ map                          │
│        ├─ Iterate relevant blocks                               │
│        │  ├─ Time range filtering                               │
│        │  ├─ Block decompression                                │
│        │  └─ Sample extraction                                  │
│        ├─ Sort samples                                          │
│        └─ Populate cache                                        │
│                                                                   │
│  7. QUERY PATH (multi-series) [~1-100ms]                        │
│     └─ StorageImpl::query(matchers, start, end)                 │
│        │                                                          │
│        ├─ INDEX LOOKUP [~100μs - 1ms]                           │
│        │  └─ ShardedIndex::find_series(matchers)                │
│        │     ├─ Equality matchers (inverted index)              │
│        │     │  ├─ O(1) lookup per matcher                      │
│        │     │  └─ Set intersection for AND                     │
│        │     ├─ Regex/NotEqual matchers (scan)                  │
│        │     │  └─ Filter candidates                            │
│        │     └─ Return matching series IDs                      │
│        │                                                          │
│        ├─ PARALLEL READ [~1-10ms per series]                    │
│        │  └─ For each matching series:                          │
│        │     ├─ Cache lookup                                    │
│        │     ├─ Active series read                              │
│        │     └─ Block-based read                                │
│        │                                                          │
│        └─ RESULT AGGREGATION                                    │
│           ├─ Collect all series                                 │
│           ├─ Limit results (1000 max)                           │
│           └─ Return vector<TimeSeries>                          │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Detailed Component Analysis

#### 1. Cache Hierarchy (Read Path)

**L1 Cache Hit (Best Case):**
```
1. Hash table lookup: ~50ns
2. LRU update: ~30ns
3. Access metadata update: ~20ns
Total: ~100ns
```

**L2 Cache Hit (Currently Disabled):**
```
1. Memory-mapped file access: ~1μs
2. Deserialization: ~500ns
3. L1 promotion check: ~100ns
Total: ~1.6μs
```

**Cache Miss:**
```
Falls through to active series or block-based read
```

**Cache Hit Ratio:**
```
L1: 80-90% (for hot data)
L2: N/A (disabled)
L3: Remainder
```

#### 2. Active Series Read

**Purpose:** Read from in-memory series objects

**Performance:**
```cpp
// Series::Read() breakdown
1. Mutex acquisition (shared): ~50ns
2. Iterate sealed blocks: ~100ns per block
3. Block decompression: ~2-5μs per block
   - Delta-of-Delta: ~1μs
   - Gorilla: ~1-2μs
4. Current block read: ~500ns (uncompressed)
5. Sample filtering: ~10ns per sample
6. Sorting: ~100ns (small datasets)
Total: ~1-5μs (typical)
```

**Optimization:**
- Shared mutex allows concurrent reads
- Uncompressed current block (fast access)
- Lazy decompression (only if needed)

#### 3. Block-Based Read

**Purpose:** Read from persisted blocks on disk

**Performance:**
```
1. Block lookup: ~500ns (map access)
2. Disk I/O: ~1-5ms (per block)
3. Decompression: ~5μs per block
4. Sample extraction: ~10ns per sample
Total: ~1-10ms (disk-bound)
```

**Bottleneck:** Disk I/O dominates (~99% of time)

#### 4. Index Lookup (Query Path)

**Purpose:** Find series matching label matchers

**Performance:**
```cpp
// Equality matchers (fast path)
1. Inverted index lookup: ~100ns per matcher
2. Set intersection: ~500ns (sorted lists)
3. Total: ~1μs for simple queries

// Regex matchers (slow path)
1. Get all candidates: ~10μs
2. Regex match per series: ~1μs per series
3. Total: ~1-10ms for complex queries
```

**Optimization:**
- Inverted index for equality matchers
- Sorted posting lists for fast intersection
- Sharded design (16 shards) reduces contention

### Read Path Performance Breakdown

**Best Case (L1 Cache Hit):**
```
Component                    Time (ns)    % of Total
─────────────────────────────────────────────────────
Validation                   50           33%
Series ID calculation        100          67%
L1 cache lookup              100          67%
─────────────────────────────────────────────────────
TOTAL                        150          100%
```

**Typical Case (Active Series, No Cache):**
```
Component                    Time (μs)    % of Total
─────────────────────────────────────────────────────
Validation                   0.05         1%
Series ID calculation        0.1          2%
Cache miss                   0.1          2%
Active series lookup         0.5          10%
Series read (1 block)        2.0          40%
Decompression                2.0          40%
Filtering/sorting            0.25         5%
─────────────────────────────────────────────────────
TOTAL                        5.0          100%
```

**Worst Case (Disk Read):**
```
Component                    Time (ms)    % of Total
─────────────────────────────────────────────────────
Cache/active series miss     0.001        0.01%
Block lookup                 0.001        0.01%
Disk I/O (1 block)           5.0          99.96%
Decompression                0.005        0.05%
Filtering/sorting            0.001        0.01%
─────────────────────────────────────────────────────
TOTAL                        5.007        100%
```

**Query Path (100 series):**
```
Component                    Time (ms)    % of Total
─────────────────────────────────────────────────────
Index lookup                 0.1          1%
Read 100 series (cached)     0.015        0.15%
Read 100 series (memory)     0.5          5%
Read 100 series (disk)       500          94.85%
Result aggregation           0.1          1%
─────────────────────────────────────────────────────
TOTAL (all cached)           0.215        
TOTAL (all memory)           0.7          
TOTAL (all disk)             500.7        
```

---

## 🔍 CRITICAL BOTTLENECKS IDENTIFIED

### Write Path Bottlenecks

**1. WAL File I/O** (83% of write time)
```
Current: ~10μs per write
Optimization potential:
- Batch writes: 5-10x improvement
- Async I/O: 2-3x improvement
- Memory-mapped WAL: 2x improvement
Target: ~1-2μs per write
```

**2. Block Persistence** (when triggered)
```
Current: ~150μs per block (120 samples)
Optimization potential:
- Async persistence: 10x improvement
- Batch persistence: 5x improvement
- Deferred persistence: Eliminate from hot path
Target: <10μs amortized
```

**3. Lock Contention**
```
Current: Unknown (needs profiling)
Optimization potential:
- Lock-free data structures: 2-5x improvement
- Finer-grained locking: 2x improvement
- Read-copy-update (RCU): 3-5x improvement
```

### Read Path Bottlenecks

**1. Cache Misses** (L1 hit ratio: 80-90%)
```
Current: 10-20% miss rate
Optimization potential:
- Larger L1 cache: +5-10% hit rate
- Fix L2 cache: +10-15% hit rate
- Predictive prefetching: +5% hit rate
Target: 95%+ hit rate
```

**2. Disk I/O** (for cold data)
```
Current: ~5ms per block read
Optimization potential:
- SSD vs HDD: 10-100x improvement
- Prefetching: 2-5x improvement
- Larger caches: Reduce frequency
Target: <500μs per block
```

**3. Decompression** (for cached data)
```
Current: ~2-5μs per block
Optimization potential:
- SIMD acceleration: 2-4x improvement
- Better compression: Trade-off
- Lazy decompression: Avoid when possible
Target: <1μs per block
```

---

## 🚀 OPTIMIZATION RECOMMENDATIONS

### High-Impact Optimizations

**1. Async WAL Writes** (Priority: HIGH)
```
Impact: 5-10x write throughput improvement
Complexity: Medium
Risk: Medium (durability trade-offs)

Implementation:
- Ring buffer for WAL entries
- Background flush thread
- Configurable flush interval
- Group commit for batching
```

**2. Deferred Block Persistence** (Priority: HIGH)
```
Impact: Eliminate 90-260μs from write path
Complexity: Low
Risk: Low (WAL provides durability)

Implementation:
- Remove immediate persistence for new series
- Persist only when block is full
- Background compaction thread
- WAL replay handles recovery
```

**3. Fix L2 Cache** (Priority: MEDIUM)
```
Impact: +10-15% cache hit rate
Complexity: High
Risk: High (currently segfaults)

Investigation needed:
- Memory-mapped file issues
- Concurrency bugs
- Resource cleanup
```

**4. SIMD Compression/Decompression** (Priority: MEDIUM)
```
Impact: 2-4x compression/decompression speed
Complexity: High
Risk: Low

Implementation:
- AVX-512 for Gorilla algorithm
- SIMD Delta-of-Delta
- Vectorized operations
```

### Medium-Impact Optimizations

**5. Larger Object Pools** (Priority: LOW)
```
Impact: Reduce allocation overhead
Complexity: Low
Risk: Low

Current: 99%+ reuse rate (already good)
Potential: Marginal improvement
```

**6. Predictive Prefetching** (Priority: MEDIUM)
```
Impact: +5% cache hit rate
Complexity: Medium
Risk: Low

Implementation:
- Access pattern tracking (already exists)
- Background prefetch thread
- Configurable prefetch depth
```

**7. Query Parallelization** (Priority: MEDIUM)
```
Impact: 2-4x query throughput
Complexity: Medium
Risk: Low

Implementation:
- Parallel series reads
- Thread pool for queries
- Batch result aggregation
```

---

## 📊 PERFORMANCE TARGETS

### Write Path Targets

```
Current Performance:
- Throughput: ~10K writes/sec
- Latency: ~100μs per write

Target Performance (Optimized):
- Throughput: 80K+ writes/sec
- Latency: ~12μs per write (8x improvement)

Stretch Goal:
- Throughput: 200K+ writes/sec
- Latency: ~5μs per write (20x improvement)
```

### Read Path Targets

```
Current Performance:
- L1 cache hit: ~100ns
- Memory read: ~5μs
- Disk read: ~5ms

Target Performance (Optimized):
- L1 cache hit: ~50ns (2x improvement)
- L2 cache hit: ~1μs (new capability)
- Memory read: ~2μs (2.5x improvement)
- Disk read: ~500μs (10x improvement with SSD)

Cache Hit Rate Target:
- Current: 80-90% (L1 only)
- Target: 95%+ (L1+L2 combined)
```

---

## 🔬 PROFILING RECOMMENDATIONS

### What to Profile

**1. Write Path:**
```
- WAL write latency distribution
- Lock wait times (mutex contention)
- Block persistence frequency
- Memory allocation overhead
- CPU time per component
```

**2. Read Path:**
```
- Cache hit/miss distribution
- Decompression time per algorithm
- Disk I/O latency
- Index lookup performance
- Query execution time breakdown
```

**3. System-Level:**
```
- CPU utilization per core
- Memory bandwidth usage
- Disk I/O patterns
- Network I/O (for remote storage)
- Lock contention hotspots
```

### Profiling Tools

```
macOS:
- Instruments (Time Profiler)
- DTrace
- perf (if available)

Linux:
- perf
- Flamegraphs
- BPF tools

Application-Level:
- WritePerformanceInstrumentation (already exists)
- Custom timing instrumentation
- Statistics collection
```

---

## 📝 CONCLUSION

### Key Takeaways

1. **Write Path is I/O Bound**
   - WAL writes: 83% of time
   - Block persistence: 90-260μs when triggered
   - Optimization potential: 8-10x improvement

2. **Read Path is Cache-Dependent**
   - L1 hit: ~100ns (excellent)
   - L2 disabled (needs fix)
   - Disk read: ~5ms (poor)
   - Optimization potential: 10-100x with better caching

3. **Architecture is Sound**
   - Good separation of concerns
   - Lock-free data structures where possible
   - Object pooling working well (99%+ reuse)
   - Compression effective (4-6x ratio)

4. **Low-Hanging Fruit**
   - Async WAL writes (5-10x improvement)
   - Deferred block persistence (eliminate from hot path)
   - Fix L2 cache (+10-15% hit rate)
   - SIMD compression (2-4x faster)

### Next Steps

1. **Profile current performance** to validate theoretical analysis
2. **Implement async WAL** as highest-impact optimization
3. **Fix L2 cache** to improve read performance
4. **Benchmark after each change** to measure actual improvement
5. **Iterate** based on profiling results

---

**End of Analysis**
