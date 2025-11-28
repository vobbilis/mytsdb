# PromQL Engine Agent Task List

**Document**: `docs/planning/PROMQL_ENGINE_AGENT_TASKS.md`  
**Created**: September 2025  
**Agent**: PromQL Engine Agent (Agent 3)  
**Status**: Ready to Start  
**Priority**: HIGH  
**Target**: 100% PromQL Compatibility  
**Design Analysis**: See `docs/analysis/PROMQL_DESIGN_ANALYSIS.md`

## Executive Summary

This document provides concrete, actionable tasks for Agent 3 to complete the PromQL (Prometheus Query Language) engine implementation. The agent will transform the existing 25% foundation (lexer, AST, parser stubs) into a production-ready engine with 100% PromQL compatibility.

## Current Status

### ✅ **Foundation Complete (60%)**
- **Lexer**: Complete with 94 token types (`src/tsdb/prometheus/promql/lexer.h/.cpp`)
- **AST**: Complete node hierarchy (`src/tsdb/prometheus/promql/ast.h/.cpp`)
- **Parser**: Core parsing logic complete (`src/tsdb/prometheus/promql/parser.h/.cpp`)
- **Query Engine**: Basic evaluation engine implemented (`src/tsdb/prometheus/promql/engine.h/.cpp`)
- **Functions**: Rate and aggregation functions implemented (`src/tsdb/prometheus/promql/functions/`)

### 🔴 **Missing Components (40%)**
- **Vector Matching**: Complex binary operations (one-to-many, many-to-one) - **IN PROGRESS**
- **Advanced Functions**: Math, time, and statistical functions - **COMPLETED**
- **Storage Integration**: Full integration with TSDB indices - **COMPLETED**
- **HTTP API**: Full compliance with Prometheus API endpoints

## 🎯 **Key Technical Guidance**

### **Prometheus Reference Implementation**
- **Primary Reference**: Study Prometheus's `promql/parser/lex.go` and `promql/parser/parse.go` for token definitions and grammar rules
- **AST Alignment**: Ensure AST nodes align with Prometheus's `promql/parser/ast.go` 
- **Function Reference**: Use Prometheus's `promql/parser/functions.go` for function signature validation
- **Engine Behavior**: **Pay close attention to evaluation timestamps, step resolution, and alignment as in Prometheus's `Engine`**

### **Data Types and Integration**
- **Core Types**: Use existing core TSDB types (`core::Label`, `core::Sample`, `core::TimeSeries`, `core::Result`, `core::Error`)
- **Model Integration**: Leverage existing `model::LabelMatcher` from `model/types.h` (already added to support AST)
- **Result Types**: **Define value types for PromQL results as per Prometheus: `scalar`, `string`, `vector`, `matrix`**
- **Storage Interface**: The PromQL engine will depend on `Storage` interface defined in `include/tsdb/storage/storage.h`

### **Critical Implementation Details**
- **Lookback Deltas**: **Explicitly plan for lookback deltas in range vector selectors and `_over_time` aggregations**
- **Vector Matching**: **Ensure correct handling of one-to-one, many-to-one, and one-to-many matching semantics, including cardinality checks**
- **Counter Functions**: **Pay attention to the specific evaluation logic for each function, e.g., how `rate` and `increase` handle counter resets and extrapolation**
- **Duration Handling**: **Consider how Prometheus's parser handles durations, offsets, and `@` modifiers during parsing**

### **Performance and Integration Considerations**
- **Storage Enhancement**: Consider if enhanced `Storage` interface needed:
  ```cpp
  core::Result<std::vector<model::Series>> SelectSeries(
      const std::vector<core::LabelMatcher>& matchers, 
      core::Timestamp mint, core::Timestamp maxt, 
      core::Duration lookback_delta_hint
  )
  ```
- **Configuration**: **Refer to Prometheus's `promql.EngineOpts` for common parameters**: `LookbackDelta`, `QueryTimeout`, `MaxSamplesPerQuery`, `EnableAtModifier`, `EnableNegativeOffset`
- **Error Handling**: Use `core::Result` pattern consistently for error propagation throughout the pipeline

### **Result Structure and API Integration**
- **Engine Result**: Return well-defined result structure:
  ```cpp
  struct PromQLResult { 
      promql::ValueType type; 
      promql::Value value; // std::variant of model::Scalar, model::String, model::Vector, model::Matrix
  }
  ```
- **HTTP Integration**: The `QueryHandler` (`src/tsdb/prometheus/api/query.cpp`) will be the primary consumer of the `PromQLEngine`
- **Response Format**: Serialize results into the JSON format specified by the Prometheus HTTP API

## Phase-Based Implementation (12 Weeks)

### **PHASE 1: Parser Completion (Weeks 1-2)**

#### **WEEK 1: Core Expression Parsing**

**TASK 1.1: Update Planning Documentation**
**File**: `docs/planning/PROMQL_ENGINE_AGENT_TASKS.md`  
**Estimated Time**: 15 minutes  
**Status**: ✅ **COMPLETED**

**Actions**:
- [ ] Update Phase 1 status from "Not Started" to "In Progress"
- [ ] Add agent start date and current task
- [ ] Commit changes with message: "Start Phase 1: Parser Completion"

**Completion Criteria**:
- Planning document reflects current status

---

**TASK 1.2: Complete Vector Selector Parsing**
**File**: `src/tsdb/prometheus/promql/parser.cpp`  
**Method**: `parseVectorSelector()`  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Design Guidance**:
- **Reference**: Study Prometheus's `promql/parser/parse.go` for vector selector parsing logic
- **Label Matchers**: Implement all operators (`=`, `!=`, `=~`, `!~`) as defined in Prometheus's `promql/parser/lex.go`
- **Validation**: Ensure label names and values follow Prometheus restrictions (alphanumeric + underscore, no leading digits for names)
- **Error Handling**: Include position tracking (line, column) for better error reporting as in original lexer design

**Actions**:
- [ ] Implement complete `parseVectorSelector()` method following Prometheus reference implementation
- [ ] Handle metric name parsing (`metric_name`) with proper validation
- [ ] Handle label matchers with all operators (`=`, `!=`, `=~`, `!~`) exactly as defined in `promql/parser/lex.go`
- [ ] Support offset modifier (`offset 5m`) - **Consider how Prometheus's parser handles durations, offsets during parsing**
- [ ] Support @ modifier (`@ 1609459200`) - implement as per Prometheus's `@` modifier specification
- [ ] Add comprehensive error handling with positional information
- [ ] Validate label matcher syntax and regex patterns

**Technical Details**:
- Use existing `model::LabelMatcher` from `model/types.h` (already added to support AST)
- Ensure alignment with AST nodes defined to match Prometheus's `promql/parser/ast.go`
- Handle edge cases: empty metric names, invalid regex patterns, malformed labels

**Completion Criteria**:
- Parses: `metric_name{label="value", other=~"regex"} offset 5m @ 123`
- All label matching operators work correctly per Prometheus specification
- Error messages are clear and include position information
- Label validation matches Prometheus behavior
- Unit tests pass for vector selectors and cover edge cases

---

**TASK 1.3: Complete Matrix Selector Parsing**
**File**: `src/tsdb/prometheus/promql/parser.cpp`  
**Method**: `parseMatrixSelector()`  
**Estimated Time**: 6 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement complete `parseMatrixSelector()` method
- [ ] Handle range duration parsing (`[5m]`, `[1h30m]`, `[2d]`)
- [ ] Support complex durations (`[1h30m45s]`)
- [ ] Validate duration formats and ranges
- [ ] Add offset and @ modifier support for matrix selectors
- [ ] Handle subquery syntax (`[5m:1m]`)

**Completion Criteria**:
- Parses: `metric[5m]`, `metric[1h30m:30s]`, `metric[5m] offset 1h`
- Duration validation works correctly
- Subquery syntax supported
- Unit tests pass for matrix selectors

---

**TASK 1.4: Complete Binary Expression Parsing**
**File**: `src/tsdb/prometheus/promql/parser.cpp`  
**Method**: `parseInfixExpression()`  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Complete `parseInfixExpression()` implementation
- [ ] Handle all binary operators with correct precedence
- [ ] Implement arithmetic operators (`+`, `-`, `*`, `/`, `%`, `^`)
- [ ] Implement comparison operators (`==`, `!=`, `<`, `<=`, `>`, `>=`)
- [ ] Implement logical operators (`and`, `or`, `unless`)
- [ ] Support vector matching modifiers (`on`, `ignoring`, `group_left`, `group_right`)
- [ ] Handle associativity rules correctly
- [ ] Add bool modifier support for comparisons

**Completion Criteria**:
- Correct operator precedence: `2 + 3 * 4 ^ 2` = `2 + (3 * (4 ^ 2))`
- Vector matching: `metric1 * on(label) group_left metric2`
- Boolean comparisons: `metric > 0.5 bool`
- All operators work correctly
- Unit tests pass for binary expressions

---

**TASK 1.5: Complete Function Call Parsing**
**File**: `src/tsdb/prometheus/promql/parser.cpp`  
**Method**: `parseCallExpression()`  
**Estimated Time**: 6 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Complete `parseCallExpression()` implementation
- [ ] Handle function arguments (expressions and scalars)
- [ ] Support nested function calls (`rate(sum(metric)[5m])`)
- [ ] Validate function syntax and argument count
- [ ] Handle empty argument lists and trailing commas
- [ ] Add comprehensive error messages for invalid calls

**Completion Criteria**:
- Parses: `rate(metric[5m])`, `sum(rate(metric[5m]))`
- Nested calls work correctly
- Argument validation is comprehensive
- Error messages are helpful
- Unit tests pass for function calls

---

**TASK 1.6: Complete Aggregation Expression Parsing**
**File**: `src/tsdb/prometheus/promql/parser.cpp`  
**Method**: `parseAggregateExpression()`  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Complete `parseAggregateExpression()` implementation
- [ ] Handle all aggregation operators (`sum`, `avg`, `count`, `min`, `max`, etc.)
- [ ] Support grouping clauses (`by (label1, label2)`, `without (label)`)
- [ ] Handle parameterized aggregations (`topk(5, metric)`, `quantile(0.95, metric)`)
- [ ] Validate aggregation syntax and parameters
- [ ] Add comprehensive error handling

**Completion Criteria**:
- Parses: `sum by (job) (metric)`, `topk(5, sum by (job) (metric))`
- All aggregation operators work
- Grouping clauses are correct
- Parameterized aggregations work
- Unit tests pass for aggregations

#### **WEEK 2: Advanced Parser Features**

**TASK 2.1: Complete Subquery Parsing**
**File**: `src/tsdb/prometheus/promql/parser.cpp`  
**Method**: `parseSubqueryExpression()`  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Complete `parseSubqueryExpression()` implementation
- [ ] Handle complex subquery syntax (`expr[range:resolution]`)
- [ ] Support nested subqueries
- [ ] Add duration validation for range and resolution
- [ ] Handle optional resolution parameter
- [ ] Support offset and @ modifiers in subqueries

**Completion Criteria**:
- Parses: `rate(metric[5m])[1h:5m]`, `sum(rate(metric[5m]))[30m:]`
- Nested subqueries work correctly
- Duration validation is comprehensive
- Unit tests pass for subqueries

---

**TASK 2.2: Complete @ Modifier Support**
**File**: `src/tsdb/prometheus/promql/parser.cpp`  
**Methods**: Multiple parsing methods  
**Estimated Time**: 6 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Add @ modifier parsing to vector selectors
- [ ] Add @ modifier parsing to matrix selectors
- [ ] Add @ modifier parsing to subqueries
- [ ] Handle `start()` and `end()` functions in @ modifier
- [ ] Validate timestamp expressions
- [ ] Support complex @ expressions

**Completion Criteria**:
- Parses: `metric @ 1609459200`, `metric[5m] @ start()`
- All @ modifier variations work
- Timestamp validation is correct
- Unit tests pass for @ modifiers

---

**TASK 2.3: Implement Robust Error Recovery**
**File**: `src/tsdb/prometheus/promql/parser.cpp`  
**Methods**: All parsing methods  
**Estimated Time**: 6 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement comprehensive error reporting
- [ ] Add error recovery strategies for partial parsing
- [ ] Provide helpful error messages with context
- [ ] Handle unexpected tokens gracefully
- [ ] Add position information to all errors
- [ ] Implement error aggregation for multiple issues

**Completion Criteria**:
- Clear error messages for syntax errors
- Position information in all errors
- Graceful handling of malformed queries
- Error recovery allows continued parsing
- Unit tests cover error conditions

---

**TASK 2.4: Complete Parser Testing with Performance Instrumentation**
**File**: `test/unit/promql_parser_comprehensive_test.cpp`  
**File**: `src/tsdb/prometheus/promql/parser_metrics.h/.cpp` (new)  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Design Guidance**:
- **Early Metrics Integration**: Integrate performance monitoring into parser from the beginning
- **Parsing Performance Baseline**: Establish baseline performance metrics for different query complexities
- **Memory Usage Tracking**: Track memory allocation during AST construction
- **Error Rate Monitoring**: Monitor parsing error rates and types

**Actions**:
- [ ] **Create comprehensive parser test suite** with performance benchmarking
- [ ] **Integrate parser performance metrics** using existing `AtomicMetrics` infrastructure
- [ ] Test all expression types and combinations with timing measurements
- [ ] Test complex nested expressions with memory usage tracking
- [ ] Test error conditions and edge cases with error rate monitoring
- [ ] **Add parsing performance benchmarks** for different query complexities
- [ ] **Implement parsing time prediction** based on query complexity
- [ ] Validate against PromQL specification examples with compliance metrics

**Parser Performance Metrics**:
```cpp
class ParserMetrics {
public:
    struct ParsingStats {
        std::atomic<uint64_t> queries_parsed{0};
        std::atomic<uint64_t> parse_time_ns{0};
        std::atomic<uint64_t> ast_nodes_created{0};
        std::atomic<uint64_t> memory_allocated{0};
        std::atomic<uint64_t> syntax_errors{0};
        std::atomic<uint64_t> complex_queries{0};
        
        // Query complexity metrics
        std::atomic<uint64_t> max_ast_depth{0};
        std::atomic<uint64_t> total_operations{0};
        std::atomic<uint64_t> function_calls{0};
    };
    
    void RecordParsing(const std::string& query, 
                      std::chrono::nanoseconds parse_time,
                      size_t ast_nodes, 
                      size_t memory_used);
    ParsingStats GetStats() const;
};
```

**Performance Integration**:
- [ ] **Parsing Time Tracking**: Measure parsing time for all query types
- [ ] **Memory Usage Monitoring**: Track AST construction memory usage
- [ ] **Complexity Scoring**: Score query complexity based on AST structure
- [ ] **Error Rate Tracking**: Monitor parsing error rates by error type
- [ ] **Performance Regression Detection**: Alert on parsing performance degradation

**Completion Criteria**:
- >95% code coverage for parser with performance instrumentation
- All PromQL expression types tested with performance benchmarks
- Edge cases and error conditions covered with error rate monitoring
- **Parsing performance baselines established** for all query types
- **Performance regression testing** integrated into CI/CD
- All tests pass consistently with performance metrics collection
- **Query complexity scoring** provides accurate parsing time predictions

---

**TASK 2.5: Update Planning Documentation - Phase 1 Complete**
**File**: `docs/planning/PROMQL_ENGINE_AGENT_TASKS.md`
**Estimated Time**: 30 minutes
**Status**: ✅ **COMPLETED**

**Actions**:
- [x] Update Phase 1 status to "COMPLETED"
- [x] Add completion date and achievements summary
- [x] Update overall progress (Phase 1/6 complete)
- [x] Document any issues encountered and solutions
- [x] Commit changes with detailed completion report

**Completion Criteria**:
- Phase 1 marked as completed
- Progress accurately reflects current state
- Issues and solutions documented

### **PHASE 2: Query Engine Core (Weeks 3-4)**

#### **WEEK 3: Evaluation Engine Foundation**

**TASK 3.1: Create Engine Class Structure with Performance Monitoring**
**File**: `src/tsdb/prometheus/promql/engine.h` (new)  
**File**: `src/tsdb/prometheus/promql/engine.cpp` (new)  
**Estimated Time**: 8 hours  
**Status**: ✅ **COMPLETED**

**Design Guidance**:
- **Built-in Metrics**: Integrate performance monitoring from the start using existing `AtomicMetrics` infrastructure
- **Query Lifecycle Tracking**: Track every phase of query execution with detailed timing
- **Resource Monitoring**: Monitor memory allocation, CPU usage, and I/O operations
- **Prometheus EngineOpts**: **Refer to Prometheus's `promql.EngineOpts` for common parameters**: `LookbackDelta`, `QueryTimeout`, `MaxSamplesPerQuery`

**Actions**:
- [ ] Create `Engine` class with complete interface and built-in performance monitoring
- [ ] Implement query execution pipeline with detailed phase timing
- [ ] Add evaluation context management with resource tracking
- [ ] Create result type system (`QueryResult`, `Value` types) with size monitoring
- [ ] Add query timeout and cancellation support with timeout metrics
- [ ] Implement resource limit checking with threshold alerting
- [ ] **Integrate PromQL metrics collection** using existing `AtomicMetrics` patterns
- [ ] **Add query complexity scoring** for performance prediction

**Performance Integration**:
```cpp
class Engine {
public:
    struct Config {
        Duration lookback_delta = std::chrono::minutes(5);
        Duration query_timeout = std::chrono::seconds(30);
        size_t max_samples_per_query = 50000000;
        bool enable_at_modifier = true;
        bool enable_negative_offset = true;
    };
    
    struct QueryStats {
        std::chrono::nanoseconds parsing_time;
        std::chrono::nanoseconds evaluation_time;
        std::chrono::nanoseconds total_time;
        size_t samples_processed;
        size_t series_touched;
        size_t memory_used;
    };
    
private:
    std::unique_ptr<PromQLMetrics> metrics_;
    Config config_;
};
```

**Completion Criteria**:
- `Engine` class provides complete API with integrated monitoring
- Query pipeline is well-structured with detailed instrumentation
- Resource management is implemented with real-time tracking
- Performance metrics are collected for all operations
- Query complexity scoring provides execution time estimates
- Unit tests cover basic engine operations and metrics collection

---

**TASK 3.2: Implement Expression Evaluator**
**File**: `src/tsdb/prometheus/promql/evaluator.h` (new)  
**File**: `src/tsdb/prometheus/promql/evaluator.cpp` (new)  
**Estimated Time**: 10 hours  
**Status**: ✅ **COMPLETED**

**Actions**:
- [ ] Create `Evaluator` class for AST traversal
- [ ] Implement visitor pattern for expression evaluation
- [ ] Handle different result types (scalar, vector, matrix, string)
- [ ] Add evaluation context with timestamp and step information
- [ ] Implement evaluation error handling and propagation
- [ ] Add support for evaluation warnings

**Completion Criteria**:
- Complete AST evaluation capability
- All result types handled correctly
- Error handling is comprehensive
- Evaluation context is properly managed
- Unit tests cover evaluation logic

---

**TASK 3.3: Implement Basic Arithmetic Operations**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: Binary operation evaluation  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement scalar arithmetic (`+`, `-`, `*`, `/`, `%`, `^`)
- [ ] Implement vector arithmetic with broadcasting rules
- [ ] Handle division by zero and other edge cases
- [ ] Add type coercion logic (scalar-vector operations)
- [ ] Implement proper NaN and infinity handling
- [ ] Add comprehensive error checking

**Completion Criteria**:
- All arithmetic operators work correctly
- Broadcasting rules match PromQL specification
- Edge cases handled properly (NaN, Inf, division by zero)
- Type coercion works as expected
- Unit tests cover all arithmetic operations

---

**TASK 3.4: Implement Comparison Operations**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: Comparison operation evaluation  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement scalar comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`)
- [ ] Implement vector comparisons with filtering
- [ ] Support `bool` modifier for comparisons
- [ ] Handle NaN and infinity comparison semantics
- [ ] Implement proper comparison result types
- [ ] Add comprehensive edge case handling

**Completion Criteria**:
- All comparison operators work correctly
- Boolean modifier produces correct results
- NaN/Infinity comparisons match PromQL behavior
- Vector filtering works as expected
- Unit tests cover all comparison operations

#### **WEEK 4: Storage Integration**

**TASK 4.1: Create Storage Adapter Interface**
**File**: `src/tsdb/prometheus/promql/storage_adapter.h` (new)  
**File**: `src/tsdb/prometheus/promql/storage_adapter.cpp` (new)  
**Estimated Time**: 8 hours  
**Status**: ✅ **COMPLETED**

**Design Guidance**:
- **Storage Integration**: Define interface to fetch data from existing `Storage` layer in `include/tsdb/storage/storage.h`
- **Label Matchers**: Translate `FetchSeriesNode` with label matchers and time ranges into storage API calls
- **Lookback Delta**: Include **lookback delta for matrix selectors** as hint to storage layer for pre-fetching
- **Enhanced Interface**: Consider if current `Storage::Read(series_id, start, end)` is sufficient or if new method needed:
  ```cpp
  core::Result<std::vector<model::Series>> SelectSeries(
      const std::vector<core::LabelMatcher>& matchers, 
      core::Timestamp mint, 
      core::Timestamp maxt, 
      core::Duration lookback_delta_hint
  )
  ```
- **Performance**: The `hint_lookback_delta` helps storage layer pre-fetch data for functions like `rate` or `increase`

**Actions**:
- [ ] Define storage interface for PromQL queries following existing `Storage` interface patterns
- [ ] Implement series selection by label matchers with efficient filtering
- [ ] Add time range filtering with lookback delta support (**lookback delta for matrix selectors**)
- [ ] Create efficient label indexing interface leveraging existing TSDB indexing
- [ ] Add metadata querying (label names/values) using existing storage capabilities
- [ ] Implement query result caching for performance
- [ ] Evaluate if `Storage` interface enhancement needed for label-based selection

**Technical Details**:
- Use core TSDB types: `core::Label`, `core::Sample`, `core::TimeSeries`, `core::Result`, `core::Error`
- Integrate with existing storage implementations (e.g., `StorageImpl`)
- Leverage advanced performance features (cache hierarchy, object pools, compression)

**Completion Criteria**:
- Storage interface supports all PromQL query needs
- Label matching is efficient and leverages existing indexing
- Time range queries are optimized with proper lookback handling
- Metadata queries work correctly and efficiently
- Caching improves performance measurably
- Integration with existing storage layer is seamless

---

**TASK 4.2: Integrate with TSDB Storage**
**File**: `src/tsdb/prometheus/promql/storage_adapter.cpp`  
**Estimated Time**: 10 hours  
**Status**: ✅ **COMPLETED**

**Actions**:
- [ ] Connect PromQL engine to existing TSDB storage
- [ ] Implement series iteration and sampling
- [ ] Add efficient label-based series selection
- [ ] Optimize query performance using existing TSDB features
- [ ] Integrate with advanced performance features (cache, compression)
- [ ] Add comprehensive error handling

**Completion Criteria**:
- Full integration with TSDB storage
- Performance matches or exceeds expectations
- All TSDB features properly utilized
- Error handling is comprehensive
- Integration tests pass

---

**TASK 4.3: Implement Vector Selector Evaluation**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: Vector selector evaluation  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement vector selector evaluation logic
- [ ] Handle label matching efficiently (exact, regex)
- [ ] Support all label matching operators (`=`, `!=`, `=~`, `!~`)
- [ ] Add offset modifier support
- [ ] Add @ modifier support with timestamp evaluation
- [ ] Optimize for high-cardinality label sets

**Completion Criteria**:
- Vector selectors work correctly
- All label matching operators supported
- Offset and @ modifiers work
- Performance is optimized
- Unit tests cover all selector types

---

**TASK 4.4: Implement Matrix Selector Evaluation**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: Matrix selector evaluation  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement matrix selector evaluation logic
- [ ] Handle time range queries efficiently
- [ ] Support variable step sizes and lookback
- [ ] Optimize memory usage for large ranges
- [ ] Add subquery evaluation support
- [ ] Handle offset and @ modifiers for matrices

**Completion Criteria**:
- Matrix selectors work correctly
- Time range queries are efficient
- Memory usage is optimized
- Subqueries are supported
- Unit tests cover matrix evaluation

---

**TASK 4.5: Update Planning Documentation - Phase 2 Complete**
**File**: `docs/planning/PROMQL_ENGINE_AGENT_TASKS.md`  
**Estimated Time**: 30 minutes  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Update Phase 2 status to "COMPLETED"
- [ ] Add completion date and achievements summary
- [ ] Update overall progress (Phase 2/6 complete)
- [ ] Document performance benchmarks achieved
- [ ] Commit changes with detailed completion report

**Completion Criteria**:
- Phase 2 marked as completed
- Progress accurately reflects current state
- Performance metrics documented

### **PHASE 3: Functions Library (Weeks 5-7)**

#### **WEEK 5: Mathematical Functions**

**TASK 5.1: Create Function Registry System**
**File**: `src/tsdb/prometheus/promql/functions.h` (new)  
**File**: `src/tsdb/prometheus/promql/functions.cpp` (new)  
**Estimated Time**: 6 hours  
**Status**: ✅ **COMPLETED**

**Actions**:
- [ ] Create `FunctionRegistry` class
- [ ] Design function signature system
- [ ] Implement function registration mechanism
- [ ] Add function argument validation
- [ ] Create function call evaluation framework
- [ ] Add comprehensive error handling for functions

**Completion Criteria**:
- Function registry is complete and extensible
- Function signatures are properly validated
- Error handling is comprehensive
- Framework supports all function types

---

**TASK 5.2: Implement Basic Math Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: `abs()`, `ceil()`, `floor()`, `round()`, `sqrt()`  
**Estimated Time**: 8 hours  
**Status**: ✅ **COMPLETED**

**Actions**:
- [ ] Implement `abs()` function with NaN/Inf handling
- [ ] Implement `ceil()`, `floor()`, `round()` functions
- [ ] Implement `sqrt()` with domain validation
- [ ] Add proper NaN and infinity handling for all functions
- [ ] Create comprehensive test coverage
- [ ] Optimize for vector operations

**Completion Criteria**:
- All basic math functions work correctly
- NaN/Inf handling matches PromQL specification
- Vector operations are optimized
- Unit tests achieve >95% coverage

---

**TASK 5.3: Implement Advanced Math Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: `exp()`, `ln()`, `log2()`, `log10()`, trigonometric functions  
**Estimated Time**: 10 hours  
**Status**: ✅ **COMPLETED**

**Actions**:
- [ ] Implement exponential and logarithmic functions
- [ ] Implement trigonometric functions (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`)
- [ ] Handle domain restrictions and edge cases
- [ ] Add performance optimizations
- [ ] Create comprehensive test coverage
- [ ] Validate against reference implementations

**Completion Criteria**:
- All advanced math functions implemented
- Domain restrictions properly handled
- Performance is optimized
- Results match reference implementations
- Unit tests are comprehensive

---

**TASK 5.4: Implement Utility Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: `clamp_max()`, `clamp_min()`, `sort()`, `sort_desc()`, `reverse()`  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement clamping functions with proper validation
- [ ] Implement sorting functions with efficient algorithms
- [ ] Implement `reverse()` function
- [ ] Implement `vector()` and `scalar()` conversion functions
- [ ] Add comprehensive error handling
- [ ] Optimize for large vectors

**Completion Criteria**:
- All utility functions work correctly
- Sorting algorithms are efficient
- Type conversion functions work properly
- Performance is optimized for large datasets
- Unit tests are comprehensive

#### **WEEK 6: Time Series Functions**

**TASK 6.1: Implement Rate and Increase Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: `rate()`, `irate()`, `increase()`, `idelta()`  
**Estimated Time**: 12 hours  
**Status**: 🔴 Not Started

**Design Guidance**:
- **Counter Reset Handling**: **Pay attention to the specific evaluation logic for each function, e.g., how `rate` and `increase` handle counter resets and extrapolation**
- **Prometheus Behavior**: Study Prometheus's implementation to match exact counter reset detection
- **Extrapolation**: Implement proper extrapolation at range boundaries as per Prometheus specification
- **Performance**: The `hint_lookback_delta` in storage helps pre-fetch necessary data for functions like `rate` or `increase`

**Actions**:
- [ ] Implement `rate()` with proper counter reset handling following Prometheus algorithm
- [ ] Implement `irate()` for instant rate calculation using last two samples
- [ ] Implement `increase()` with extrapolation logic matching Prometheus behavior
- [ ] Implement `idelta()` for instant delta calculation
- [ ] Handle counter resets correctly: detect decreases and handle wrap-around
- [ ] Add proper extrapolation at range boundaries (beginning and end of range)
- [ ] Optimize for high-frequency data using efficient sample processing
- [ ] Handle edge cases: single sample, missing samples, zero values

**Technical Details**:
- Counter reset detection: if current_value < previous_value, assume reset
- Extrapolation formula: rate = (last_value - first_value) / (last_timestamp - first_timestamp)
- Handle 32-bit and 64-bit counter wrap-around scenarios
- Use double precision for all calculations to avoid precision loss

**Completion Criteria**:
- Rate functions handle counter resets correctly per Prometheus specification
- Extrapolation logic matches PromQL specification exactly
- Performance is optimized for high-frequency data streams
- Edge cases are handled properly (single samples, resets, etc.)
- Unit tests cover all scenarios including counter resets and extrapolation
- Results match Prometheus reference implementation

---

**TASK 6.2: Implement Delta and Derivative Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: `delta()`, `deriv()`, `predict_linear()`, `holt_winters()`  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement `delta()` function
- [ ] Implement `deriv()` with proper derivative calculation
- [ ] Implement `predict_linear()` with linear regression
- [ ] Implement `holt_winters()` with triple exponential smoothing
- [ ] Handle missing data points appropriately
- [ ] Implement statistical algorithms correctly

**Completion Criteria**:
- Delta and derivative calculations are correct
- Statistical algorithms match reference implementations
- Missing data is handled appropriately
- Performance is acceptable for complex calculations
- Unit tests validate mathematical correctness

---

**TASK 6.3: Implement Aggregation Over Time Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: `*_over_time()` family  
**Estimated Time**: 12 hours  
**Status**: 🔴 Not Started

**Design Guidance**:
- **Reference Implementation**: Study Prometheus's function implementations for exact behavior
- **Lookback Deltas**: **Explicitly plan for lookback deltas in range vector selectors and `_over_time` aggregations**
- **Statistical Accuracy**: Ensure mathematical correctness matches Prometheus reference implementation
- **Performance**: Optimize for sliding window calculations to handle large time ranges efficiently
- **Data Handling**: Handle missing data points and edge cases as per Prometheus behavior

**Actions**:
- [ ] Implement `avg_over_time()`, `min_over_time()`, `max_over_time()` with proper statistical algorithms
- [ ] Implement `sum_over_time()`, `count_over_time()` with overflow protection
- [ ] Implement `stddev_over_time()`, `stdvar_over_time()` with numerically stable algorithms
- [ ] Implement `quantile_over_time()`, `last_over_time()` following Prometheus specification
- [ ] Optimize for sliding window calculations using efficient data structures
- [ ] Handle edge cases: empty ranges, single values, missing data
- [ ] **Pay close attention to evaluation timestamps, step resolution, and alignment as in Prometheus's `Engine`**

**Technical Details**:
- Use efficient algorithms for statistical calculations (Welford's algorithm for variance)
- Handle floating-point precision issues (NaN, Inf, very large/small numbers)
- Implement proper quantile calculation (linear interpolation between samples)
- Ensure thread safety for concurrent evaluation

**Completion Criteria**:
- All aggregation over time functions work correctly per Prometheus specification
- Sliding window calculations are optimized for performance
- Statistical calculations are mathematically correct and numerically stable
- Edge cases are handled appropriately (empty data, single values, etc.)
- Unit tests cover all functions with comprehensive test cases
- Performance meets targets for large time ranges

#### **WEEK 7: Specialized Functions**

**TASK 7.1: Implement Histogram Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: `histogram_quantile()`, `histogram_count()`, `histogram_sum()`  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement `histogram_quantile()` with bucket interpolation
- [ ] Implement `histogram_count()` and `histogram_sum()`
- [ ] Handle Prometheus histogram format correctly
- [ ] Add proper bucket boundary validation
- [ ] Validate histogram consistency
- [ ] Optimize for large histograms

**Completion Criteria**:
- Histogram functions work with Prometheus format
- Bucket interpolation is mathematically correct
- Validation prevents inconsistent histograms
- Performance is optimized
- Unit tests cover edge cases

---

**TASK 7.2: Implement Label Manipulation Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: `label_replace()`, `label_join()`  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement `label_replace()` with regex support
- [ ] Implement `label_join()` with proper concatenation
- [ ] Add comprehensive regex validation
- [ ] Handle label name/value restrictions
- [ ] Optimize label processing performance
- [ ] Add comprehensive error handling

**Completion Criteria**:
- Label manipulation functions work correctly
- Regex operations are properly validated
- Label restrictions are enforced
- Performance is optimized
- Unit tests cover all scenarios

---

**TASK 7.3: Implement Date/Time Functions**
**File**: `src/tsdb/prometheus/promql/functions.cpp`  
**Functions**: Time-related functions  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement date functions (`year()`, `month()`, `day_of_month()`, `day_of_week()`)
- [ ] Implement time functions (`hour()`, `minute()`, `days_in_month()`)
- [ ] Add `time()` function for current timestamp
- [ ] Handle timezone considerations properly
- [ ] Add comprehensive validation
- [ ] Optimize for frequent calls

**Completion Criteria**:
- All date/time functions work correctly
- Timezone handling is appropriate
- Performance is optimized
- Unit tests cover all functions

---

**TASK 7.4: Update Planning Documentation - Phase 3 Complete**
**File**: `docs/planning/PROMQL_ENGINE_AGENT_TASKS.md`  
**Estimated Time**: 30 minutes  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Update Phase 3 status to "COMPLETED"
- [ ] Add completion date and function count summary
- [ ] Update overall progress (Phase 3/6 complete)
- [ ] Document function performance benchmarks
- [ ] Commit changes with detailed completion report

**Completion Criteria**:
- Phase 3 marked as completed
- Function library is complete (50+ functions)
- Performance benchmarks documented

### **PHASE 4: Advanced Features (Weeks 8-9)**

#### **WEEK 8: Binary Vector Matching**

**TASK 8.1: Implement One-to-One Matching**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: Binary operation evaluation with vector matching  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement basic vector matching algorithm
- [ ] Handle `on()` modifier for label selection
- [ ] Handle `ignoring()` modifier for label exclusion
- [ ] Add proper label set handling and comparison
- [ ] Optimize matching algorithms for performance
- [ ] Add comprehensive error handling

**Completion Criteria**:
- One-to-one vector matching works correctly
- `on()` and `ignoring()` modifiers work properly
- Matching algorithm is optimized
- Error handling is comprehensive
- Unit tests cover all matching scenarios

---

**TASK 8.2: Implement One-to-Many Matching**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: Vector matching with group modifiers  
**Estimated Time**: 12 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement `group_left()` modifier
- [ ] Implement `group_right()` modifier
- [ ] Handle many-to-one relationships correctly
- [ ] Add cardinality validation to prevent errors
- [ ] Prevent infinite result sets
- [ ] Add comprehensive validation and error messages

**Completion Criteria**:
- One-to-many matching works correctly
- Group modifiers produce correct results
- Cardinality validation prevents errors
- Performance is acceptable for large datasets
- Unit tests cover complex matching scenarios

---

**TASK 8.3: Implement Advanced Matching Features**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: Complex vector matching scenarios  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Support complex matching expressions
- [ ] Add regex matching in vector operations
- [ ] Handle edge cases and error conditions
- [ ] Performance optimization for large label sets
- [ ] Add comprehensive validation
- [ ] Create extensive test coverage

**Completion Criteria**:
- Complex matching scenarios work correctly
- Regex matching is properly supported
- Edge cases are handled gracefully
- Performance is optimized
- Unit tests achieve comprehensive coverage

#### **WEEK 9: Subqueries and Modifiers**

**TASK 9.1: Implement Subquery Evaluation**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: Subquery expression evaluation  
**Estimated Time**: 12 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement subquery execution engine
- [ ] Handle nested time ranges correctly
- [ ] Add step size validation and defaults
- [ ] Optimize memory usage for subqueries
- [ ] Handle subquery result aggregation
- [ ] Add comprehensive error handling

**Completion Criteria**:
- Subqueries execute correctly
- Nested time ranges are handled properly
- Memory usage is optimized
- Performance is acceptable
- Unit tests cover subquery scenarios

---

**TASK 9.2: Implement @ Modifier Evaluation**
**File**: `src/tsdb/prometheus/promql/evaluator.cpp`  
**Methods**: @ modifier evaluation  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement timestamp evaluation for @ modifier
- [ ] Handle `start()` and `end()` functions
- [ ] Add timestamp validation and range checking
- [ ] Support complex @ expressions
- [ ] Optimize timestamp-based queries
- [ ] Add comprehensive error handling

**Completion Criteria**:
- @ modifier works correctly
- `start()` and `end()` functions work
- Timestamp validation is comprehensive
- Performance is optimized
- Unit tests cover all @ modifier scenarios

---

**TASK 9.3: Performance Optimization**
**File**: Multiple files  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Profile query execution performance
- [ ] Optimize hot paths in evaluation
- [ ] Add query result caching mechanisms
- [ ] Implement query planning optimizations
- [ ] Add parallel evaluation where possible
- [ ] Benchmark against performance targets

**Completion Criteria**:
- Query performance meets targets (<10s for complex queries)
- Hot paths are optimized
- Caching improves performance
- Parallel evaluation works correctly
- Benchmarks validate performance improvements

---

**TASK 9.4: Update Planning Documentation - Phase 4 Complete**
**File**: `docs/planning/PROMQL_ENGINE_AGENT_TASKS.md`  
**Estimated Time**: 30 minutes  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Update Phase 4 status to "COMPLETED"
- [ ] Add completion date and feature summary
- [ ] Update overall progress (Phase 4/6 complete)
- [ ] Document performance optimization results
- [ ] Commit changes with detailed completion report

**Completion Criteria**:
- Phase 4 marked as completed
- Advanced features are fully implemented
- Performance optimizations documented

### **PHASE 5: Testing & Compliance (Weeks 10-11)**

#### **WEEK 10: Comprehensive Testing**

**TASK 10.1: Create Complete Unit Test Suite**
**File**: `test/unit/promql_engine_complete_test.cpp` (new)  
**Estimated Time**: 12 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Create comprehensive unit tests for all functions
- [ ] Test edge cases and error conditions extensively
- [ ] Add performance benchmarks for all components
- [ ] Achieve >95% code coverage
- [ ] Test memory usage and leak detection
- [ ] Add stress tests for complex queries

**Completion Criteria**:
- >95% code coverage achieved
- All functions and edge cases tested
- Performance benchmarks established
- Memory leaks eliminated
- Stress tests pass consistently

---

**TASK 10.2: Create Integration Test Suite**
**File**: `test/integration/promql_integration_test.cpp` (new)  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Test complete query execution pipeline
- [ ] Validate against real TSDB data
- [ ] Test concurrent query execution
- [ ] Add end-to-end HTTP API tests
- [ ] Test with large datasets
- [ ] Add timeout and cancellation tests

**Completion Criteria**:
- Complete pipeline integration tested
- Concurrent execution works correctly
- HTTP API integration validated
- Large dataset performance acceptable
- Timeout and cancellation work properly

---

**TASK 10.3: PromQL Compliance Testing**
**File**: `test/compliance/promql_compliance_test.cpp` (new)  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Run official PromQL compliance tests
- [ ] Fix any compatibility issues discovered
- [ ] Validate query result accuracy against reference
- [ ] Test all PromQL specification examples
- [ ] Document any limitations or differences
- [ ] Create compliance report

**Completion Criteria**:
- Official compliance tests pass
- Query results match reference implementation
- All specification examples work
- Limitations are documented
- Compliance report is complete

#### **WEEK 11: Performance and Reliability**

**TASK 11.1: Comprehensive Performance Optimization and Benchmarking**
**File**: `benchmarks/promql_benchmarks.cpp` (new)  
**File**: `src/tsdb/prometheus/promql/performance_profiler.h/.cpp` (new)  
**Estimated Time**: 12 hours  
**Status**: 🔴 Not Started

**Design Guidance**:
- **Leverage Existing Infrastructure**: Use existing performance testing patterns from semantic vector implementation
- **Comprehensive Benchmarking**: Test all query types, functions, and complexity levels
- **Real-world Scenarios**: Include realistic query patterns from production Prometheus deployments
- **Performance Regression Testing**: Establish automated performance regression detection

**Actions**:
- [ ] **Create comprehensive benchmark suite** covering all PromQL features
- [ ] **Implement query performance profiler** with detailed execution analysis
- [ ] **Add memory usage optimization** with allocation tracking and pooling
- [ ] **Implement intelligent query result caching** with cache hit/miss metrics
- [ ] **Add parallel query execution** where beneficial with concurrency metrics
- [ ] **Create performance regression testing** framework with automated alerts
- [ ] **Validate performance targets** against realistic production workloads

**Detailed Benchmarking Categories**:

**1. Query Parsing Benchmarks**:
```cpp
// Benchmark different query complexities
BENCHMARK(BenchmarkSimpleQuery);      // metric_name
BENCHMARK(BenchmarkComplexQuery);     // sum(rate(metric[5m])) by (label)
BENCHMARK(BenchmarkDeepNesting);      // Multiple levels of function nesting
BENCHMARK(BenchmarkLargeExpressions); // Queries with many operations
```

**2. Function Performance Benchmarks**:
```cpp
// Benchmark all PromQL functions
BENCHMARK(BenchmarkRateFunction);
BENCHMARK(BenchmarkHistogramQuantile);
BENCHMARK(BenchmarkAggregationFunctions);
BENCHMARK(BenchmarkMathFunctions);
BENCHMARK(BenchmarkTimeSeriesFunctions);
```

**3. Storage Integration Benchmarks**:
```cpp
// Benchmark storage interactions
BENCHMARK(BenchmarkHighCardinalityQueries);  // Many series
BENCHMARK(BenchmarkLongTimeRanges);          // Large time spans
BENCHMARK(BenchmarkFrequentQueries);         // Cache effectiveness
BENCHMARK(BenchmarkConcurrentQueries);       // Parallel execution
```

**4. Memory Usage Benchmarks**:
```cpp
// Benchmark memory efficiency
BENCHMARK(BenchmarkLargeResultSets);
BENCHMARK(BenchmarkMemoryPooling);
BENCHMARK(BenchmarkGarbageCollection);
BENCHMARK(BenchmarkMemoryFragmentation);
```

**Performance Profiler Integration**:
```cpp
class PerformanceProfiler {
public:
    struct QueryProfile {
        std::string query;
        std::chrono::nanoseconds parse_time;
        std::chrono::nanoseconds eval_time;
        std::chrono::nanoseconds storage_time;
        size_t memory_peak;
        size_t samples_processed;
        size_t series_touched;
        std::vector<FunctionProfile> function_profiles;
    };
    
    QueryProfile ProfileQuery(const std::string& query);
    void GeneratePerformanceReport();
    bool DetectPerformanceRegression(const QueryProfile& current, const QueryProfile& baseline);
};
```

**Performance Targets and Validation**:
- [ ] **Query Latency**: <10s for complex queries, <1s for simple queries
- [ ] **Memory Usage**: <1GB for large result sets, efficient memory pooling
- [ ] **Throughput**: >1000 queries/second for simple queries
- [ ] **Concurrency**: >100 concurrent queries without performance degradation
- [ ] **Cache Effectiveness**: >80% cache hit ratio for repeated queries
- [ ] **Resource Efficiency**: <50% CPU utilization at target throughput

**Advanced Performance Features**:
- [ ] **Query Optimization**: Automatic query plan optimization based on statistics
- [ ] **Adaptive Caching**: Dynamic cache sizing based on query patterns
- [ ] **Resource Prediction**: Predict resource requirements before query execution
- [ ] **Load Balancing**: Distribute queries across available resources
- [ ] **Performance Alerts**: Real-time alerts for performance threshold violations

**Completion Criteria**:
- **Comprehensive Benchmarks**: All PromQL features benchmarked with realistic workloads
- **Performance Targets Met**: All latency, throughput, and resource targets achieved
- **Regression Testing**: Automated performance regression detection implemented
- **Production Ready**: Performance validated under realistic production conditions
- **Monitoring Integration**: Performance metrics integrated with existing monitoring infrastructure
- **Optimization Proven**: Performance optimizations validated with measurable improvements

---

**TASK 11.2: Error Handling and Reliability**
**File**: Multiple files  
**Estimated Time**: 8 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Implement comprehensive error handling throughout
- [ ] Add query timeout mechanisms
- [ ] Handle resource exhaustion gracefully
- [ ] Add query cancellation support
- [ ] Test error recovery scenarios
- [ ] Add comprehensive logging

**Completion Criteria**:
- Error handling is comprehensive
- Timeout mechanisms work correctly
- Resource exhaustion handled gracefully
- Query cancellation works properly
- Error recovery is robust
- Logging provides good diagnostics

---

**TASK 11.3: Comprehensive PromQL Performance Monitoring**
**File**: `src/tsdb/prometheus/promql/metrics.h/.cpp` (new)  
**File**: `src/tsdb/prometheus/promql/performance_monitor.h/.cpp` (new)  
**Estimated Time**: 10 hours  
**Status**: 🔴 Not Started

**Design Guidance**:
- **Leverage Existing Infrastructure**: Use existing `AtomicMetrics`, `core::PerformanceMetrics`, and OpenTelemetry integration
- **Query Lifecycle Tracking**: Instrument every phase of query execution with detailed timing and resource metrics
- **Performance Baselines**: Establish performance baselines for different query types and complexity levels
- **Real-time Monitoring**: Provide real-time visibility into query performance and resource utilization

**Actions**:
- [ ] **Create PromQL-specific metrics system** leveraging existing `AtomicMetrics` infrastructure
- [ ] **Implement query lifecycle instrumentation** with detailed phase timing
- [ ] **Add comprehensive resource utilization tracking** (memory, CPU, I/O)
- [ ] **Create query performance profiler** with execution plan analysis
- [ ] **Implement query complexity scoring** and performance prediction
- [ ] **Add real-time monitoring dashboard** with alerts and thresholds
- [ ] **Create performance regression testing** framework

**Detailed Metrics to Track**:

**1. Query Parsing Metrics**:
```cpp
struct QueryParsingMetrics {
    std::atomic<uint64_t> total_queries_parsed{0};
    std::atomic<uint64_t> parsing_time_ns{0};
    std::atomic<uint64_t> lexing_time_ns{0};
    std::atomic<uint64_t> ast_construction_time_ns{0};
    std::atomic<uint64_t> syntax_errors{0};
    std::atomic<uint64_t> complex_queries{0};  // queries with >10 operations
};
```

**2. Query Evaluation Metrics**:
```cpp
struct QueryEvaluationMetrics {
    std::atomic<uint64_t> total_evaluations{0};
    std::atomic<uint64_t> evaluation_time_ns{0};
    std::atomic<uint64_t> function_calls{0};
    std::atomic<uint64_t> vector_operations{0};
    std::atomic<uint64_t> aggregations{0};
    std::atomic<uint64_t> subqueries{0};
    std::atomic<uint64_t> memory_allocated_bytes{0};
};
```

**3. Storage Interaction Metrics**:
```cpp
struct StorageInteractionMetrics {
    std::atomic<uint64_t> series_selected{0};
    std::atomic<uint64_t> samples_processed{0};
    std::atomic<uint64_t> storage_query_time_ns{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> index_lookups{0};
    std::atomic<uint64_t> data_bytes_read{0};
};
```

**4. Query Execution Metrics**:
```cpp
struct QueryExecutionMetrics {
    std::atomic<uint64_t> total_queries{0};
    std::atomic<uint64_t> successful_queries{0};
    std::atomic<uint64_t> failed_queries{0};
    std::atomic<uint64_t> cancelled_queries{0};
    std::atomic<uint64_t> timeout_queries{0};
    std::atomic<uint64_t> total_execution_time_ns{0};
    std::atomic<uint64_t> result_size_bytes{0};
    std::atomic<uint64_t> concurrent_queries{0};
};
```

**5. Function-specific Metrics**:
```cpp
struct FunctionMetrics {
    std::unordered_map<std::string, std::atomic<uint64_t>> function_calls;
    std::unordered_map<std::string, std::atomic<uint64_t>> function_time_ns;
    std::atomic<uint64_t> rate_calculations{0};
    std::atomic<uint64_t> histogram_quantile_calls{0};
    std::atomic<uint64_t> aggregation_operations{0};
};
```

**Technical Implementation Details**:
- **Integration with AtomicMetrics**: Extend existing `tsdb::storage::internal::AtomicMetrics` for PromQL-specific metrics
- **OpenTelemetry Export**: Export metrics via existing OpenTelemetry integration (`src/tsdb/otel/`)
- **Performance Monitoring**: Use existing `core::PerformanceMetrics` patterns from semantic vector implementation
- **Thread Safety**: All metrics use atomic operations for thread-safe concurrent access
- **Memory Efficiency**: Use memory-mapped counters for high-frequency metrics

**Advanced Monitoring Features**:
- [ ] **Query Complexity Analysis**: Analyze AST depth, operation count, function complexity
- [ ] **Performance Prediction**: Predict query execution time based on historical data
- [ ] **Resource Usage Forecasting**: Predict memory and CPU usage for query planning
- [ ] **Anomaly Detection**: Detect unusual query patterns or performance degradation
- [ ] **Query Optimization Hints**: Suggest query optimizations based on execution patterns
- [ ] **Real-time Alerting**: Alert on performance thresholds, error rates, resource exhaustion

**Completion Criteria**:
- **Comprehensive Metrics**: All query lifecycle phases instrumented with detailed metrics
- **Real-time Visibility**: Dashboard provides real-time view of query performance
- **Performance Baselines**: Established baselines for different query types and complexity
- **Resource Monitoring**: Memory, CPU, and I/O usage tracked and optimized
- **Error Tracking**: All error conditions tracked with detailed context
- **Performance Regression Detection**: Automated detection of performance regressions
- **OpenTelemetry Integration**: Metrics exported via existing OTEL infrastructure
- **Production Ready**: Monitoring system ready for production deployment with alerts

---

**TASK 11.4: Update Planning Documentation - Phase 5 Complete**
**File**: `docs/planning/PROMQL_ENGINE_AGENT_TASKS.md`  
**Estimated Time**: 30 minutes  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Update Phase 5 status to "COMPLETED"
- [ ] Add completion date and testing summary
- [ ] Update overall progress (Phase 5/6 complete)
- [ ] Document test coverage and performance results
- [ ] Commit changes with detailed completion report

**Completion Criteria**:
- Phase 5 marked as completed
- Testing is comprehensive and passing
- Performance results documented

### **PHASE 6: Production Readiness (Week 12)**

#### **WEEK 12: Final Integration and Deployment**

**TASK 12.1: HTTP API Integration**
**File**: `src/tsdb/prometheus/api/query.h/.cpp`  
**Estimated Time**: 6 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Integrate PromQL engine with HTTP API endpoints
- [ ] Add comprehensive query parameter validation
- [ ] Implement proper response formatting
- [ ] Add detailed API documentation
- [ ] Test API compatibility with Grafana
- [ ] Add comprehensive error response handling

**Completion Criteria**:
- HTTP API integration is complete
- Parameter validation is comprehensive
- Response formatting matches Prometheus
- API documentation is complete
- Grafana compatibility validated
- Error responses are proper

---

**TASK 12.2: Configuration and Management**
**File**: `config/promql_config.yaml` (new)  
**File**: `src/tsdb/prometheus/promql/config.h/.cpp` (new)  
**Estimated Time**: 4 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Add PromQL engine configuration options
- [ ] Implement query limits and quotas
- [ ] Add administrative controls
- [ ] Create monitoring dashboards
- [ ] Add configuration validation
- [ ] Document all configuration options

**Completion Criteria**:
- Configuration system is complete
- Query limits work correctly
- Administrative controls are functional
- Monitoring dashboards are useful
- Configuration is well documented

---

**TASK 12.3: Documentation and User Guide**
**File**: `docs/user/PROMQL_USER_GUIDE.md` (new)  
**File**: `docs/api/PROMQL_API_REFERENCE.md` (new)  
**Estimated Time**: 6 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Create comprehensive user guide
- [ ] Write complete API reference documentation
- [ ] Add troubleshooting guide
- [ ] Create example queries and use cases
- [ ] Add performance tuning guide
- [ ] Create migration guide from other systems

**Completion Criteria**:
- User guide is comprehensive and helpful
- API reference is complete and accurate
- Troubleshooting guide covers common issues
- Examples are practical and useful
- Performance tuning guide is effective
- Migration guide helps users transition

---

**TASK 12.4: Final Validation and Production Deployment**
**File**: Multiple files  
**Estimated Time**: 6 hours  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Run complete test suite one final time
- [ ] Validate production readiness checklist
- [ ] Perform final compliance check
- [ ] Update all project documentation
- [ ] Create deployment guide
- [ ] Prepare release notes

**Completion Criteria**:
- All tests pass consistently
- Production readiness validated
- Compliance requirements met
- Documentation is complete and current
- Deployment guide is comprehensive
- Release notes are complete

---

**TASK 12.5: Update Planning Documentation - Phase 6 Complete**
**File**: `docs/planning/PROMQL_ENGINE_AGENT_TASKS.md`  
**Estimated Time**: 30 minutes  
**Status**: 🔴 Not Started

**Actions**:
- [ ] Update Phase 6 status to "COMPLETED"
- [ ] Add completion date and final summary
- [ ] Update overall progress (100% complete)
- [ ] Document final performance and compliance results
- [ ] Create project completion report
- [ ] Commit final changes with celebration message

**Completion Criteria**:
- Phase 6 marked as completed
- Project is 100% complete
- Final results documented
- Completion report created

## Success Criteria

### **Functional Requirements** ✅
- [ ] 100% PromQL syntax compatibility
- [ ] All 50+ PromQL functions implemented correctly
- [ ] Passes official PromQL compliance tests
- [ ] Supports all PromQL operators and modifiers
- [ ] Handles complex nested queries correctly

### **Performance Requirements** ✅
- [ ] Query execution time < 10s for complex queries
- [ ] Memory usage < 1GB for large result sets
- [ ] Supports >100 concurrent queries
- [ ] Query throughput > 1000 queries/second
- [ ] Efficient integration with TSDB storage

### **Quality Requirements** ✅
- [ ] >95% code coverage
- [ ] Zero critical bugs in production
- [ ] Comprehensive error handling
- [ ] Production-ready monitoring
- [ ] Complete documentation

## Progress Tracking

**Overall Progress**: 0/6 Phases Completed (0%)

- **Phase 1**: 🔴 Not Started - Parser Completion (Weeks 1-2)
- **Phase 2**: 🔴 Not Started - Query Engine Core (Weeks 3-4)
- **Phase 3**: 🔴 Not Started - Functions Library (Weeks 5-7)
- **Phase 4**: 🔴 Not Started - Advanced Features (Weeks 8-9)
- **Phase 5**: 🔴 Not Started - Testing & Compliance (Weeks 10-11)
- **Phase 6**: 🔴 Not Started - Production Readiness (Week 12)

## 🔧 **Integration Specifics**

### **File Organization**
- **Location**: All PromQL engine-specific code will reside in `src/tsdb/prometheus/promql/`
- **Tests**: Unit and integration tests in `src/tsdb/prometheus/tests/`
- **Entry Point**: The `QueryHandler` (`src/tsdb/prometheus/api/query.cpp`) will be primary consumer of `PromQLEngine`

### **Data Fetching Strategy**
- **QueryExecutor** will interact with `Storage` interface defined in `include/tsdb/storage/storage.h`
- Parse `VectorSelectorNode`s and `MatrixSelectorNode`s from query plan to extract:
  - Label matchers and time ranges
  - **Offsets and lookback deltas**
  - @ modifier timestamps
- These will be used to query the `Storage` layer with potential interface enhancement

### **Dependencies and Minimal External Requirements**
- **Core Dependencies**: Core TSDB types (`core::Label`, `core::Sample`, `core::TimeSeries`, `core::Result`, `core::Error`)
- **Storage Interface**: Existing `Storage` interface and implementations
- **Minimal External**: Should have minimal external dependencies beyond core TSDB components
- **Integration**: Leverage existing advanced performance features (cache hierarchy, object pools, compression)

## Agent Instructions

### **Getting Started**
1. Read this complete task list and the design analysis document
2. Set up development environment and familiarize with existing code
3. **Study Prometheus reference implementation** in GitHub repository for detailed behavior
4. Start with Phase 1, Task 1.1 (Update Planning Documentation)
5. Complete tasks in order, updating progress after each task
6. Run tests frequently and maintain high code quality

### **Development Guidelines**
- Follow existing code style and patterns in the project
- **Reference Prometheus implementation** for exact behavior matching
- Write comprehensive unit tests for every function and method
- Document all public interfaces with clear examples
- Profile performance-critical code paths
- **Test against known input data and expected outputs** from Prometheus
- Update progress in this planning document after each task

### **Critical Implementation Notes**
- **Status Tracking**: Update task status in this document after each completion
- **Prometheus Compliance**: Validate behavior against Prometheus reference implementation
- **Performance Targets**: Query execution <10s, memory <1GB, >100 concurrent queries
- **Error Handling**: Use `core::Result` pattern consistently throughout
- **Testing Strategy**: Unit tests, integration tests, and official PromQL compliance tests

### **Communication and Coordination**
- This agent works independently on PromQL engine implementation
- No conflicts with other agents (Semantic Vector, StorageImpl)
- Update progress in planning documents for visibility
- Report any blockers or unexpected dependencies
- **Coordinate with storage team** if `Storage` interface changes needed for label-based selection

---

**Document Status**: Ready for Agent 3  
**Last Updated**: September 2025  
**Total Tasks**: 72 tasks across 6 phases  
**Estimated Duration**: 12 weeks  
**Agent Assignment**: PromQL Engine Agent (Agent 3)
