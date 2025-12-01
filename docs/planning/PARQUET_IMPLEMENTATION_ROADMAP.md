# Parquet Integration Implementation Roadmap

**Status:** Draft Plan
**Objective:** Implement a Hybrid Storage Engine with Apache Parquet as the Cold Tier backing store.

## 1. Phase 1: Foundation & Dependencies (Week 1)
*Goal: Set up the build environment and verify basic Arrow/Parquet operations.*

### 1.1 Build System Updates
- [ ] **Task:** Update `CMakeLists.txt` to include `Apache Arrow` and `Apache Parquet` C++ libraries.
- [ ] **Task:** Create a `docker/Dockerfile.dev` update to include these libraries in the build image.
- [ ] **Verification:** Create a simple "Hello World" C++ program that links against Arrow and creates a simple Table.

### 1.2 Core Interfaces
- [ ] **Task:** Define `ColdStorageWriter` interface.
- [ ] **Task:** Define `ColdStorageReader` interface.
- [ ] **Task:** Create `ParquetConfig` struct (compression level, row group size, etc.).

## 2. Phase 2: Core Components (Coding) (Weeks 2-3)
*Goal: Implement the low-level machinery to read/write Parquet files.*

### 2.1 Schema Mapping (`src/tsdb/storage/parquet/schema_mapper.cpp`)
- [ ] **Task:** Implement `TimeSeriesToArrowSchema()`: Maps internal types to Arrow Schema.
- [ ] **Task:** Implement `SampleBatchToArrowRecordBatch()`: Converts a batch of samples to an Arrow RecordBatch.
- [ ] **Task:** **Unit Test:** `SchemaMapperTest` - Verify type conversions (Double, Int64, String, Map, Struct/Histogram).

### 2.2 Parquet Writer (`src/tsdb/storage/parquet/writer.cpp`)
- [ ] **Task:** Implement `ParquetWriter` class wrapping `parquet::arrow::FileWriter`.
- [ ] **Task:** Implement `WriteBatch()`: Accepts `RecordBatch` and writes to disk.
- [ ] **Task:** Implement `Close()`: Finalizes the file and writes footer.
- [ ] **Task:** **Unit Test:** `ParquetWriterTest` - Write a file and verify it exists and has valid magic bytes.

### 2.3 Parquet Reader (`src/tsdb/storage/parquet/reader.cpp`)
- [ ] **Task:** Implement `ParquetReader` class wrapping `parquet::arrow::FileReader`.
- [ ] **Task:** Implement `ReadSchema()`: Returns the file schema.
- [ ] **Task:** Implement `ReadBatch()`: Returns `RecordBatch` stream.
- [ ] **Task:** **Unit Test:** `ParquetReaderTest` - Read the file written by `ParquetWriterTest` and verify data equality.

## 3. Phase 3: Storage Engine Integration (Weeks 4-5)
*Goal: Connect the Parquet components to the main `StorageImpl`.*

### 3.1 Background Flusher
- [ ] **Task:** Modify `BackgroundProcessor` to identify "Sealed & Old" blocks.
- [ ] **Task:** Implement `BlockToParquetConverter`:
    -   Iterates over a `Block`.
    -   Uses `SchemaMapper` and `ParquetWriter` to create a `.parquet` file.
- [ ] **Task:** **Integration Test:** `BlockConversionTest` - Create a populated Block, run converter, verify Parquet file output.

### 3.2 Cold Tier Management
- [ ] **Task:** Update `BlockManager` to track "Cold Blocks" (Parquet files) separately from "Hot Blocks".
- [ ] **Task:** Implement `ColdBlockIndex`: A lightweight in-memory index (min/max time, bloom filter) for Parquet files to avoid opening them unnecessarily.

## 4. Phase 4: Query Engine Integration (Weeks 6-7)
*Goal: Make the data searchable.*

### 4.1 Query Path Update
- [ ] **Task:** Update `StorageImpl::query()` to check `ColdBlockIndex`.
- [ ] **Task:** Implement `ParquetSeriesIterator`:
    -   *Option A:* Use **Apache Arrow Acero** for streaming execution (filtering/projection).
    -   *Option B:* Use **DuckDB** for SQL-based execution (if complex aggregations are needed).
    -   *Decision:* Start with Acero for simple filtering.
- [ ] **Task:** **Integration Test:** `HybridQueryTest` - Write data, force flush to Parquet, query it back via standard API (merging Hot+Cold results).

### 4.2 Predicate Pushdown (Optimization)
- [ ] **Task:** Implement `FilterToArrowExpression()`: Convert internal `LabelMatcher` to Arrow Compute Expressions.
- [ ] **Task:** Pass these expressions to the `ParquetReader` to enable row group filtering.

## 5. Comprehensive Testing Plan

### 5.1 Unit Tests (C++)
-   `SchemaMapperTest`:
    -   `TestSimpleMetricMapping`: Value + Timestamp.
    -   `TestLabelMapping`: Sparse columns vs. Map type.
    -   `TestHistogramMapping`: Nested struct correctness.
-   `ParquetIOTest`:
    -   `TestRoundTrip`: Write -> Read -> Compare.
    -   `TestCompression`: Verify file size reduction with Snappy/ZSTD.

### 5.2 Integration Tests
-   `StorageLifecycleTest`:
    -   Write 1M points -> Wait for Flush -> Verify Parquet File Created -> Verify Memory Cleared.
-   `ColdQueryTest`:
    -   Query data that exists *only* in Parquet.
    -   Query data spanning *both* Memory and Parquet (Merge logic).

### 5.3 End-to-End (E2E) Tests
-   **Tool:** `tests/e2e_parquet_suite.py` (New)
-   **Scenario 1: High Cardinality**
    -   Ingest 1M series with unique `request_id`.
    -   Flush to Parquet.
    -   Query specific `request_id`.
    -   **Success Criteria:** Query < 100ms, Memory usage stable.
-   **Scenario 2: Interoperability**
    -   Ingest data -> Flush to Parquet.
    -   Open the resulting file with `duckdb` CLI or `pandas`.
    -   **Success Criteria:** External tools can read the schema and data correctly.

### 5.4 Performance Benchmarks
-   **Write Throughput:** Measure overhead of the background flush thread.
-   **Read Latency:** Compare `Block` read vs. `Parquet` read.
-   **Storage Size:** Compare `.block` size vs. `.parquet` size (expecting 30-50% reduction).

## 6. Risk Management
-   **Risk:** Build complexity with Arrow.
    -   *Mitigation:* Use pre-built binaries or Docker images.
-   **Risk:** Query performance regression on Cold Data.
    -   *Mitigation:* Aggressive caching of Parquet Footers/Metadata.
