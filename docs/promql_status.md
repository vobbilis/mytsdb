# PromQL Implementation Status

**Last Updated:** November 2025

## Overview
The PromQL engine implementation is currently in **Phase 4 (Full Compliance)**. We have successfully implemented the core evaluation engine, storage integration, aggregation operators, and a comprehensive set of functions.

## ✅ Implemented Features

### 1. Core Engine
- **Parsing:** Full support for PromQL syntax including vector selectors, matrix selectors, function calls, aggregations, and subqueries.
- **Evaluation:** robust `Evaluator` and `Engine` classes with context management.
- **Storage Integration:** Seamless integration with TSDB storage via `StorageAdapter`.

### 2. Aggregation Operators
All standard PromQL aggregation operators are supported:
- `sum`, `avg`, `min`, `max`, `count`
- `stddev`, `stdvar`
- `topk`, `bottomk`
- `quantile`
- `count_values`
- **Grouping:** `by (...)` and `without (...)` modifiers.

### 3. Functions
A wide range of PromQL functions are implemented:
- **Rate Functions:** `rate`, `increase`, `irate` (with counter reset handling).
- **Math Functions:** `abs`, `ceil`, `floor`, `round`, `sqrt`, `exp`, `ln`, `log2`, `log10`.
- **Time Functions:** `time`, `year`, `day_of_month`, `day_of_week`, `days_in_month`, `hour`, `minute`.
- **Over-Time Aggregations:** `avg_over_time`, `sum_over_time`, `min_over_time`, `max_over_time`, `count_over_time`, `quantile_over_time`, `stddev_over_time`, `stdvar_over_time`, `last_over_time`, `present_over_time`, `absent_over_time`.
- **Label Manipulation:** `label_join`, `label_replace`.
- **Utility:** `vector`, `scalar`, `clamp`, `clamp_min`, `clamp_max`.
- **Sorting:** `sort`, `sort_desc`.
- **Other:** `changes`, `resets`, `deriv`, `predict_linear`, `delta`, `idelta`.

### 4. Selectors
- **Vector Selectors:** Support for all matchers (`=`, `!=`, `=~`, `!~`).
- **Matrix Selectors:** Support for time ranges and lookback deltas.

## 🚧 In Progress / Planned

### 1. Binary Operators
- **Arithmetic:** `+`, `-`, `*`, `/`, `%`, `^`.
- **Comparison:** `==`, `!=`, `<`, `<=`, `>`, `>=` (with `bool` modifier).
- **Logical:** `and`, `or`, `unless`.
- **Vector Matching:** `on`, `ignoring`, `group_left`, `group_right`.

### 2. Subqueries
- Evaluation logic for subqueries (Parsing is complete).

### 3. HTTP API
- Full compliance with Prometheus HTTP API endpoints (`/api/v1/query`, `/api/v1/query_range`).

## Verification
The implementation is verified by a comprehensive test suite:
- `test/comprehensive_promql/selector_tests.cpp`
- `test/comprehensive_promql/aggregation_tests.cpp`
- `test/comprehensive_promql/function_tests.cpp`
- `test/comprehensive_promql/binary_operator_tests.cpp` (Upcoming)

## Usage Example
```cpp
#include "tsdb/prometheus/promql/engine.h"

// Setup engine
tsdb::prometheus::promql::EngineOptions options;
options.storage_adapter = my_storage_adapter;
tsdb::prometheus::promql::Engine engine(options);

// Execute query
auto result = engine.ExecuteInstant("rate(http_requests_total[5m])", timestamp);

if (!result.hasError()) {
    // Process result.value
}
```
