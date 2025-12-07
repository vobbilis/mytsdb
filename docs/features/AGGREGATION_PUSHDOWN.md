# Aggregation Pushdown

## Overview

Aggregation Pushdown is a key performance optimization in MyTSDB that moves aggregation computations from the query layer to the storage layer, achieving **~785x speedup** for supported operations.

---

## How It Works

### Traditional Query Path
```
Query → Fetch ALL raw data → Transfer to Query Engine → Aggregate → Result
        (millions of samples)
```

### With Aggregation Pushdown
```
Query → Push aggregation to Storage → Return pre-aggregated result
        (single value per series)
```

---

## Supported Aggregations

| Function | Pushdown Support |
|----------|------------------|
| `sum()` | ✅ Full |
| `avg()` | ✅ Full |
| `min()` | ✅ Full |
| `max()` | ✅ Full |
| `count()` | ✅ Full |
| `sum_over_time()` | ✅ Full |
| `avg_over_time()` | ✅ Full |
| `min_over_time()` | ✅ Full |
| `max_over_time()` | ✅ Full |
| `count_over_time()` | ✅ Full |
| `stddev()` | ⚠️ Partial (requires two-pass) |
| `topk()` / `bottomk()` | ❌ Not supported |

---

## Performance Impact

| Metric | Without Pushdown | With Pushdown | Improvement |
|--------|------------------|---------------|-------------|
| Query Time | 7,850ms | 10ms | **785x** |
| Data Transfer | 10M samples | 1K values | **10,000x** |
| Memory Usage | 800MB | 8MB | **100x** |

---

## Usage

### Automatic Pushdown

Aggregation pushdown is **enabled by default**. The PromQL engine automatically detects pushdown opportunities.

```promql
-- These queries benefit from automatic pushdown:
sum(rate(http_requests_total[5m]))
avg by (service) (cpu_usage)
max(memory_usage)
```

### Configuration

```cpp
QueryConfig config;
config.enable_aggregation_pushdown = true;  // Default: true
config.pushdown_threshold_samples = 10000;  // Min samples to trigger pushdown
```

### Force Disable (For Testing)

```cpp
config.enable_aggregation_pushdown = false;
```

---

## Implementation Details

The `FilterStorage` wrapper intercepts queries and applies pushdown:

```cpp
class FilterStorage : public Storage {
public:
    // Detects aggregation in AST and pushes to storage
    QueryResult query(const ASTNode& ast) override {
        if (can_pushdown(ast)) {
            return storage_->execute_aggregation(ast);
        }
        return default_query(ast);
    }
};
```

### Storage-Side Execution

The storage layer maintains pre-computed aggregates for each block:

- **Block-level aggregates**: Sum, count, min, max per block
- **Incremental updates**: Aggregates updated on each write
- **Merge on read**: Block aggregates merged for query time ranges

---

## Best Practices

1. **Prefer pushdown-compatible aggregations** - Use `sum`, `avg`, `min`, `max`, `count`
2. **Group wisely** - Fewer groups = more efficient pushdown
3. **Combine with time-based filtering** - Reduces blocks to scan
4. **Monitor pushdown hit rate** - Via `mytsdb_pushdown_queries_total` metric

---

## Metrics

| Metric | Description |
|--------|-------------|
| `mytsdb_pushdown_queries_total` | Total queries with pushdown |
| `mytsdb_pushdown_savings_samples` | Samples saved by pushdown |
| `mytsdb_pushdown_time_saved_ms` | Time saved by pushdown |
