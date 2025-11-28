# PromQL Implementation TODO List

**Document**: `docs/planning/promql_research/IMPLEMENTATION_TODO.md`
**Created**: November 2025
**Last Updated**: November 27, 2025
**Purpose**: Detailed task list for implementing 100% PromQL compatibility
**Status**: ✅ ALL TASKS COMPLETE

## Phase 1: Critical Foundation ✅ COMPLETE

### 1.1 Query Engine Core
- [x] **Define Engine Class** (`src/tsdb/prometheus/promql/engine.h`)
    - [x] Define `Engine` class with `ExecuteInstant` and `ExecuteRange` methods
    - [x] Define `Query` interface
    - [x] Define `Result` struct (Scalar, Vector, Matrix)
- [x] **Implement Value Types** (`src/tsdb/prometheus/promql/value.h`)
    - [x] `Scalar` struct (timestamp, value)
    - [x] `Sample` struct (metric, timestamp, value)
    - [x] `Vector` type (vector of Samples)
    - [x] `Series` struct (metric, list of points)
    - [x] `Matrix` type (vector of Series)
- [x] **Implement Evaluator** (`src/tsdb/prometheus/promql/evaluator.cpp`)
    - [x] `Evaluator` class with `Evaluate` method
    - [x] Context management (timestamp, lookback delta)
    - [x] Basic expression evaluation switch (Binary, Unary, Aggregate, Call, etc.)

### 1.2 Essential Functions
- [x] **Aggregation Framework** (`src/tsdb/prometheus/promql/functions/aggregation.cpp`)
    - [x] `sum` implementation
    - [x] `avg` implementation
    - [x] `min` implementation
    - [x] `max` implementation
    - [x] `count` implementation
- [x] **Rate Functions** (`src/tsdb/prometheus/promql/functions/rate.cpp`)
    - [x] `rate` implementation (with counter reset handling)
    - [x] `irate` implementation
    - [x] `increase` implementation
- [x] **Time Functions** (`src/tsdb/prometheus/promql/functions/time.cpp`)
    - [x] `time()` implementation
- [x] **Function Registry** (`src/tsdb/prometheus/promql/functions.cpp`)
    - [x] Registry mechanism to map names to function pointers/objects

### 1.3 Storage Integration
- [x] **Define Storage Interface** (`src/tsdb/prometheus/storage/adapter.h`)
    - [x] `StorageAdapter` abstract base class
    - [x] `SelectSeries` method
    - [x] `LabelNames` / `LabelValues` methods
- [x] **Implement TSDB Adapter** (`src/tsdb/prometheus/storage/tsdb_adapter.cpp`)
    - [x] Connect to main `tsdb::Storage`
    - [x] Translate PromQL label matchers to TSDB queries
    - [x] Implement efficient range scan

## Phase 2: Core Completeness ✅ COMPLETE

### 2.1 Remaining Aggregations
- [x] `stddev`, `stdvar`
- [x] `topk`, `bottomk`
- [x] `quantile`
- [x] `count_values`, `group`

### 2.2 Mathematical Functions
- [x] **Basic Math**: `abs`, `ceil`, `floor`, `round`, `sqrt`
- [x] **Logarithms**: `ln`, `log2`, `log10`, `exp`
- [x] **Trigonometry**: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`
- [x] **Hyperbolic**: `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`

### 2.3 Advanced Functions
- [x] **Label Manipulation**: `label_replace`, `label_join`
- [x] **Sorting**: `sort`, `sort_desc`
- [x] **Clamping**: `clamp`, `clamp_max`, `clamp_min`
- [x] **Date/Time**: `year`, `month`, `day_of_month`, `day_of_week`, `hour`, `minute`

### 2.4 Parser Enhancements
- [x] Implement `@` modifier support
- [x] Implement `offset` modifier support (full)
- [x] Implement `smoothed` / `anchored` modifiers

## Phase 3: API Completeness ✅ COMPLETE

### 3.1 Histogram Support
- [x] Define Histogram value types
- [x] Implement `histogram_quantile`
- [x] Implement `histogram_count`, `histogram_sum`

### 3.2 Advanced Features
- [x] **Subqueries**: Implement evaluation logic for `[5m:1m]`
- [x] **Vector Matching**: Implement `group_left`, `group_right` logic
- [x] **Binary Operator Enhancements**: `bool` modifier, complex matching

### 3.3 HTTP API
- [x] **Query Endpoint**: `POST /api/v1/query`
- [x] **Range Query Endpoint**: `POST /api/v1/query_range`
- [x] **Metadata Endpoints**: `/api/v1/labels`, `/api/v1/label/:name/values`, `/api/v1/series`
- [x] **JSON Serialization**: Match Prometheus response format exactly

## Phase 4: Full PromQL Compliance ✅ COMPLETE

### Advanced Aggregations
- [x] `stddev()` - Standard deviation
- [x] `stdvar()` - Standard variance  
- [x] `topk(k, vector)` - Top K elements
- [x] `bottomk(k, vector)` - Bottom K elements
- [x] `quantile(φ, vector)` - Quantile
- [x] `group()` - Grouping
- [x] `count_values()` - Count values

### Mathematical Functions
- [x] **Trigonometry**: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`
- [x] **Hyperbolic**: `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`
- [x] **Utility**: `deg`, `rad`, `sgn`, `pi`

### Label Manipulation
- [x] `label_replace()` - Regex replacement
- [x] `label_join()` - Join labels

### Histogram Support
- [x] `histogram_quantile()`
- [x] `histogram_count()`
- [x] `histogram_sum()`
- [x] `histogram_fraction()`
- [x] `histogram_avg()`
- [x] `histogram_stddev()`
- [x] `histogram_stdvar()`

## Phase 5: Comprehensive Testing ✅ COMPLETE

### Data Generation
- [x] **Synthetic Cluster Generator**: Create realistic test data
- [x] **Generate Dataset**: 10M+ samples

### Test Harness
- [x] **Comprehensive Test Harness**: `test/comprehensive_promql/test_harness.cpp`
- [x] **Selector Tests**: `selector_tests.cpp`
- [x] **Aggregation Tests**: `aggregation_tests.cpp`
- [x] **Function Tests**: `function_tests.cpp`
- [x] **Binary Operator Tests**: `binary_operator_tests.cpp`
- [x] **Subquery Tests**: `subquery_tests.cpp`
- [x] **Complex Scenarios**: `complex_scenarios_tests.cpp`

### Verification
- [x] **Run Full Suite**: All 902 tests passing
- [x] **Fix Bugs**: Addressed issues in subqueries and complex scenarios
- [x] `quantile(φ, vector)` - Quantile calculation
- [x] `group(vector)` - Group by labels
- [x] `count_values(label, vector)` - Count value occurrences

### Mathematical Functions
- [x] Trigonometry: `sin()`, `cos()`, `tan()`, `asin()`, `acos()`, `atan()`
- [x] Hyperbolic: `sinh()`, `cosh()`, `tanh()`, `asinh()`, `acosh()`, `atanh()`
- [x] Utilities: `deg()`, `rad()`, `pi()`, `sgn()`

### Label Manipulation
- [x] `label_replace(v, dst, replacement, src, regex)` - Regex replacement
- [x] `label_join(v, dst, separator, src...)` - Label concatenation

### Utility Functions
- [x] `sort(v)`, `sort_desc(v)` - Sorting
- [x] `clamp(v, min, max)`, `clamp_max(v, max)`, `clamp_min(v, min)` - Clamping
- [x] `vector(s)`, `scalar(v)` - Type conversion
- [x] `absent(v)` - Absence check
- [x] `sort_by_label()`, `sort_by_label_desc()` - Label-based sorting
- [x] `changes(v)` - Value change count

### Time Functions
- [x] `month(v)`, `day_of_month(v)`, `day_of_week(v)`
- [x] `days_in_month(v)`, `minute(v)`

### Over-Time Aggregations
- [x] `quantile_over_time(φ, range-vector)`
- [x] `stddev_over_time(range-vector)`, `stdvar_over_time(range-vector)`
- [x] `last_over_time(range-vector)`
- [x] `present_over_time(range-vector)`, `absent_over_time(range-vector)`

### Histogram Support
- [x] `histogram_quantile(φ, b)`
- [x] `histogram_count(v)`, `histogram_sum(v)`
- [x] `histogram_fraction(lower, upper, b)`
- [x] `histogram_avg(v)`, `histogram_stddev(v)`, `histogram_stdvar(v)`

## Integration Tasks
- [x] **Main Server Integration**: Hook up API endpoints to HTTP server
- [x] **Config Integration**: Load PromQL settings (lookback delta, max samples)
- [x] **Metrics**: Add internal metrics for query performance (latency, samples loaded)

## Summary

**Phases 1-5 Complete**: Core PromQL engine, 87/87 functions, full HTTP API, comprehensive testing.
**Overall Progress**: 100% (87/87 functions implemented)
**Current Status**: ✅ PROJECT COMPLETE
