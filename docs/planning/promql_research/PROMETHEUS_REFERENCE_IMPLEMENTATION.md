# Prometheus PromQL Reference Implementation Analysis

**Document**: `docs/planning/promql_research/PROMETHEUS_REFERENCE_IMPLEMENTATION.md`  
**Created**: November 2025  
**Purpose**: Analysis of the official Prometheus PromQL implementation for reference  
**Source**: [prometheus/prometheus GitHub repository](https://github.com/prometheus/prometheus)

## Executive Summary

This document analyzes the official Prometheus PromQL implementation to serve as the reference standard for achieving 100% PromQL compatibility in MyTSDB. The Prometheus implementation is written in Go and consists of a yacc-generated parser, a sophisticated query execution engine, and a comprehensive function library.

## Architecture Overview

### Core Components

```
promql/
├── parser/              # Parser and lexer implementation
│   ├── generated_parser.y    # Yacc grammar definition
│   ├── lex.go               # Lexer implementation
│   ├── ast.go               # AST node definitions
│   └── parse.go             # Parser wrapper
├── engine.go           # Query execution engine
├── functions.go        # Built-in function implementations
├── value.go            # Value types (Scalar, Vector, Matrix)
└── promql.go           # Public API
```

## 1. Parser Implementation

### Parser Architecture

**Technology**: Yacc-based parser generator
- **Grammar File**: `generated_parser.y`
- **Parser Type**: LALR(1) parser with yacc/bison
- **Language**: Go

### Grammar Structure

The Prometheus parser uses a formal yacc grammar with the following key elements:

#### Token Types
```yacc
// Literals
IDENTIFIER, NUMBER, STRING, DURATION

// Operators
ADD, SUB, MUL, DIV, MOD, POW, ATAN2
EQLC, NEQ, LTE, LSS, GTE, GTR
LAND, LOR, LUNLESS
EQL_REGEX, NEQ_REGEX

// Aggregators
AVG, BOTTOMK, COUNT, COUNT_VALUES, GROUP
MAX, MIN, QUANTILE, STDDEV, STDVAR
SUM, TOPK, LIMITK, LIMIT_RATIO

// Keywords
BOOL, BY, WITHOUT, ON, IGNORING
GROUP_LEFT, GROUP_RIGHT
OFFSET, AT, SMOOTHED, ANCHORED

// Delimiters
LEFT_PAREN, RIGHT_PAREN
LEFT_BRACKET, RIGHT_BRACKET
LEFT_BRACE, RIGHT_BRACE
```

#### Operator Precedence (Lowest to Highest)
```yacc
%left LOR                          // Logical OR
%left LAND LUNLESS                 // Logical AND, UNLESS
%left EQLC GTE GTR LSS LTE NEQ    // Comparison operators
%left ADD SUB                      // Addition, Subtraction
%left MUL DIV MOD ATAN2           // Multiplication, Division, Modulo
%right POW                         // Power (right-associative)
%nonassoc OFFSET                   // Offset modifier
%right LEFT_BRACKET                // Matrix/subquery selector
```

#### Expression Types
```yacc
expr:
    | aggregate_expr       // sum(metric) by (label)
    | binary_expr          // metric1 + metric2
    | function_call        // rate(metric[5m])
    | matrix_selector      // metric[5m]
    | number_duration_literal
    | offset_expr          // metric offset 5m
    | anchored_expr        // metric @ start()
    | smoothed_expr        // metric smoothed
    | paren_expr           // (expr)
    | string_literal       // "string"
    | subquery_expr        // metric[5m:1m]
    | unary_expr           // -metric
    | vector_selector      // metric{label="value"}
    | step_invariant_expr
    | duration_expr
```

### Key Parser Features

1. **Vector Matching Modifiers**
   - `on(labels)` / `ignoring(labels)`
   - `group_left(labels)` / `group_right(labels)`
   - `bool` modifier for comparison operators

2. **Time Modifiers**
   - `offset <duration>` - time offset
   - `@ <timestamp>` - evaluation at specific time
   - `@ start()`, `@ end()` - preprocessor functions

3. **Advanced Selectors**
   - Label matchers: `=`, `!=`, `=~`, `!~`
   - Matrix selectors: `[5m]`
   - Subqueries: `[5m:1m]` (range:resolution)

4. **Aggregation Modifiers**
   - `by (labels)` - group by specified labels
   - `without (labels)` - group by all except specified labels

## 2. Query Execution Engine

### Engine Architecture

The Prometheus engine (`engine.go`) implements a sophisticated query execution system:

#### Core Types

```go
// Query interface
type Query interface {
    Exec(ctx context.Context) *Result
    Close()
    Statement() parser.Statement
    Stats() *stats.Statistics
    Cancel()
    String() string
}

// Result types
type Result struct {
    Err      error
    Value    parser.Value  // Scalar, Vector, or Matrix
    Warnings annotations.Annotations
}

// Value types
type Scalar struct {
    T int64   // Timestamp
    V float64 // Value
}

type Sample struct {
    Metric labels.Labels
    T      int64
    F      float64                    // Float value
    H      *histogram.FloatHistogram  // Histogram value
}

type Vector []Sample

type Series struct {
    Metric     labels.Labels
    Floats     []FPoint
    Histograms []HPoint
}

type Matrix []Series
```

#### Engine Configuration

```go
type EngineOpts struct {
    Logger             log.Logger
    Reg                prometheus.Registerer
    MaxSamples         int           // Max samples per query
    Timeout            time.Duration // Query timeout
    ActiveQueryTracker QueryTracker
    LookbackDelta      time.Duration // Default: 5 minutes
    NoStepSubqueryIntervalFn func(rangeMillis int64) int64
    EnableAtModifier   bool
    EnableNegativeOffset bool
    EnablePerStepStats bool
}
```

### Query Execution Flow

1. **Parse Phase**
   - Lex and parse query string
   - Build AST
   - Validate syntax

2. **Preparation Phase**
   - Analyze AST
   - Determine required time ranges
   - Optimize query plan

3. **Evaluation Phase**
   - For instant queries: evaluate at single timestamp
   - For range queries: evaluate at each step
   - Fetch data from storage
   - Apply functions and operators
   - Aggregate results

4. **Result Phase**
   - Format results
   - Apply limits
   - Return Vector, Matrix, or Scalar

### Storage Interface

```go
type Queryable interface {
    Querier(ctx context.Context, mint, maxt int64) (Querier, error)
}

type Querier interface {
    Select(sortSeries bool, hints *SelectHints, matchers ...*labels.Matcher) SeriesSet
    LabelValues(name string, matchers ...*labels.Matcher) ([]string, Warnings, error)
    LabelNames(matchers ...*labels.Matcher) ([]string, Warnings, error)
    Close() error
}

type SeriesSet interface {
    Next() bool
    At() Series
    Err() error
    Warnings() Warnings
}
```

## 3. Function Library

### Complete Function Catalog

The Prometheus implementation includes **60+ built-in functions** organized into categories:

#### Aggregation Operators (13 functions)
- `sum()` - Sum over dimensions
- `avg()` - Average over dimensions
- `min()` - Minimum over dimensions
- `max()` - Maximum over dimensions
- `count()` - Count number of elements
- `stddev()` - Standard deviation over dimensions
- `stdvar()` - Standard variance over dimensions
- `group()` - Group all values
- `count_values()` - Count number of elements with same value
- `topk()` - Largest k elements
- `bottomk()` - Smallest k elements
- `quantile()` - φ-quantile (0 ≤ φ ≤ 1)
- `limitk()` - Limit sample count per series (experimental)
- `limit_ratio()` - Limit ratio of series (experimental)

#### Rate/Counter Functions (8 functions)
- `rate()` - Per-second average rate of increase
- `irate()` - Per-second instant rate of increase
- `increase()` - Total increase in time range
- `delta()` - Difference between first and last value
- `idelta()` - Difference between last two samples
- `deriv()` - Per-second derivative
- `predict_linear()` - Linear prediction
- `resets()` - Number of counter resets

#### Aggregation Over Time (11 functions)
- `avg_over_time()` - Average over time
- `min_over_time()` - Minimum over time
- `max_over_time()` - Maximum over time
- `sum_over_time()` - Sum over time
- `count_over_time()` - Count over time
- `quantile_over_time()` - Quantile over time
- `stddev_over_time()` - Standard deviation over time
- `stdvar_over_time()` - Standard variance over time
- `last_over_time()` - Last value over time
- `present_over_time()` - 1 if any samples exist
- `absent_over_time()` - 1 if no samples exist

#### Mathematical Functions (15 functions)
- `abs()` - Absolute value
- `ceil()` - Round up to nearest integer
- `floor()` - Round down to nearest integer
- `round()` - Round to nearest integer
- `sqrt()` - Square root
- `exp()` - Exponential function (e^x)
- `ln()` - Natural logarithm
- `log2()` - Binary logarithm
- `log10()` - Decimal logarithm
- `sin()` - Sine
- `cos()` - Cosine
- `tan()` - Tangent
- `asin()` - Arcsine
- `acos()` - Arccosine
- `atan()` - Arctangent

#### Trigonometric Functions
- `sinh()` - Hyperbolic sine
- `cosh()` - Hyperbolic cosine
- `tanh()` - Hyperbolic tangent
- `asinh()` - Inverse hyperbolic sine
- `acosh()` - Inverse hyperbolic cosine
- `atanh()` - Inverse hyperbolic tangent
- `deg()` - Radians to degrees
- `rad()` - Degrees to radians
- `sgn()` - Sign of value (-1, 0, or 1)
- `pi()` - Pi constant

#### Date/Time Functions (8 functions)
- `time()` - Current Unix timestamp
- `timestamp()` - Timestamp of sample
- `year()` - Year from timestamp
- `month()` - Month from timestamp
- `day_of_month()` - Day of month
- `day_of_week()` - Day of week (0=Sunday)
- `hour()` - Hour from timestamp
- `minute()` - Minute from timestamp
- `days_in_month()` - Number of days in month

#### Label Manipulation (2 functions)
- `label_replace()` - Replace label values using regex
- `label_join()` - Join multiple label values

#### Histogram Functions (3 functions)
- `histogram_quantile()` - Calculate quantile from histogram
- `histogram_count()` - Count from histogram
- `histogram_sum()` - Sum from histogram
- `histogram_fraction()` - Fraction of observations
- `histogram_avg()` - Average from histogram
- `histogram_stddev()` - Standard deviation from histogram
- `histogram_stdvar()` - Standard variance from histogram

#### Utility Functions (10+ functions)
- `vector()` - Convert scalar to vector
- `scalar()` - Convert single-element vector to scalar
- `sort()` - Sort by value ascending
- `sort_desc()` - Sort by value descending
- `sort_by_label()` - Sort by label values
- `sort_by_label_desc()` - Sort by label values descending
- `clamp()` - Clamp value to range
- `clamp_max()` - Clamp to maximum
- `clamp_min()` - Clamp to minimum
- `absent()` - 1 if vector is empty
- `changes()` - Number of value changes
- `holt_winters()` - Holt-Winters smoothing

#### Special Functions
- `histogram_quantile()` - Quantile from histogram buckets
- `histogram_count()` - Total count from histogram
- `histogram_sum()` - Total sum from histogram

### Function Implementation Pattern

All functions follow a consistent implementation pattern:

```go
type FunctionCall func(
    vectorVals []Vector,
    matrixVals Matrix,
    args parser.Expressions,
    enh *EvalNodeHelper,
) (Vector, annotations.Annotations)

// Example: rate() function
func funcRate(vals Matrix, args parser.Expressions, enh *EvalNodeHelper) (Vector, annotations.Annotations) {
    return extrapolatedRate(vals, args, enh, true, true)
}
```

## 4. Key Implementation Details

### Lookback Delta

Default: 5 minutes
- Used for instant queries to find samples
- Configurable per engine instance
- Critical for handling sparse data

### Sample Limits

- `MaxSamples`: Maximum samples loaded into memory
- Prevents OOM on large queries
- Default: configurable per deployment

### Counter Reset Handling

Automatic detection and correction for:
- Counter resets in `rate()`, `irate()`, `increase()`
- Monotonic counter assumptions
- Proper extrapolation at boundaries

### Vector Matching

Three cardinality types:
1. **One-to-One**: Default matching
2. **Many-to-One**: `group_left()`
3. **One-to-Many**: `group_right()`

### Subquery Support

Format: `metric[range:resolution]`
- `range`: Time range to query
- `resolution`: Step size for evaluation
- Example: `rate(metric[5m:1m])` - evaluate rate every 1m over 5m window

## 5. Testing Strategy

### Prometheus Test Suite

The Prometheus repository includes comprehensive tests:

1. **Parser Tests**
   - `parser/parse_test.go` - Grammar and syntax tests
   - Edge cases and error conditions
   - All expression types

2. **Engine Tests**
   - `promql_test.go` - Query evaluation tests
   - Correctness tests with expected results
   - Performance benchmarks

3. **Function Tests**
   - Individual function correctness
   - Edge cases (NaN, Inf, empty vectors)
   - Histogram handling

4. **Integration Tests**
   - End-to-end query execution
   - Storage integration
   - Concurrent query handling

## 6. API Compatibility

### HTTP API Endpoints

```
POST /api/v1/query
  - Instant query evaluation
  - Parameters: query, time, timeout

POST /api/v1/query_range
  - Range query evaluation
  - Parameters: query, start, end, step, timeout

GET /api/v1/labels
  - Get all label names

GET /api/v1/label/<label_name>/values
  - Get all values for a label

POST /api/v1/series
  - Find series by label matchers
```

### Response Format

```json
{
  "status": "success",
  "data": {
    "resultType": "vector|matrix|scalar|string",
    "result": [...]
  },
  "warnings": [...],
  "errorType": "...",
  "error": "..."
}
```

## 7. Performance Optimizations

### Query Optimization

1. **Lazy Evaluation**: Only fetch needed data
2. **Parallel Evaluation**: Independent subexpressions
3. **Result Caching**: Cache parsed AST for range queries
4. **Memory Pooling**: Reuse sample slices

### Storage Optimizations

1. **Efficient Label Matching**: Use inverted index
2. **Block-level Filtering**: Skip irrelevant blocks
3. **Chunk Encoding**: Compressed time series data
4. **Batch Fetching**: Minimize storage round-trips

## 8. Compatibility Requirements for MyTSDB

To achieve 100% PromQL compatibility, MyTSDB must:

### Parser Compatibility
- ✅ Support complete PromQL grammar
- ✅ Identical operator precedence
- ✅ All modifiers (offset, @, bool, etc.)
- ✅ Subquery syntax

### Function Compatibility
- ✅ All 60+ functions implemented
- ✅ Identical semantics and edge case handling
- ✅ Histogram support
- ✅ Counter reset handling

### API Compatibility
- ✅ HTTP endpoints match Prometheus
- ✅ Request/response formats identical
- ✅ Error messages compatible
- ✅ Warning annotations

### Behavioral Compatibility
- ✅ Lookback delta handling
- ✅ Sample limit enforcement
- ✅ Vector matching semantics
- ✅ Timestamp handling (millisecond precision)

## 9. References

- **Repository**: https://github.com/prometheus/prometheus
- **Parser**: `promql/parser/generated_parser.y`
- **Engine**: `promql/engine.go`
- **Functions**: `promql/functions.go`
- **Documentation**: https://prometheus.io/docs/prometheus/latest/querying/basics/

---

**Document Status**: Complete  
**Last Updated**: November 2025  
**Next Steps**: Use this as reference for gap analysis and implementation
