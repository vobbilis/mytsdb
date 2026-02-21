# CLAUDE.md - MyTSDB Development Guide

## Project Overview

MyTSDB is a high-performance, Prometheus-compatible time series database written in C++17. It features object pooling, multi-tier caching, predictive prefetching, Apache Parquet cold storage, and a full PromQL query engine. The server exposes HTTP (Prometheus-compatible), gRPC (OpenTelemetry OTLP), and Apache Arrow Flight interfaces.

## Build System

### Prerequisites

- **Required:** C++17 compiler (GCC 9+, Clang 10+), CMake 3.15+, OpenSSL
- **Optional:** Intel TBB 2021.0+, spdlog 1.9.0+, Google Test 1.11.0+, gRPC + Protobuf, httplib, RapidJSON, Snappy, Apache Arrow/Parquet

Install all dependencies:
```bash
make deps          # Auto-detects OS (macOS via Homebrew, Linux via apt/dnf)
```

### Building

All builds go through the root Makefile which wraps CMake:

```bash
make                # Configure + build (Release mode)
make configure      # CMake configuration only
make build          # Build only (runs configure first)
make rebuild        # Clean everything and rebuild from scratch
make clean          # Clean build artifacts, keep CMake cache
make clean-all      # Remove entire build/ directory
```

**Build directory:** `build/`

**CMake flags** (set in Makefile line 14):
```
-DCMAKE_BUILD_TYPE=Release
-DBUILD_TESTS=ON
-DTSDB_SEMVEC=OFF
-DENABLE_PARQUET=ON
-DENABLE_OTEL=ON
-DHAVE_GRPC=ON
```

Key CMake options:
- `BUILD_TESTS` — Enable test targets (ON by default in Makefile)
- `ENABLE_SIMD` — AVX-512 auto-detection (ON by default)
- `ENABLE_OTEL` — OpenTelemetry SDK support
- `TSDB_SEMVEC` — Semantic vector components (OFF by default, experimental)
- `ENABLE_PARQUET` — Apache Parquet cold storage
- `HAVE_GRPC` — gRPC support for OTLP ingestion

## Testing

**Framework:** Google Test (GTest). All test commands auto-build first.

### Quick Reference

```bash
make test-unit                # Unit tests (357 tests, 30s timeout) - run this first
make test-integration         # Integration tests (177 tests, 60s timeout)
make test-all                 # All tests (632+ tests, 120s timeout)
```

### Targeted Test Suites

```bash
make test-core-unit           # Core types/config tests (38 tests)
make test-storage-unit        # Storage engine tests (60 tests)
make test-cache-unit          # Cache hierarchy tests (28 tests)
make test-compression-unit    # Compression algorithm tests (19 tests)
make test-histogram-unit      # Histogram tests (22 tests)
make test-parser              # PromQL parser tests (34 tests)
make test-main-integration    # Main integration suite (124 tests)
make test-storageimpl-phases  # StorageImpl lifecycle tests (64 tests)
make test-individual-integration  # Individual integration tests (53 tests)
make test-promql-full         # Comprehensive PromQL tests (with data gen)
make test-otel-grpc           # OpenTelemetry/gRPC tests
```

### Performance Tests

```bash
make test-wal-performance         # WAL benchmarks (6 tests)
make test-memory-efficiency       # Memory efficiency (6 tests)
make test-indexing-performance    # Indexing benchmarks (8 tests)
make test-concurrency-performance # Concurrency benchmarks (8 tests)
make test-all-performance         # All performance tests (28 tests)
```

### Serial Mode (for debugging flaky tests)

Append `-serial` to run with `CTEST_PARALLEL_LEVEL=1`:
```bash
make test-all-serial
make test-unit-serial
make test-integration-serial
```

## Running the Server

```bash
make server-start-clean   # Build + start with clean data dir (/tmp/tsdb_data)
make server-stop          # Stop running server
```

Server ports: HTTP 9090, Arrow Flight 8815, gRPC/OTLP 4317

### Benchmarks

```bash
make benchmark-quick      # Quick validation (~1M samples, 30s)
make benchmark-full       # Full benchmark (10M samples, 2min)
make benchmark-20m        # Enhanced benchmark (20M samples, 3min)
make benchmark-grpc       # gRPC/OTEL benchmark (20M samples)
make benchmark-negative   # Negative matcher workload
make benchmark-help       # List all benchmark targets
```

## Repository Structure

```
mytsdb/
├── include/tsdb/           # Public headers (API surface)
│   ├── core/               # Core types: Labels, Sample, TimeSeries, Result, Error
│   ├── storage/            # Storage engine interfaces and implementations
│   │   ├── internal/       # Internal block implementation details
│   │   ├── semantic_vector/# Semantic vector (experimental)
│   │   └── parquet/        # Parquet cold storage
│   ├── prometheus/         # Prometheus compatibility layer
│   │   ├── promql/         # PromQL parser, AST, engine, evaluator
│   │   ├── auth/           # Authentication strategies
│   │   ├── remote/         # Remote write/read handlers
│   │   ├── server/         # HTTP server
│   │   └── api/            # API handlers
│   ├── otel/               # OpenTelemetry bridge
│   ├── histogram/          # DDSketch and histogram support
│   ├── arrow/              # Arrow Flight server
│   └── query/              # Query interface
├── src/tsdb/               # Implementation source files
│   ├── main.cpp            # Server entry point
│   ├── storage/            # Storage engine (~50 .cpp files)
│   ├── prometheus/         # Prometheus layer implementations
│   ├── otel/               # OTEL bridge implementation
│   └── ...                 # Mirrors include/ structure
├── test/                   # Test suite (632+ tests)
│   ├── unit/               # Unit tests
│   ├── integration/        # Integration tests
│   ├── benchmark/          # Performance benchmarks
│   ├── comprehensive_promql/ # PromQL test suite
│   ├── otel_grpc/          # gRPC/OTEL tests
│   ├── stress/             # Stress tests
│   └── data/               # Test fixtures
├── common/proto/           # Protobuf definitions (alerts, metrics, remote)
├── config/config.yaml      # Runtime configuration
├── tools/                  # Benchmark and data generation tools
├── deployments/kubernetes/ # K8s manifests and Grafana dashboards
├── docs/                   # Extensive documentation (~100 files)
├── examples/               # Usage examples
├── otel-proto/             # OpenTelemetry proto submodule
├── CMakeLists.txt          # Root CMake configuration
└── Makefile                # Build wrapper (all commands go through here)
```

## Architecture

### Key Components

| Component | Header | Implementation | Purpose |
|-----------|--------|----------------|---------|
| StorageImpl | `storage/storage_impl.h` | `storage/storage_impl.cpp` | Main storage engine (3K+ LOC) |
| BlockManager | `storage/block_manager.h` | `storage/block_manager.cpp` | Block lifecycle management |
| CacheHierarchy | `storage/cache_hierarchy.h` | `storage/cache_hierarchy.cpp` | Multi-tier L1/L2/L3 caching |
| Compression | `storage/compression.h` | `storage/compression.cpp` | Gorilla, Delta-of-Delta, XOR, RLE |
| ObjectPool | `storage/object_pool.h` | `storage/object_pool.cpp` | Memory pooling for allocations |
| WAL | `storage/sharded_wal.h` | `storage/sharded_wal.cpp` | Sharded write-ahead log |
| Index | `storage/sharded_index.h` | `storage/sharded_index.cpp` | Sharded inverted + forward index |
| PromQL Engine | `prometheus/promql/engine.h` | `prometheus/promql/engine.cpp` | Full PromQL query engine |
| OTEL Bridge | `otel/bridge.h` | `otel/bridge_impl.cpp` | OpenTelemetry metric ingestion |
| BackgroundProcessor | `storage/background_processor.h` | `storage/background_processor.cpp` | Async compaction and cleanup |

### Storage Tiers

- **Hot tier:** In-memory blocks with lock-free concurrent access (Intel TBB)
- **Warm tier:** Sealed blocks pending compaction
- **Cold tier:** Apache Parquet files with secondary indexes and bloom filters

### Data Flow

1. **Write path:** Samples -> ShardedWAL -> Active Block -> Sealed Block -> Parquet (demotion)
2. **Read path:** Query -> CacheHierarchy -> Hot blocks -> Cold Parquet files (with aggregation pushdown)

## Code Conventions

### Namespaces

All code lives under nested namespaces matching the directory structure:
```cpp
namespace tsdb {
namespace storage { ... }    // Storage engine
namespace core { ... }       // Core types
namespace prometheus {
namespace promql { ... }     // PromQL engine
}
}
```

Tests use an additional `test` sub-namespace:
```cpp
namespace tsdb { namespace storage { namespace test { ... }}}
```

### Naming Conventions

- **Types/Classes:** `PascalCase` — `StorageImpl`, `BlockManager`, `CacheHierarchy`
- **Functions/Methods:** `snake_case` — `read_nolock()`, `create_test_series()`, `to_string()`
- **Member variables:** `snake_case_` with trailing underscore — `labels_`, `value_`, `error_msg_`
- **Constants/Enums:** `UPPER_SNAKE_CASE` — `UNKNOWN`, `INVALID_ARGUMENT`, `NOT_FOUND`
- **Type aliases:** `PascalCase` — `using SeriesID = uint64_t;`, `using Timestamp = int64_t;`
- **Macros:** `UPPER_SNAKE_CASE` — `TSDB_ENABLE_METRICS`

### Header Guards

Use `#ifndef` style (not `#pragma once`):
```cpp
#ifndef TSDB_STORAGE_STORAGE_IMPL_H_
#define TSDB_STORAGE_STORAGE_IMPL_H_
// ...
#endif
```

Pattern: `TSDB_<MODULE>_<FILENAME>_H_`

### Error Handling

The codebase uses a `Result<T>` type (not exceptions) for fallible operations:
```cpp
core::Result<void> write(const core::TimeSeries& series);
core::Result<core::TimeSeries> read(const core::Labels& labels, int64_t start, int64_t end);

// Usage:
auto result = storage.read(labels, start, end);
if (result.ok()) {
    auto& ts = result.value();
} else {
    LOG_ERROR("Read failed: {}", result.error());
}

// Creating errors:
return Result<T>::error("descriptive message");
```

Error hierarchy: `Error` (base) -> `InvalidArgumentError`, `NotFoundError`, `AlreadyExistsError`, `TimeoutError`, `ResourceExhaustedError`

### Include Style

Project headers use quoted paths from the project root:
```cpp
#include "tsdb/core/types.h"
#include "tsdb/storage/storage_impl.h"
```

System/library headers use angle brackets:
```cpp
#include <memory>
#include <tbb/concurrent_hash_map.h>
#include <gtest/gtest.h>
```

### Test Patterns

Tests use Google Test with test fixtures:
```cpp
class MyFeatureTest : public ::testing::Test {
protected:
    void SetUp() override { ... }
    void TearDown() override { ... }
};

TEST_F(MyFeatureTest, DescriptiveName) {
    EXPECT_EQ(actual, expected);
    ASSERT_TRUE(condition);
}
```

Test files are named `<component>_test.cpp` and located in `test/unit/` or `test/integration/` mirroring the source tree.

### Documentation Style

Doxygen-style comments for public APIs:
```cpp
/**
 * @brief Short description
 * @param name Description of parameter
 * @return Description of return value
 */
```

## CI/CD

GitHub Actions (`.github/workflows/ci.yml`):
- **Trigger:** Push/PR to `main`
- **Matrix:** Ubuntu (GCC 11, Clang 14) + macOS (GCC 11, Clang)
- **Steps:** Install deps -> CMake configure (Ninja) -> Build -> CTest

## Configuration

Runtime config: `config/config.yaml` with sections for:
- `storage:` — Block size, cache size, demotion threshold, retention, granularity tiers
- `histogram:` — Default type, bucket boundaries, DDSketch alpha
- `otel:` — Server address, batch settings, queue size
- `query:` — Concurrency limits, timeout, cache size
- `logging:` — Level, format, file paths, rotation

## Key Files for Common Tasks

| Task | Start Here |
|------|------------|
| Add a new storage feature | `include/tsdb/storage/storage_impl.h`, `src/tsdb/storage/storage_impl.cpp` |
| Modify query engine | `include/tsdb/prometheus/promql/engine.h`, `src/tsdb/prometheus/promql/` |
| Add a PromQL function | `src/tsdb/prometheus/promql/functions/` |
| Add authentication method | `include/tsdb/prometheus/auth/`, follow existing patterns (basic, bearer, header) |
| Add new compression codec | `include/tsdb/storage/compression.h`, `src/tsdb/storage/compression.cpp` |
| Modify HTTP API | `include/tsdb/prometheus/server/http_server.h` |
| Add OTEL metric type | `include/tsdb/otel/bridge.h`, `src/tsdb/otel/` |
| Add a new test | `test/unit/<module>/` or `test/integration/<module>/` |
| Server CLI flags | `src/tsdb/main.cpp` |

## Git Conventions

Commit messages use conventional-style prefixes observed in the project:
- `feat(scope):` — New features
- `fix(scope):` — Bug fixes
- `bench:` — Benchmark changes
- `chore:` — Maintenance tasks
- `Phase/Step` prefixes for phased development work
- Descriptive messages summarizing the "what" and "why"

## Important Notes

- The Makefile is the primary interface for all build/test operations — always use `make` targets rather than invoking CMake directly
- Warnings are suppressed (`-w` flag) in the build to keep output clean
- The `TSDB_SEMVEC` option is experimental and OFF by default
- Parquet cold storage uses `.idx` sidecar files for fast lookups
- The `otel-proto/` directory is a git submodule — run `git submodule update --init` after cloning
- Test data fixtures live in `test/data/`
