# Project Analysis

## 1. Project Structure
The project is a Time Series Database (TSDB) written in C++.
- **Root**: Contains build configuration (`CMakeLists.txt`, `Makefile`) and documentation.
- **docs**: Extensive documentation including architecture, planning, and testing.
- **src**: Source code, organized by component (`tsdb`, `core`, `histogram`, `storage`).
- **include**: Header files.
- **test**: Test files (unit and integration).

## 2. Architecture
The system follows a layered architecture:
1.  **Client Layer**: HTTP/gRPC clients.
2.  **API Gateway**: HTTP Server, gRPC Service, OTEL Bridge.
3.  **Query Layer**: PromQL Engine, Query Planner.
4.  **Core Layer**: Data models (TimeSeries, Labels, Sample).
5.  **AdvPerf Layer**: Caching, Object Pooling, Background Processing.
6.  **Storage Layer**: Block Management, Compression, Series Management.
7.  **Persistence Layer**: File System, WAL.

Key features:
-   **High Concurrency**: Sharded storage, write queues.
-   **Performance**: Multi-level caching, object pooling, advanced compression (Gorilla, XOR, RLE).
-   **Integration**: OTEL and Prometheus support.

## 3. Code Analysis
-   **StorageImpl**: The central component implementing the storage logic. It integrates various sub-components:
    -   **Memory Management**: Uses `TimeSeriesPool`, `LabelsPool`, and `SamplePool` to reduce allocations.
    -   **Caching**: Implements a `WorkingSetCache` for hot data and a `CacheHierarchy` (L1/L2/L3) for tiered caching.
    -   **Persistence**: Uses `BlockManager` for managing persistent blocks and `WriteAheadLog` (WAL) for crash recovery.
    -   **Indexing**: Maintains an in-memory `Index` for fast series lookups.
    -   **Background Processing**: `BackgroundProcessor` handles maintenance tasks like compaction and cleanup.
    -   **Predictive Caching**: `PredictiveCache` learns access patterns to prefetch data.
-   **Write Path**:
    1.  Log to WAL.
    2.  Calculate Series ID and lookup/create in Index.
    3.  Append to in-memory `Series` object.
    4.  Update Cache Hierarchy.
    5.  If block is full, seal and persist via `BlockManager`.
-   **Read Path**:
    1.  Check `PredictiveCache` and trigger prefetching.
    2.  Check `CacheHierarchy`.
    3.  If miss, check in-memory `Active Series`.
    4.  If not in memory, read from `BlockManager`.
-   **Compression**: Uses adapter classes to support multiple algorithms (Gorilla, XOR, RLE).
-   **Concurrency**: Uses `StorageImpl` sharding and background processing. Thread safety via `shared_mutex` and `concurrent_hash_map`.

## 4. Test Design
-   **Strategy**: Comprehensive testing plan covering Unit, Integration, and Performance testing.
-   **Status**:
    -   Phase 1 (Basic Functionality): Complete.
    -   Phase 2 (Component Integration): Partially complete. Missing Block Management, Background Processing, and Predictive Caching integration tests.
    -   Phase 3 (Performance): Consolidated but blocked by Phase 2.
-   **Recent Fixes**: Addressed segfaults, deadlocks, and data corruption issues.
-   **Next Steps**: Implement missing Phase 2 tests and then proceed to Phase 3.

## 5. Build System
-   **Build Tool**: CMake (managed via Makefile).
-   **Prerequisites**: C++17 compiler, CMake, Make. Optional: TBB, spdlog, gRPC, Protobuf, GTest.
-   **Configuration**:
    -   `BUILD_TESTS`: Enable tests (default ON in Makefile).
    -   `ENABLE_SIMD`: Enable SIMD optimizations (default ON).
    -   `ENABLE_OTEL`: Enable OpenTelemetry (default OFF).
-   **Build Commands**:
    -   `make deps`: Install dependencies (macOS/Linux).
    -   `make configure`: Configure with CMake.
    -   `make build`: Build the project.
    -   `make test-all`: Run all tests.
    -   `make clean`: Clean build artifacts.

## 6. Test Results (Latest Run)
-   **Status**: Failed (Exit Code 2).
-   **Failed Tests**:
    -   `Phase2BlockManagementIntegrationTest.BlockPerformanceUnderLoad`: Performance issue?
    -   `benchmark_concurrency`: SIGTRAP (Debug break?).
    -   `GRPCServerE2ETest`: Server executable not found.
    -   `BridgeConversionTest`: Attribute conversion failures.
    -   `GRPCPath.SimulateExportRequest`: Failure.
    -   `QueryServiceTest`: SEGFAULTs in `GetLabelNames` and `GetLabelValues`.
-   **Analysis**:
    -   gRPC/OTEL integration seems broken (missing executable, conversion errors).
    -   Query service has critical stability issues (SEGFAULTs).
    -   Concurrency benchmark is hitting a breakpoint/trap.

## 7. Key Files
-   `docs/architecture/ARCHITECTURE_OVERVIEW.md`: High-level architecture.
-   `docs/planning/STORAGEIMPL_COMPREHENSIVE_TEST_PLAN.md`: Detailed test plan and status.
-   `src/tsdb/storage/storage_impl.cpp`: Main storage implementation.
-   `Makefile`: Main entry point for build operations.
-   `CMakeLists.txt`: CMake configuration.
