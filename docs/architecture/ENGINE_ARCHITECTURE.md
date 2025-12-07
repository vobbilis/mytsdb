# Engine Architecture

## 🧠 System Overview

MyTSDB is a **high-performance, Prometheus-compatible time series database** designed for:
- **High throughput writes** via lock-free/sharded architecture
- **Low latency reads** via concurrent index and zero-copy access
- **Minimal mutex contention** through sharded design

The engine decouples **Compute** (Query Engine) from **Storage** (Data Layer) with systematic optimizations at every level.

---

## ⚡ Write Path Architecture

The write path is optimized for **maximum throughput** with **minimal contention**.

### Lock-Free Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                      WRITE REQUEST                               │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  ShardedWAL (Lock-Free)                                          │
│  ┌─────────┬─────────┬─────────┬─────────┐                      │
│  │ Shard 0 │ Shard 1 │ Shard 2 │ Shard N │  hash(labels) % N    │
│  │ AsyncQ  │ AsyncQ  │ AsyncQ  │ AsyncQ  │                      │
│  └─────────┴─────────┴─────────┴─────────┘                      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  ShardedIndex (Concurrent)                                       │
│  ┌─────────┬─────────┬─────────┬─────────┐                      │
│  │ Shard 0 │ Shard 1 │ Shard 2 │ Shard N │                      │
│  │InvIndex │InvIndex │InvIndex │InvIndex │                      │
│  └─────────┴─────────┴─────────┴─────────┘                      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  active_series_: tbb::concurrent_hash_map                        │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  SeriesID → Series (in-memory buffer)                     │   │
│  │  • Accessor-based updates (no global lock)               │   │
│  │  • Per-series locking                                     │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  series_blocks_: tbb::concurrent_hash_map                        │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  SeriesID → vector<BlockPtr>                              │   │
│  │  • Lock-free block assignment                            │   │
│  │  • Significant contention reduction                      │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Key Optimizations

| Component | Technology | Benefit |
|-----------|------------|---------|
| **WAL** | `ShardedWAL` + `AsyncWALShard` | Lock-free durability |
| **Index** | `ShardedIndex` | Concurrent label lookups |
| **Series Map** | `tbb::concurrent_hash_map` | Zero global contention |
| **Block Map** | `tbb::concurrent_hash_map` | **Non-blocking block assignment** |

### Write Performance Goals
- **Throughput**: Optimized for high volume ingestion
- **Latency**: Minimized write latency via Async WAL
- **Contention**: Minimized via Sharding and Concurrent Maps

---

## 📖 Read Path Architecture

The read path is optimized for **vectorized, zero-copy data access**.

### Hybrid Query Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                      PromQL QUERY                                │
│  sum(rate(http_requests_total{job="api"}[5m]))                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  PromQL Parser + Optimizer                                       │
│  • AST construction                                              │
│  • Time-range pruning                                            │
│  • Aggregation pushdown decision                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  BufferedStorageAdapter (O(1) Cache)                             │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ Matcher Cache: {job="api"} → [TimeRanges]                │   │
│  │ Lookup: O(1) complexity                                  │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  StorageImpl.read_from_blocks_nolock()                           │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ 1. series_blocks_.find(accessor, series_id)              │   │
│  │ 2. For each block:                                        │   │
│  │    • block->read_columns()  // Zero-copy!                │   │
│  │    • Direct vector<int64_t>, vector<double> access       │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
            ┌─────────────────┴─────────────────┐
            ▼                                   ▼
┌──────────────────────┐          ┌───────────────────────────────┐
│  HOT TIER (Memory)   │          │  COLD TIER (Parquet)          │
│  • BlockImpl         │          │  • ParquetBlock               │
│  • Gorilla compress  │          │  • ZSTD + Dictionary          │
│  • shared_mutex      │          │  • Arrow RecordBatch          │
└──────────────────────┘          └───────────────────────────────┘
```

### Zero-Copy Read Path

The critical optimization is avoiding `Sample` object creation:

```cpp
// OLD: Object-heavy path (slow)
for (auto& sample : block->read()) {
    result.add_sample(sample);  // Object allocation
}

// NEW: Zero-copy columnar path (fast)
auto [timestamps, values] = block->read_columns(labels);
// Direct vector access - no Sample objects created
```

### Read Performance Goals
- **Throughput**: High QPS support
- **Latency**: Low latency query execution
- **Efficiency**: Zero-copy data paths where possible

---

## 💾 Storage Tiers

### Multi-Tier Architecture

| Tier | Storage | Compression | Access | Age |
|------|---------|-------------|--------|-----|
| **HOT** | `BlockImpl` (Memory) | Gorilla | Immediate | < 2h |
| **WARM** | Sealed Blocks | Delta-of-Delta | Fast | 2h - 24h |
| **COLD** | `ParquetBlock` (Disk) | ZSTD + Dict | Columnar | > 24h |

### Parquet Optimizations

1. **Time-Based Partitioning**: `data/YYYY/MM/DD/` structure
2. **Row Group Optimization**: Batch demotion prevents fragmentation
3. **Dictionary Encoding**: High-cardinality labels compressed
4. **Predicate Pushdown**: Skip irrelevant row groups

---

## 🔬 Self-Monitoring

MyTSDB exports **35+ internal metrics** via Prometheus API at `:9090/api/v1/query`:

### Key Metrics

| Metric | Description |
|--------|-------------|
| `mytsdb_write_mutex_lock_seconds_total` | Mutex contention monitoring |
| `mytsdb_write_sample_append_seconds_total` | I/O time monitoring |
| `mytsdb_query_duration_seconds_bucket` | Query latency histogram |
| `mytsdb_read_index_search_seconds_total` | Index performance monitoring |

### Automated Benchmarking

The `k8s_combined_benchmark` tool automatically queries all metrics:

```bash
./build/tools/k8s_combined_benchmark --preset quick --duration 5
# Outputs all server-side metrics at end of run
```

---

## 📊 Performance Architecture
The system is architected for:
- **Scalable Read/Write** operations
- **Low Contention** locking strategies
- **High Throughput** ingestion pipeline

---

## 🔮 Future Improvements

1. **Async Block Persistence** - Remove 9.4s Block Persist bottleneck
2. **SIMD Aggregations** - Vectorized sum/avg/count
3. **Distributed Sharding** - Multi-node query scatter-gather
4. **JIT Expression Compilation** - PromQL to native code

