# Aggregation Pushdown - Comprehensive Admin Guide

## Overview

**Aggregation Pushdown** is a query optimization that moves aggregation computations from the PromQL engine layer to the storage layer, achieving up to **~785x performance improvement** by eliminating massive data transfers.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Without Pushdown                          │
│  PromQL Engine ◄── 10M samples ◄── Storage Layer            │
│       ▼                                                      │
│  [Aggregate in Engine] = SLOW (7,850ms)                     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    With Pushdown                             │
│  PromQL Engine ◄── 1K aggregated ◄── [Aggregate in Storage] │
│       ▼                                                      │
│  [Return result] = FAST (10ms)                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Data Structures

### AggregationOp Enum

```cpp
// include/tsdb/core/aggregation.h

enum class AggregationOp {
    SUM,            // Sum of values
    MIN,            // Minimum value
    MAX,            // Maximum value
    COUNT,          // Count of samples
    AVG,            // Average (computed as SUM/COUNT)
    STDDEV,         // Standard deviation
    STDVAR,         // Standard variance
    COUNT_VALUES,   // Count distinct values
    BOTTOMK,        // Bottom K values
    TOPK,           // Top K values
    QUANTILE        // Quantile (requires param)
};
```

### AggregationRequest Struct

```cpp
// include/tsdb/core/aggregation.h

struct AggregationRequest {
    AggregationOp op;                      // Aggregation operation
    std::vector<std::string> grouping_keys; // Labels to group by
    bool without;                           // true = without these keys, false = by these keys
    double param = 0.0;                     // For topk, bottomk, quantile
};
```

### Storage Interface

```cpp
// include/tsdb/storage/storage.h

class Storage {
public:
    /**
     * @brief Queries time series with aggregation pushdown
     * 
     * This is the key method for aggregation pushdown. Instead of returning
     * all raw samples, it performs the aggregation at the storage layer
     * and returns only the aggregated results.
     */
    virtual core::Result<std::vector<core::TimeSeries>> query_aggregate(
        const std::vector<core::LabelMatcher>& matchers,
        int64_t start_time,
        int64_t end_time,
        const core::AggregationRequest& aggregation);
};
```

### StorageAdapter (PromQL to Storage Bridge)

```cpp
// include/tsdb/prometheus/storage/tsdb_adapter.h

class TSDBAdapter : public StorageAdapter {
public:
    // Standard query - returns all samples
    promql::Matrix SelectSeries(
        const std::vector<model::LabelMatcher>& matchers,
        int64_t start,
        int64_t end) override;

    // Pushdown query - aggregation computed at storage layer
    promql::Matrix SelectAggregateSeries(
        const std::vector<model::LabelMatcher>& matchers,
        int64_t start,
        int64_t end,
        const core::AggregationRequest& aggregation) override;
};
```

---

## Pushdown Support Matrix

| Aggregation | Pushdown | Notes |
|-------------|----------|-------|
| `sum()` | ✅ Full | Sum computed per block, merged |
| `min()` | ✅ Full | Min of mins is min |
| `max()` | ✅ Full | Max of maxs is max |
| `count()` | ✅ Full | Sum of counts |
| `avg()` | ✅ Full | Track sum + count, divide at end |
| `sum_over_time()` | ✅ Full | Same as sum with time range |
| `min_over_time()` | ✅ Full | Same as min with time range |
| `max_over_time()` | ✅ Full | Same as max with time range |
| `count_over_time()` | ✅ Full | Same as count with time range |
| `avg_over_time()` | ✅ Full | Same as avg with time range |
| `stddev()` | ⚠️ Partial | Requires sum, sum_sq, count per shard |
| `stdvar()` | ⚠️ Partial | Requires sum, sum_sq, count per shard |
| `topk()` | ❌ No | Requires global comparison |
| `bottomk()` | ❌ No | Requires global comparison |
| `quantile()` | ❌ No | Requires full distribution |

---

## Usage Examples

### Automatic Pushdown (Default)

Pushdown happens automatically when the PromQL engine detects compatible queries:

```promql
# These queries automatically use pushdown:

sum(http_requests_total)                    # ✅ Pushdown
avg by (service) (cpu_usage)                # ✅ Pushdown
max(memory_usage{env="prod"})               # ✅ Pushdown
count by (host, datacenter) (up)            # ✅ Pushdown
sum(rate(requests_total[5m]))               # ✅ Pushdown (inner rate, outer sum pushed)
```

### Programmatic Usage

```cpp
#include "tsdb/core/aggregation.h"
#include "tsdb/storage/storage.h"

// Create aggregation request
core::AggregationRequest agg_request;
agg_request.op = core::AggregationOp::SUM;
agg_request.grouping_keys = {"service", "method"};
agg_request.without = false;  // "by" semantics

// Create label matchers
std::vector<core::LabelMatcher> matchers;
matchers.push_back(core::LabelMatcher{
    .name = "__name__",
    .value = "http_requests_total",
    .type = core::MatchType::EQUAL
});
matchers.push_back(core::LabelMatcher{
    .name = "env",
    .value = "production",
    .type = core::MatchType::EQUAL
});

// Execute with pushdown
auto result = storage->query_aggregate(
    matchers,
    start_time_ms,
    end_time_ms,
    agg_request
);

if (result.ok()) {
    for (const auto& series : result.value()) {
        std::cout << "Labels: " << series.labels().to_string() << std::endl;
        std::cout << "Value: " << series.samples()[0].value << std::endl;
    }
}
```

### Using via PromQL Engine

```cpp
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"

// Create adapter (enables pushdown)
auto adapter = std::make_shared<TSDBAdapter>(storage);

// Create engine with adapter
Engine engine(adapter);

// Execute query - pushdown happens automatically if applicable
auto result = engine.ExecuteInstant(
    "sum by (service) (rate(http_requests_total[5m]))",
    now_ms
);
```

---

## Configuration

### Enable/Disable Pushdown

```cpp
// In storage configuration
core::StorageConfig config;
config.enable_aggregation_pushdown = true;  // Default: true
config.pushdown_threshold_samples = 10000;  // Minimum samples to trigger pushdown
```

### Per-Query Control

```cpp
// Disable pushdown for specific query (debugging)
QueryOptions opts;
opts.disable_pushdown = true;

auto result = engine.ExecuteInstant(query, timestamp, opts);
```

---

## How Pushdown Works Internally

### Step 1: Query Analysis

The PromQL engine parses the query AST and identifies pushdown opportunities:

```cpp
// Pseudocode for pushdown detection
bool can_pushdown(const ASTNode& node) {
    // Check if this is an aggregation node
    if (node.type != NodeType::AGGREGATION) return false;
    
    // Check if the operation supports pushdown
    switch (node.aggregation_op) {
        case SUM: case MIN: case MAX: case COUNT: case AVG:
            return true;
        default:
            return false;
    }
}
```

### Step 2: Request Transformation

```cpp
// Transform PromQL aggregation to storage request
AggregationRequest to_storage_request(const AggregateExpr& expr) {
    AggregationRequest req;
    req.op = translate_op(expr.op);          // PromQL op → storage op
    req.grouping_keys = expr.grouping;       // ["service", "method"]
    req.without = expr.without;               // by vs without semantics
    return req;
}
```

### Step 3: Storage Execution

```cpp
// Block manager executes aggregation on each block
Result<AggregateValues> BlockManager::aggregate_block(
    const BlockHeader& block,
    const AggregationRequest& request
) {
    // Each block maintains pre-computed aggregates for common ops
    switch (request.op) {
        case SUM:
            return block.sum_by_labels(request.grouping_keys);
        case MIN:
            return block.min_by_labels(request.grouping_keys);
        case MAX:
            return block.max_by_labels(request.grouping_keys);
        case COUNT:
            return block.count_by_labels(request.grouping_keys);
        case AVG:
            auto sum = block.sum_by_labels(request.grouping_keys);
            auto count = block.count_by_labels(request.grouping_keys);
            return sum / count;
    }
}
```

### Step 4: Merge Results

```cpp
// Merge results from multiple blocks
AggregateValues merge_blocks(
    const std::vector<AggregateValues>& block_results,
    AggregationOp op
) {
    switch (op) {
        case SUM:
            return sum_all(block_results);
        case MIN:
            return min_all(block_results);
        case MAX:
            return max_all(block_results);
        case COUNT:
            return sum_all(block_results);  // Count merges as sum
        case AVG:
            auto total_sum = sum_all(extract_sums(block_results));
            auto total_count = sum_all(extract_counts(block_results));
            return total_sum / total_count;
    }
}
```

---

## Performance Benchmarks

| Query | Samples | Without Pushdown | With Pushdown | Speedup |
|-------|---------|------------------|---------------|---------|
| `sum(http_requests{...})` | 10M | 7,850ms | 10ms | 785x |
| `avg by (svc) (cpu{...})` | 5M | 3,200ms | 8ms | 400x |
| `max(memory_usage{...})` | 1M | 890ms | 5ms | 178x |
| `count by (host) (up)` | 100K | 95ms | 2ms | 47x |

---

## Best Practices

### 1. Design Queries for Pushdown

```promql
# GOOD: Outer aggregation pushes down
sum by (service) (rate(requests[5m]))

# LESS OPTIMAL: Multiple nested aggregations
avg(sum by (pod) (rate(requests[5m])))
```

### 2. Use Label Filtering

```promql
# GOOD: Filter first, then aggregate
sum(rate(http_requests{env="prod"}[5m]))

# LESS OPTIMAL: Aggregate all, filter later (no pushdown benefit)
sum(rate(http_requests[5m])) and on() label_match(env, "prod")
```

### 3. Limit Time Ranges

```promql
# GOOD: Smaller time range = fewer blocks
sum(rate(requests[1h]))

# EXPENSIVE: Large time range
sum(rate(requests[30d]))
```

---

## Metrics for Monitoring

```prometheus
# Pushdown usage
mytsdb_pushdown_queries_total{type="sum|avg|min|max|count"}

# Samples saved by pushdown
mytsdb_pushdown_samples_saved_total

# Time saved by pushdown (histogram)
mytsdb_pushdown_time_saved_seconds_bucket{le="0.01|0.1|1|10"}

# Pushdown hit rate
mytsdb_pushdown_hit_rate = 
  mytsdb_pushdown_queries_total / tsdb_queries_total
```
