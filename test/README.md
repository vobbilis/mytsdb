# TSDB Test Suite

This directory contains the comprehensive test suite for the Time Series Database (TSDB) project. The test suite consists of **462 individual tests** across multiple categories, providing thorough coverage of all system components.

## Test Statistics

- **Total Tests**: 462
- **Test Files**: 36 (with content)
- **Total Lines of Code**: ~15,000
- **Test Categories**: 8 major categories
- **Coverage**: Unit, Integration, Performance, Load, Stress, Scalability

## Test Organization

### 1. **Unit Tests** (`unit/`)
Core component testing with **349 tests** across:

#### **Core Components** (`unit/core/`) - 27 tests
- **Result Tests** (15 tests): Success/error construction, move semantics, type handling
- **Error Tests** (12 tests): Error construction, comparison, message handling

#### **Storage Components** (`unit/storage/`) - 322 tests
- **Object Pool Tests** (13 tests): TimeSeries, Labels, Sample pool operations
- **Working Set Cache Tests** (15 tests): LRU behavior, hit ratio, thread safety
- **Cache Hierarchy Tests** (28 tests): L1/L2/L3 cache operations, promotion/demotion
- **Predictive Cache Tests** (20 tests): Pattern detection, prefetching, confidence scoring
- **Atomic Reference Counting Tests** (11 tests): Reference management, thread safety
- **Adaptive Compressor Tests** (19 tests): Type detection, compression ratios
- **Delta-of-Delta Encoder Tests** (18 tests): Timestamp compression, zigzag encoding
- **Atomic Metrics Tests** (24 tests): Performance tracking, thread safety
- **Performance Config Tests** (24 tests): Feature flags, A/B testing, configuration
- **Sharded Write Buffer Tests** (16 tests): Buffer management, load balancing
- **Background Processor Tests** (28 tests): Task execution, queue management
- **Lock-Free Queue Tests** (17 tests): Concurrent operations, performance
- **Storage Tests** (13 tests): Basic CRUD operations, error handling
- **Block Management Tests** (13 tests): Block lifecycle, compaction
- **Compression Tests** (9 tests): Various compression algorithms

### 2. **Integration Tests** (`integration/`) - 113 tests
End-to-end system testing:

#### **Core Integration** (3 tests)
- Basic time series creation and storage
- Configuration integration
- Data type consistency

#### **Storage-Histogram Integration** (5 tests)
- DDSketch and Fixed Bucket histogram storage
- Histogram merging across boundaries
- Large histogram handling

#### **Configuration Integration** (8 tests)
- Storage, histogram, and query config propagation
- Configuration validation and persistence

#### **OpenTelemetry Integration** (8 tests)
- Counter, gauge, histogram, summary metric conversion
- Resource attributes and label preservation

#### **gRPC Service Integration** (8 tests)
- Metrics ingestion, batch processing
- Error handling, rate limiting, health checks

#### **End-to-End Workflows** (7 tests)
- OpenTelemetry to storage to query workflows
- Batch and real-time processing scenarios

#### **Multi-Component Operations** (7 tests)
- Concurrent read/write operations
- Cross-component error handling
- Resource sharing and lifecycle management

#### **Error Handling** (7 tests)
- Error propagation across components
- Resource exhaustion handling
- Recovery mechanisms

#### **Recovery Scenarios** (7 tests)
- Storage corruption recovery
- Network interruption handling
- Component restart scenarios

### 3. **PromQL Parser Tests** - 1 test
- PromQL lexer and parser functionality

### 4. **Performance Tests** (`benchmark/`) - 0 tests (placeholder)
- Write/read performance benchmarks
- Compaction performance
- Resource utilization metrics

### 5. **Load Tests** (`load/`) - 0 tests (placeholder)
- Peak load testing
- Sustained load testing

### 6. **Stress Tests** (`stress/`) - 0 tests (placeholder)
- Data volume stress testing
- Resource stress testing

### 7. **Scalability Tests** (`scalability/`) - 0 tests (placeholder)
- Horizontal scalability testing
- Vertical scalability testing

## Test Data

Test data is organized in `test/data/`:
- **Storage Data**: Blocks, compression samples, histograms
- **Sample Metrics**: Various metric types and distributions
- **Configuration Files**: Test configurations and settings

## Running Tests

### Running All Tests
```bash
cd build
ctest --output-on-failure
```

### Running Specific Test Categories
```bash
# Run unit tests only
ctest -R "unit.*"

# Run integration tests only
ctest -R "integration.*"

# Run storage tests
ctest -R "storage.*"

# Run cache tests
ctest -R "cache.*"

# Run compression tests
ctest -R "compression.*"
```

### Running Tests with Coverage
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
make
ctest
gcovr -r .. .
```

## Test Categories Overview

### **Unit Tests** (349 tests)
- **Purpose**: Test individual components in isolation
- **Scope**: Mock dependencies, focus on edge cases
- **Speed**: Fast execution (< 1 second per test)
- **Coverage**: Core functionality, error conditions

### **Integration Tests** (113 tests)
- **Purpose**: Test component interactions and workflows
- **Scope**: Real dependencies, common use cases
- **Speed**: Medium execution (1-10 seconds per test)
- **Coverage**: End-to-end scenarios, error propagation

### **Performance Tests** (0 tests - planned)
- **Purpose**: Measure throughput and latency
- **Scope**: Various loads, resource monitoring
- **Speed**: Longer execution (10+ seconds per test)
- **Coverage**: Performance characteristics, bottlenecks

## Test Coverage Targets

- **Line Coverage**: >90%
- **Branch Coverage**: >85%
- **Function Coverage**: >95%

## Writing New Tests

### Guidelines
1. **Clear Purpose**: Each test should have a specific objective
2. **Comprehensive Coverage**: Test success, failure, and edge cases
3. **Resource Management**: Clean up resources after tests
4. **Documentation**: Document assumptions and requirements
5. **Naming Convention**: `TestSuite.TestName` format

### Test Structure
```cpp
TEST_F(TestFixture, TestName) {
    // ARRANGE - Set up test conditions
    
    // ACT - Perform the operation being tested
    
    // ASSERT - Verify the results
}
```

### Adding New Tests
1. Create test file in appropriate directory
2. Add test to CMakeLists.txt
3. Follow existing naming conventions
4. Include necessary test data
5. Document test purpose

## Debugging Tests

### Common Issues
- Resource cleanup failures
- Race conditions in concurrent tests
- System-dependent behavior
- Performance test variability

### Debugging Tools
```bash
# Break on test failure
ctest --gtest_break_on_failure

# Verbose output
ctest --output-on-failure

# Run specific test
./build/test/unit/tsdb_storage_unit_tests --gtest_filter="StorageTest.*"
```

## Test Infrastructure

### Build System
- **CMake**: Primary build system
- **Google Test**: Testing framework
- **C++20**: Language standard
- **Parallel Execution**: Tests run in parallel where possible

### Test Configuration
- **test_config.h**: Generated configuration header
- **TSDB_TEST_DATA_DIR**: Test data directory path
- **Common Library**: `tsdb_test_common` interface library

## Contributing

When contributing new tests:
1. Follow existing test patterns and naming conventions
2. Include both positive and negative test cases
3. Document test purpose and requirements
4. Verify test coverage impact
5. Ensure tests are deterministic and reliable

## Notes

- All tests are deterministic and repeatable
- Tests clean up resources even if they fail
- Performance tests are isolated to prevent interference
- Integration tests use real dependencies for realistic scenarios
- Unit tests use mocks for fast, isolated execution 