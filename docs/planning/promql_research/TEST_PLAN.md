# PromQL Test Plan

**Document**: `docs/planning/promql_research/TEST_PLAN.md`
**Created**: November 2025
**Last Updated**: November 27, 2025
**Purpose**: Comprehensive test plan for PromQL implementation (Unit, Integration, E2E)
**Status**: ✅ ALL TESTS PASSING

## 1. Testing Strategy Overview

We employed a pyramid testing strategy:
- **Unit Tests (94%)**: Test individual functions, parser rules, and engine components in isolation.
- **Integration Tests (6%)**: Test the interaction between Parser -> Engine -> Storage.
- **End-to-End (E2E) Tests**: Test the full HTTP API surface and compliance with Prometheus standards.

## 2. Unit Testing Plan

### 2.1 Parser Tests
**Location**: `src/tsdb/prometheus/tests/promql_parser_test.cpp`
**Target**: `test-parser`

- [x] **Lexer Tests**: Verify all tokens are correctly identified.
- [x] **Parser Syntax Tests**:
    - Valid queries: `sum(rate(metric[5m]))`
    - Invalid queries: `sum(rate(metric[5m])` (missing paren)
    - Operator precedence: `a + b * c` -> `a + (b * c)`
- [x] **AST Verification**: Check that the AST structure matches expectations.

### 2.2 Engine Unit Tests
**Location**: `src/tsdb/prometheus/tests/promql_engine_test.cpp`
**Target**: `test-unit`

- [x] **Evaluator Tests**:
    - Mock storage adapter.
    - Test binary operations: `1 + 1 = 2`, `vector + scalar`, `vector + vector`.
    - Test aggregations: `sum`, `avg` on static data.
- [x] **Function Tests**:
    - Individual test cases for EACH of the 87 functions.
    - Edge cases: NaN, Inf, empty vectors, counter resets.

### 2.3 Storage Adapter Tests
**Location**: `src/tsdb/prometheus/tests/storage_adapter_test.cpp`

- [x] **Label Matching**: Test `=`, `!=`, `=~`, `!~` against in-memory data.
- [x] **Series Selection**: Verify correct series are returned.

## 3. Integration Testing Plan

### 3.1 Engine + Storage Integration
**Location**: `test/comprehensive_promql/`
**Target**: `comprehensive_promql_test_harness`

- [x] **Real Storage Test**:
    - Write data to actual TSDB storage.
    - Execute PromQL queries via Engine.
    - Verify results match written data.
- [x] **Complex Queries**:
    - Test nested functions: `sum(rate(http_requests_total[5m])) by (job)`
    - Test range queries over time.

### 3.2 Concurrency Tests
- [x] Run multiple concurrent queries against the engine.
- [x] Verify thread safety and performance.

## 4. End-to-End (E2E) Testing Plan

### 4.1 HTTP API Tests
**Location**: `test/e2e/promql_api_test.py`

- [x] **Query Endpoint**: `curl localhost:9090/api/v1/query?query=up`
- [x] **Range Query Endpoint**: Verify JSON structure matches Prometheus exactly.
- [x] **Error Handling**: Verify 4xx/5xx responses for bad queries.

### 4.2 Compliance Testing
- [x] **Prometheus Compliance Suite**:
    - Use the official [Prometheus Compliance Test Suite](https://github.com/prometheus/compliance).
    - Configure it to point to MyTSDB.
    - Run tests and generate report.

## 5. Directory Structure for Tests (Implemented)

```
test/comprehensive_promql/
├── CMakeLists.txt
├── test_harness.cpp          # Main entry point
├── test_fixture.h            # Common test fixture with storage setup
├── selector_tests.cpp        # Selector and Matcher tests
├── aggregation_tests.cpp     # Aggregation operator tests
├── function_tests.cpp        # PromQL function tests
├── binary_operator_tests.cpp # Binary operator tests
├── subquery_tests.cpp        # Subquery tests
└── complex_scenarios_tests.cpp # Complex real-world scenarios
```

## 6. Test Status ✅ COMPLETE

- **Unit Tests**: 851 tests in `test/unit`
- **Integration Tests**: 51 tests in `test/comprehensive_promql`
- **Total Coverage**: 902 tests
- **Pass Rate**: 100%

## 7. Makefile / CMake Changes

- **CMakeLists.txt**:
    - Add new test executables for engine and functions.
    - Link against `tsdb_lib` and `gtest`.
- **Makefile**:
    - Add `test-promql-engine` target.
    - Add `test-promql-compliance` target.
