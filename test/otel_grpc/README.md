# OTEL/gRPC Testing Directory

This directory contains all tests related to OpenTelemetry (OTEL) and gRPC functionality.

## Directory Structure

```
otel_grpc/
├── integration/     # Integration tests for OTEL bridge and gRPC service
├── e2e/            # End-to-end tests that start actual gRPC server
├── unit/           # Unit tests for OTEL/gRPC components
└── performance/    # Performance benchmarks for OTEL/gRPC writes
```

## Test Categories

### Integration Tests (`integration/`)
Tests that verify the integration between OTEL bridge, gRPC service, and storage:
- OTEL metric conversion
- gRPC service functionality
- Bridge implementation
- Error handling

### End-to-End Tests (`e2e/`)
Tests that start the actual TSDB gRPC server and test it via real gRPC connections:
- Server startup and shutdown
- Real gRPC client connections
- Metric ingestion via OTEL/gRPC
- Server health checks
- Concurrent client connections

### Unit Tests (`unit/`)
Unit tests for individual OTEL/gRPC components:
- Bridge conversion logic
- MetricsService methods
- Protocol buffer handling

### Performance Tests (`performance/`)
Performance benchmarks for OTEL/gRPC write operations:
- Write throughput
- Latency measurements
- Batch processing performance
- Concurrent write performance

## Running Tests

### All OTEL/gRPC Tests
```bash
make test-otel-grpc
```

### Specific Test Categories
```bash
make test-otel-grpc-integration  # Integration tests
make test-otel-grpc-e2e          # End-to-end tests
make test-otel-grpc-unit         # Unit tests
make test-otel-grpc-performance  # Performance tests
```

## Test Data

Test data is stored in temporary directories that are cleaned up after tests complete.
For e2e tests, the server uses a temporary data directory.

