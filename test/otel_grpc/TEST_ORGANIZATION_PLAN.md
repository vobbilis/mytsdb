# OTEL/gRPC Test Organization Plan

**Date:** 2025-01-23  
**Status:** ✅ Implemented

## Overview

This document outlines the organization of all OTEL/gRPC related tests in the `test/otel_grpc/` directory.

## Directory Structure

```
test/otel_grpc/
├── README.md                    # Overview and usage guide
├── TEST_ORGANIZATION_PLAN.md    # This document
├── CMakeLists.txt               # Main CMake configuration
├── e2e/                         # End-to-end tests
│   ├── CMakeLists.txt
│   └── grpc_server_e2e_test.cpp # E2E test that starts actual server
├── integration/                 # Integration tests
│   └── CMakeLists.txt
├── unit/                        # Unit tests
│   └── CMakeLists.txt
└── performance/                 # Performance benchmarks
    └── CMakeLists.txt
```

## Test Categories

### 1. E2E Tests (`e2e/`)

**Purpose:** Test the actual gRPC server by starting it and making real gRPC calls.

**Current Tests:**
- `grpc_server_e2e_test.cpp`: Comprehensive E2E test suite
  - Server startup and shutdown
  - Single metric export
  - Multiple metrics export
  - Graceful shutdown
  - Concurrent exports

**How it works:**
- Uses `fork()` to start the server as a subprocess
- Waits for server to be ready by attempting connections
- Makes real gRPC calls via `MetricsService::Stub`
- Cleans up server process on test completion

**Running:**
```bash
make test-otel-grpc-e2e
```

### 2. Integration Tests (`integration/`)

**Purpose:** Test the integration between OTEL bridge, gRPC service, and storage.

**Status:** Directory ready for future tests. Existing integration tests are in `test/integration/`:
- `otel_integration_test.cpp`
- `grpc_integration_test.cpp`

**Future tests:**
- OTEL metric conversion accuracy
- gRPC service error handling
- Bridge implementation correctness
- Cross-component integration

### 3. Unit Tests (`unit/`)

**Purpose:** Unit tests for individual OTEL/gRPC components.

**Status:** Directory ready for future tests.

**Future tests:**
- Bridge conversion logic
- MetricsService methods
- Protocol buffer handling
- Error handling in isolation

### 4. Performance Tests (`performance/`)

**Purpose:** Performance benchmarks for OTEL/gRPC write operations.

**Status:** Directory ready. Will be implemented in Phase 2.

**Planned tests:**
- Write throughput via OTEL/gRPC
- Latency measurements (P50, P95, P99)
- Batch processing performance
- Concurrent write performance
- Comparison with direct writes

## CMake Integration

### Main Test CMakeLists.txt

The `test/CMakeLists.txt` includes the OTEL/gRPC test directory:
```cmake
if(HAVE_GRPC)
    add_subdirectory(otel_grpc)
endif()
```

### Test Targets

**Available targets:**
- `make test-otel-grpc-e2e` - Run E2E tests only
- `make test-otel-grpc` - Run all OTEL/gRPC tests

**Test results:**
- XML reports: `build/test-results/otel-grpc-e2e/e2e_tests.xml`
- Logs: Test output in console

## Testing Workflow

### 1. Build the Server

```bash
mkdir -p build && cd build
cmake ..
make tsdb_server
```

### 2. Build the Tests

```bash
cd build
make tsdb_grpc_server_e2e_test
```

### 3. Run E2E Tests

```bash
# Run via CMake target
make test-otel-grpc-e2e

# Or run directly
./test/otel_grpc/e2e/tsdb_grpc_server_e2e_test
```

### 4. Manual Server Testing

```bash
# Start server manually
./build/src/tsdb/tsdb_server --address 0.0.0.0:4317 --data-dir /tmp/tsdb

# In another terminal, use OTEL client to send metrics
# (See examples/client.cpp for reference)
```

## Test Data Management

- **Temporary directories:** Each test creates a temporary directory for server data
- **Cleanup:** Directories are automatically cleaned up after tests
- **Isolation:** Each test uses a unique port (based on PID) to avoid conflicts

## Future Enhancements

1. **Integration Tests:**
   - Move existing OTEL/gRPC integration tests here
   - Add more comprehensive integration scenarios

2. **Unit Tests:**
   - Add unit tests for bridge conversion
   - Test MetricsService in isolation
   - Test protocol buffer handling

3. **Performance Tests:**
   - Implement performance benchmarks (Phase 2)
   - Compare OTEL/gRPC vs direct writes
   - Measure overhead

4. **Test Infrastructure:**
   - Add test fixtures for common scenarios
   - Create helper utilities for test setup
   - Add test data generators

## Notes

- All tests are conditionally compiled based on `HAVE_GRPC`
- E2E tests use Unix-specific features (`fork()`, `waitpid()`) - may need Windows alternatives
- Server executable must be built before running E2E tests
- Tests use temporary directories that are cleaned up automatically

