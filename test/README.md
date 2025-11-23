# MyTSDB Test Suite

This directory contains the complete test suite for MyTSDB.

---

## 📁 Directory Structure

```
test/
├── unit/           # Unit tests (357 tests)
│   ├── core/       # Core type tests
│   ├── histogram/  # Histogram tests
│   └── storage/    # Storage component tests
├── integration/    # Integration tests (177 tests)
│   └── storageimpl_phases/  # Phase-based integration tests
├── benchmark/      # Performance benchmarks
├── stress/         # Stress testing
├── scalability/    # Scalability testing
└── load/           # Load testing
```

---

## 🚀 Running Tests

### All Tests

```bash
cd build
make test
```

### By Category

```bash
# Unit tests only
make test-unit

# Integration tests only
make test-integration

# Performance benchmarks
make test-all-performance
```

### Specific Test Suites

```bash
cd build

# Core tests
./test/unit/tsdb_core_unit_tests

# Storage tests
./test/unit/tsdb_storage_unit_tests

# Cache hierarchy tests
./test/unit/tsdb_cache_hierarchy_unit_tests

# Integration tests
./test/integration/tsdb_integration_test_suite

# With gtest filter (run specific tests)
./test/unit/tsdb_storage_unit_tests --gtest_filter=StorageTest.*
```

---

## 📊 Test Status

### Unit Tests (357 tests)

```
✅ Core Tests (38/38)
   - ResultTest (14/14)
   - ErrorTest (11/11)
   - StorageConfigTest (5/5)
   - GranularityTest (2/2)
   - Others (6/6)

✅ Storage Tests (59/59)
   - BasicWriteAndRead ✅
   - MultipleSeries ✅
   - LabelOperations ✅
   - DeleteSeries ✅
   - HighFrequencyData ✅
   - ConcurrentOperations ✅
   - CompactionAndFlush ✅
   - 1 test disabled for specific testing

✅ Cache Hierarchy (28/28)
   - L1/L2/L3 operations
   - LRU eviction
   - Concurrent access
   - Statistics tracking

✅ Predictive Cache (20/20)
   - Pattern detection
   - Prefetching
   - Confidence scoring
   - Adaptive behavior

✅ Background Processor (29/29)
   - Task execution
   - Priority ordering
   - Error handling
   - Graceful shutdown

✅ Object Pools (passing)
   - TimeSeriesPool
   - LabelsPool
   - SamplePool
   - 99%+ reuse rate

✅ Compression (passing)
   - Gorilla compression
   - Delta-of-Delta encoding
   - XOR compression
   - RLE compression

✅ Other Components (passing)
   - Lock-Free Queue (13/13)
   - Atomic Metrics (passing)
   - Performance Config (passing)
   - Working Set Cache (passing)
   - Atomic Ref Counted (11/11)
```

### Integration Tests (177 tests)

```
✅ Phase 1: Object Pool Integration (passing)
✅ Phase 2: Cache & Background Processing (27/27)
   - Cache hierarchy integration
   - Block management
   - Background processing
   - Predictive caching
✅ Phase 3: Compression Integration (passing)
✅ Config Integration (8/8)
✅ Core Storage Integration (3/3)
✅ End-to-End Workflow (7/7)
✅ Multi-Component Tests (passing)
✅ Error Handling Tests (passing)
```

### Known Issues

```
⚠️ L2 Cache Disabled - Segfaults in production, disabled in constructor
⚠️ Background Processing Disabled by Default - For testing stability
⚠️ Some tests may hang in full suite - Individual execution works
```

---

## 🎯 Test Categories

### 1. Unit Tests

**Purpose:** Test individual components in isolation

**Location:** `test/unit/`

**Examples:**
- `tsdb_core_unit_tests` - Core types and result handling
- `tsdb_storage_unit_tests` - Storage operations
- `tsdb_cache_hierarchy_unit_tests` - Cache operations
- `tsdb_object_pool_unit_tests` - Object pool functionality

**Run:** `make test-unit` or `./test/unit/<test_name>`

### 2. Integration Tests

**Purpose:** Test component interactions and workflows

**Location:** `test/integration/`

**Examples:**
- `tsdb_integration_test_suite` - Main integration suite (124 tests)
- `tsdb_core_storage_integration_test` - Core-storage integration
- `tsdb_end_to_end_workflow_test` - Complete workflows

**Run:** `make test-integration`

### 3. Performance Benchmarks

**Purpose:** Measure performance characteristics

**Location:** `test/benchmark/`

**Examples:**
- `tsdb_performance_test` - Main performance benchmark
- `tsdb_wal_performance_benchmark` - WAL performance
- `tsdb_memory_efficiency_performance_test` - Memory efficiency
- `tsdb_indexing_performance_test` - Index performance
- `tsdb_concurrency_performance_test` - Concurrency performance

**Run:** `make test-all-performance`

### 4. Stress Tests

**Purpose:** Test system under heavy load

**Location:** `test/stress/`

**Run:** `make test-stress`

### 5. Scalability Tests

**Purpose:** Test scaling characteristics

**Location:** `test/scalability/`

**Run:** `make test-scalability`

---

## 🔧 Test Utilities

### Running with CTest Timeout

Use ctest with timeout to prevent hanging tests:

```bash
# Run all tests with 30-second timeout per test
ctest --timeout 30

# Run specific test with timeout
ctest -R StorageTest --timeout 30 --verbose

# Run tests matching pattern
ctest -R "Phase.*" --timeout 60
```

### Test Filters

Run specific tests using gtest filters:

```bash
# Run specific test
./test/unit/tsdb_storage_unit_tests --gtest_filter=StorageTest.BasicWriteAndRead

# Run test pattern
./test/unit/tsdb_storage_unit_tests --gtest_filter='StorageTest.*'

# Exclude tests
./test/unit/tsdb_storage_unit_tests --gtest_filter=-StorageTest.ErrorConditions
```

### Verbose Output

```bash
# Enable verbose output
./test/unit/tsdb_storage_unit_tests --gtest_verbose=1

# List all tests
./test/unit/tsdb_storage_unit_tests --gtest_list_tests
```

---

## 📈 Performance Benchmarks

### Current Baselines

```
Write Performance:       ~10K ops/sec (single-threaded)
Read Performance:        ~50K ops/sec (cached)
Object Pool Reuse:       99%+ (measured)
Cache Hit Ratio:         80-90% (hot data)
Compression Ratio:       4-6x (typical)
```

### Running Benchmarks

```bash
cd build

# All performance tests
make test-all-performance

# Specific benchmarks
./test/benchmark/tsdb_performance_test
./test/benchmark/tsdb_wal_performance_benchmark
./test/benchmark/tsdb_memory_efficiency_performance_test
./test/benchmark/tsdb_indexing_performance_test
./test/benchmark/tsdb_concurrency_performance_test
```

---

## 🐛 Debugging Failed Tests

### 1. Run Test with Valgrind

```bash
valgrind --leak-check=full ./test/unit/tsdb_storage_unit_tests
```

### 2. Run Test with GDB

```bash
gdb ./test/unit/tsdb_storage_unit_tests
(gdb) run
(gdb) bt  # backtrace on crash
```

### 3. Enable Debug Logging

```bash
# Set environment variable
export SPDLOG_LEVEL=debug
./test/unit/tsdb_storage_unit_tests
```

### 4. Run with AddressSanitizer

```bash
# Rebuild with sanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
make

# Run test
./test/unit/tsdb_storage_unit_tests
```

### 5. Check Test Logs

```bash
# Test results directory
ls build/test-results/

# View test output
cat build/test-results/unit/unit_tests.xml
```

---

## 📝 Writing New Tests

### Unit Test Template

```cpp
#include <gtest/gtest.h>
#include "tsdb/storage/your_component.h"

TEST(YourComponentTest, BasicFunctionality) {
    // Arrange
    YourComponent component;
    
    // Act
    auto result = component.do_something();
    
    // Assert
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), expected_value);
}
```

### Integration Test Template

```cpp
#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"

TEST(YourIntegrationTest, EndToEndWorkflow) {
    // Setup
    core::StorageConfig config = core::StorageConfig::Default();
    StorageImpl storage(config);
    
    // Write
    core::TimeSeries series = create_test_series();
    auto write_result = storage.write(series);
    ASSERT_TRUE(write_result.ok());
    
    // Read
    auto read_result = storage.read(series.labels(), 0, now());
    ASSERT_TRUE(read_result.ok());
    
    // Verify
    EXPECT_EQ(read_result.value().size(), 1);
}
```

### Adding Tests to CMake

Edit `test/CMakeLists.txt`:

```cmake
add_executable(your_test_name
    ${CMAKE_SOURCE_DIR}/test/unit/your/test_file.cpp
)
target_link_libraries(your_test_name PRIVATE tsdb_test_common)
add_test(NAME YourTestName COMMAND your_test_name)
```

---

## 🎓 Best Practices

### 1. Test Naming

```cpp
// Good: Descriptive and specific
TEST(StorageImplTest, WriteTimeSeriesWithMultipleSamples)

// Bad: Vague
TEST(StorageImplTest, Test1)
```

### 2. Test Isolation

```cpp
// Good: Each test is independent
TEST(StorageImplTest, WriteOperation) {
    StorageImpl storage;  // Fresh instance
    // ...
}

// Bad: Tests depend on each other
static StorageImpl* global_storage;  // Shared state
```

### 3. Clear Assertions

```cpp
// Good: Clear assertion messages
ASSERT_TRUE(result.ok()) << "Write failed: " << result.error();

// Bad: Silent failures
ASSERT_TRUE(result.ok());
```

### 4. Test Data

```cpp
// Good: Helper functions for test data
core::TimeSeries create_test_series(const std::string& name) {
    core::Labels labels;
    labels.add("__name__", name);
    return core::TimeSeries(labels);
}

// Bad: Inline test data everywhere
```

---

## 📚 Additional Resources

- **[Comprehensive Test Plan](../docs/planning/STORAGEIMPL_COMPREHENSIVE_TEST_PLAN.md)** - Master test plan
- **[Integration Testing Plan](../docs/planning/INTEGRATION_TESTING_PLAN.md)** - Integration test strategy
- **[Performance Testing Plan](../docs/planning/COMPREHENSIVE_PERFORMANCE_TESTING_PLAN.md)** - Performance test strategy
- **[Google Test Documentation](https://google.github.io/googletest/)** - GTest framework

---

**Last Updated:** November 18, 2025  
**Test Suite Version:** 1.0  
**Total Tests:** 632+ test cases
