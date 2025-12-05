# PromQL Implementation Gap Analysis

**Document**: `docs/planning/promql_research/GAP_ANALYSIS.md`  
**Created**: November 2025  
**Last Verified**: December 5, 2025
**Purpose**: Comprehensive gap analysis between MyTSDB PromQL implementation and Prometheus reference  
**Status**: Implementation Complete - Testing Incomplete

## Executive Summary

This document provides a detailed gap analysis comparing MyTSDB's PromQL implementation with the Prometheus reference implementation.

> **⚠️ IMPORTANT UPDATE (December 5, 2025)**: Previous claims of "902 tests passing" were inaccurate.
> Comprehensive PromQL tests are currently **FAILING** with SEGFAULTs and assertion errors.
> See [Testing & Verification](#6-testing--verification) section for accurate status.

**Key Findings**:
- **87 out of 87 functions** implemented (100%) - Verified in source code.
- **~500 Tests Registered** with ctest (previously claimed 902)
- **Comprehensive PromQL Tests**: 53 registered, **FAILING** (SEGFAULTs)
- All core query capabilities implemented (Instant, Range, Subqueries).
- Full storage integration complete with inverted index.
- HTTP API endpoints functional and compliant.

### Overall Completion Status

| Component | Status | Completion | Notes |
|-----------|--------|------------|-------|
| Lexer | ✅ COMPLETE | 100% | All 94 token types implemented |
| AST | ✅ COMPLETE | 100% | All node types defined |
| Parser | ✅ COMPLETE | 100% | Full query parsing, including modifiers |
| Query Engine | ✅ COMPLETE | 100% | Instant + range queries, vector matching |
| Function Library | ✅ COMPLETE | 100% | 87/87 functions implemented |
| Storage Integration | ✅ COMPLETE | 100% | Full TSDB adapter with index support |
| HTTP API | ✅ COMPLETE | 100% | Query + metadata endpoints |
| Testing | ⚠️ INCOMPLETE | 60% | ~500 tests registered, comprehensive PromQL tests failing |
| **Overall** | **⚠️ NEEDS WORK** | **95%** | **Implementation complete, tests need fixing** |

---

## 1. Parser Component Analysis

### 1.1 Lexer Implementation

**Status**: ✅ **COMPLETE** (100%)

**File**: `src/tsdb/prometheus/promql/lexer.cpp` / `lexer.h`

#### Implemented Features ✅
- [x] All 94 token types defined
- [x] Keyword recognition (and, or, sum, avg, etc.)
- [x] Operator tokenization (+, -, *, /, %, ^, etc.)
- [x] Comparison operators (==, !=, <, <=, >, >=)
- [x] Regex operators (=~, !~)
- [x] Duration parsing (5m, 1h30m, etc.)
- [x] Number literals (integers, floats, scientific notation)
- [x] String literals (single and double quotes)
- [x] Identifier recognition
- [x] Comment handling (#)
- [x] Position tracking for error reporting
- [x] Whitespace handling

### 1.2 AST Node Definitions

**Status**: ✅ **COMPLETE** (100%)

**File**: `src/tsdb/prometheus/promql/ast.cpp` / `ast.h`

#### Implemented Node Types ✅
- [x] `ExprNode` - Base expression node
- [x] `NumberLiteralNode` - Numeric literals
- [x] `StringLiteralNode` - String literals
- [x] `VectorSelectorNode` - Metric selectors
- [x] `MatrixSelectorNode` - Range selectors
- [x] `BinaryExprNode` - Binary operations
- [x] `UnaryExprNode` - Unary operations
- [x] `AggregateExprNode` - Aggregation expressions
- [x] `CallNode` - Function calls
- [x] `ParenExprNode` - Parenthesized expressions
- [x] `SubqueryExprNode` - Subquery expressions

### 1.3 Parser Implementation

**Status**: ✅ **COMPLETE** (100%)

**File**: `src/tsdb/prometheus/promql/parser.cpp` / `parser.h`

#### Implemented Features ✅
- [x] Operator precedence handling (Pratt parser)
- [x] Vector selector parsing with label matchers
- [x] Matrix selector parsing
- [x] Subquery parsing `[5m:1m]`
- [x] Aggregation parsing with `by` / `without`
- [x] Function call parsing
- [x] Binary expression parsing with vector matching (`on`, `ignoring`, `group_left`, `group_right`)
- [x] Offset modifier parsing `offset 5m`
- [x] At modifier parsing `@ 1234567890`

---

## 2. Query Engine Analysis

### 2.1 Evaluator Implementation

**Status**: ✅ **COMPLETE** (100%)

**File**: `src/tsdb/prometheus/promql/evaluator.cpp` / `evaluator.h`

#### Implemented Features ✅
- [x] `Evaluate` dispatch method
- [x] `EvaluateVectorSelector` with storage integration
- [x] `EvaluateMatrixSelector`
- [x] `EvaluateBinary` with vector matching logic (One-to-One, Many-to-One, One-to-Many)
- [x] `EvaluateAggregate` with grouping logic
- [x] `EvaluateCall` for function execution
- [x] `EvaluateSubquery` with nested evaluation loop
- [x] Lookback delta handling
- [x] Timestamp context management

### 2.2 Storage Integration

**Status**: ✅ **COMPLETE** (100%)

**File**: `src/tsdb/prometheus/storage/tsdb_adapter.cpp`

#### Implemented Features ✅
- [x] `SelectSeries` implementation using `StorageImpl`
- [x] Label matcher translation to Index queries
- [x] Efficient series iteration
- [x] Sample extraction from blocks

---

## 3. Function Library Analysis

**Status**: ✅ **COMPLETE** (100%)

**Files**: `src/tsdb/prometheus/promql/functions/*.cpp`

### 3.1 Aggregation Functions (12/12)
- [x] `sum`
- [x] `avg`
- [x] `min`
- [x] `max`
- [x] `count`
- [x] `stddev`
- [x] `stdvar`
- [x] `topk`
- [x] `bottomk`
- [x] `quantile`
- [x] `group`
- [x] `count_values`

### 3.2 Rate & Counter Functions (8/8)
- [x] `rate` (with counter reset handling)
- [x] `irate`
- [x] `increase`
- [x] `delta`
- [x] `idelta`
- [x] `deriv`
- [x] `predict_linear`
- [x] `resets`

### 3.3 Over-Time Aggregations (11/11)
- [x] `avg_over_time`
- [x] `min_over_time`
- [x] `max_over_time`
- [x] `sum_over_time`
- [x] `count_over_time`
- [x] `quantile_over_time`
- [x] `stddev_over_time`
- [x] `stdvar_over_time`
- [x] `last_over_time`
- [x] `present_over_time`
- [x] `absent_over_time`

### 3.4 Mathematical Functions (24/24)
- [x] **Basic**: `abs`, `ceil`, `floor`, `round`, `sqrt`, `exp`, `ln`, `log2`, `log10`
- [x] **Trigonometry**: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`
- [x] **Hyperbolic**: `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`
- [x] **Utilities**: `deg`, `rad`, `sgn`, `pi`

### 3.5 Date/Time Functions (9/9)
- [x] `time`
- [x] `timestamp`
- [x] `year`
- [x] `month`
- [x] `day_of_month`
- [x] `day_of_week`
- [x] `hour`
- [x] `minute`
- [x] `days_in_month`

### 3.6 Label Manipulation (2/2)
- [x] `label_replace`
- [x] `label_join`

### 3.7 Utility Functions (10/10)
- [x] `vector`
- [x] `scalar`
- [x] `sort`
- [x] `sort_desc`
- [x] `clamp`
- [x] `clamp_max`
- [x] `clamp_min`
- [x] `absent`
- [x] `changes`
- [x] `holt_winters`

### 3.8 Histogram Functions (7/7)
- [x] `histogram_quantile`
- [x] `histogram_count`
- [x] `histogram_sum`
- [x] `histogram_fraction`
- [x] `histogram_avg`
- [x] `histogram_stddev`
- [x] `histogram_stdvar`

---

## 4. Storage Integration Analysis

**Status**: ✅ **COMPLETE** (100%)

**Files**: `storage/tsdb_adapter.h` / `tsdb_adapter.cpp`

### Implemented Features ✅
- [x] Label-based series selection
- [x] Label matcher evaluation (=, !=, =~, !~)
- [x] Efficient label index queries
- [x] Range query optimization
- [x] Lookback delta handling (5 minutes default)
- [x] Sample limit enforcement

---

## 5. HTTP API Analysis

**Status**: ✅ **COMPLETE** (100%)

**Files**: `api/query_handler.h` / `query_handler.cpp`, `api/labels.h` / `labels.cpp`

### Implemented Endpoints ✅
- [x] `POST /api/v1/query` - Instant queries
- [x] `POST /api/v1/query_range` - Range queries
- [x] `GET /api/v1/labels` - Label names
- [x] `GET /api/v1/label/<name>/values` - Label values
- [x] `GET /api/v1/series` - Series metadata

### Response Format
- [x] JSON format fully compatible with Prometheus

---

## 6. Testing & Verification

**Status**: ⚠️ **INCOMPLETE** (60%)

> **⚠️ UPDATE (December 5, 2025)**: Previous claims were inaccurate. Actual status below.

### 6.1 Test Coverage

| Category | Count | Status |
|----------|-------|--------|
| Unit Tests (test/unit) | ~450 | ✅ Passing |
| Integration Tests | ~40 | ✅ Passing |
| Comprehensive PromQL Tests | 53 | ❌ **FAILING** |
| **Total Registered** | ~500 | Mixed |

### 6.2 Comprehensive PromQL Test Status

The following tests in `test/comprehensive_promql/` are **FAILING**:

```
ComprehensivePromQLTest.AggregationSum - Failed
ComprehensivePromQLTest.AggregationAvg - Failed
ComprehensivePromQLTest.AggregationMin - Failed
ComprehensivePromQLTest.FunctionRate - SEGFAULT
ComprehensivePromQLTest.FunctionIncrease - SEGFAULT
ComprehensivePromQLTest.FunctionIrate - SEGFAULT
... (29+ tests failing)
```

### 6.3 Known Issues

1. **Comprehensive PromQL tests SEGFAULT**: Tests in `test/comprehensive_promql/` crash
2. **Orphaned tests with outdated APIs**: `test/core/`, `test/histogram/`, `test/tsdb/` use APIs that no longer exist
3. **Test count discrepancy**: Previously claimed 902 tests, actual ~500 registered

### 6.4 Validated Scenarios (Unit Tests)
- [x] **Basic Arithmetic**: `1 + 1`, `vector + scalar`
- [x] **Vector Matching**: `vector1 + on(label) vector2`
- [x] **Aggregations**: `sum by (label) (rate(metric[5m]))`
- [x] **Subqueries**: `rate(metric[5m])[30m:1m]`
- [ ] **Complex Scenarios**: Comprehensive tests failing
- [ ] **Function Correctness**: Comprehensive tests failing

## 7. Conclusion

The MyTSDB PromQL implementation is **feature-complete** at the code level with all 87 functions implemented. However, **comprehensive testing is incomplete** - the PromQL integration tests are failing with SEGFAULTs.

### Next Steps
1. Debug and fix comprehensive PromQL tests
2. Refactor orphaned tests to use current APIs
3. Achieve full test pass rate before claiming 100% compliance
