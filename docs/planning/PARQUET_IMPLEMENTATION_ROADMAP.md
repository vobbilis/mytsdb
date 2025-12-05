# Parquet Integration Implementation Roadmap

**Status:** Implementation Complete (Phases 1-5)
**Objective:** Implement a Hybrid Storage Engine with Apache Parquet as the Cold Tier backing store.

## 1. Phase 1: Foundation & Dependencies (Completed)
*Goal: Set up the build environment and verify basic Arrow/Parquet operations.*

### 1.1 Build System Updates
- [x] **Task:** Update `CMakeLists.txt` to include `Apache Arrow` and `Apache Parquet` C++ libraries.
- [x] **Task:** Create a `docker/Dockerfile.dev` update to include these libraries in the build image.
- [x] **Verification:** Create a simple "Hello World" C++ program that links against Arrow and creates a simple Table.

### 1.2 Core Interfaces
- [x] **Task:** Define `ColdStorageWriter` interface.
- [x] **Task:** Define `ColdStorageReader` interface.
- [x] **Task:** Create `ParquetConfig` struct (compression level, row group size, etc.).

## 2. Phase 2: Core Components (Completed)
*Goal: Implement the low-level machinery to read/write Parquet files.*

### 2.1 Schema Mapping (`src/tsdb/storage/parquet/schema_mapper.cpp`)
- [x] **Task:** Implement `TimeSeriesToArrowSchema()`: Maps internal types to Arrow Schema.
- [x] **Task:** Implement `SampleBatchToArrowRecordBatch()`: Converts a batch of samples to an Arrow RecordBatch.
- [x] **Task:** **Unit Test:** `SchemaMapperTest` - Verify type conversions (Double, Int64, String, Map, Struct/Histogram).

### 2.2 Parquet Writer (`src/tsdb/storage/parquet/writer.cpp`)
- [x] **Task:** Implement `ParquetWriter` class wrapping `parquet::arrow::FileWriter`.
- [x] **Task:** Implement `WriteBatch()`: Accepts `RecordBatch` and writes to disk.
- [x] **Task:** Implement `Close()`: Finalizes the file and writes footer.
- [x] **Task:** **Unit Test:** `ParquetWriterTest` - Write a file and verify it exists and has valid magic bytes.

### 2.3 Parquet Reader (`src/tsdb/storage/parquet/reader.cpp`)
- [x] **Task:** Implement `ParquetReader` class wrapping `parquet::arrow::FileReader`.
- [x] **Task:** Implement `ReadSchema()`: Returns the file schema.
- [x] **Task:** Implement `ReadBatch()`: Returns `RecordBatch` stream.
- [x] **Task:** **Unit Test:** `ParquetReaderTest` - Read the file written by `ParquetWriterTest` and verify data equality.

## 3. Phase 3: Storage Engine Integration (Completed)
*Goal: Connect the Parquet components to the main `StorageImpl`.*

### 3.1 Background Flusher
- [x] **Task:** Modify `BackgroundProcessor` to identify "Sealed & Old" blocks.
- [x] **Task:** Implement `BlockToParquetConverter`:
    -   Iterates over a `Block`.
    -   Uses `SchemaMapper` and `ParquetWriter` to create a `.parquet` file.
- [x] **Task:** **Integration Test:** `BlockConversionTest` - Create a populated Block, run converter, verify Parquet file output.

### 3.2 Cold Tier Management
- [x] **Task:** Update `BlockManager` to track "Cold Blocks" (Parquet files) separately from "Hot Blocks".
- [x] **Task:** Implement `ColdBlockIndex`: A lightweight in-memory index (min/max time) for Parquet files to avoid opening them unnecessarily.

## 4. Phase 3.5: Schema Evolution & High Cardinality (Completed)
*Goal: Support changing dimensions without creating new series, solving the cardinality curse.*

### 4.1 Schema Evolution Support
- [x] **Task:** Update `TimeSeries` to distinguish between **Identity Labels** (Tags) and **Field Labels** (Dimensions).
- [x] **Task:** Update `StorageImpl` to allow appending samples with new fields to existing series.
- [x] **Task:** Update `BlockImpl` and `SchemaMapper` to handle sparse columns and schema merging.

### 4.2 High Cardinality Testing
- [x] **Task:** **Benchmark:** `TestSchemaEvolution` - 10M samples, 100s of changing dimensions.
    -   Verify no new series creation for dimension changes.
    -   Measure write latency and memory overhead.
- [x] **Task:** **Integration Test:** Verify reading back data with evolved schema (union of all columns).

## 5. Phase 4: Query Integration (Hybrid Path) (Completed)
*Goal: Make the data searchable across both Hot and Cold tiers.*

### 5.1 Hybrid Query Engine
- [x] **Task:** Update `StorageImpl::query` to check `ColdBlockIndex`.
- [x] **Task:** Implement `ParquetBlock` class implementing `Block` interface (Read-Only).
- [x] **Task:** Implement logic to merge results from Hot (Memory) and Cold (Parquet) tiers.

### 5.2 Verification
- [x] **Task:** **Integration Test:** `HybridQueryTest`
    -   `TestHotAndColdQuery`: Verify querying ranges spanning both tiers.
    -   `TestPersistenceAndRecovery`: Verify data survives restart.
    -   `TestSchemaEvolutionQuery`: Verify querying across schema changes.
    -   `TestQueryOnlyCold`: Verify querying only Parquet data.

## 6. Phase 5: Parquet Optimizations (Completed)
*Goal: Improve storage efficiency and write performance for high-scale production use.*

### 6.1 Compaction
- [x] **Task:** Implement `BlockManager::compactParquetFiles` to merge small Parquet files into larger row groups.
- [x] **Task:** Implement `StorageImpl::execute_background_compaction` to automatically trigger compaction.
- [x] **Task:** **Verification:** `TestCompaction` - Verify merging of multiple files and data integrity.

### 6.2 Storage Efficiency
- [x] **Task:** Implement **Dictionary Encoding** for high-cardinality string fields.
- [x] **Task:** Tune **Compression Levels** (Switched to **ZSTD**).

### 6.3 Write Performance
- [ ] **Task:** Implement **Async Write Path** (Thread Pool) to avoid blocking flush (Deferred).

## 7. Comprehensive Testing Evidence

### 7.1 Unit Tests (C++)
-   `SchemaMapperTest`: Verified type conversions and schema generation.
-   `ParquetIOTest`: Verified basic Read/Write operations.

### 7.2 Integration Tests
-   `ParquetIntegrationTest`:
    -   `TestFlushToParquet`: Verified write -> flush -> Parquet file creation.
    -   `TestLargeScaleFlush`: Verified stability with 1000 series / 100k samples.
-   `HybridQueryTest`:
    -   `TestCompaction`: Verified merging of small blocks.
    -   `TestPersistenceAndRecovery`: Verified restart capability.
    -   `TestSchemaEvolutionQuery`: Verified dynamic schema handling.

### 7.3 Performance Benchmarks
-   **Throughput**: ~29.5k samples/sec (with schema evolution).
-   **Persistence**: 10,000 Parquet files created (196 MB total).
-   **Stability**: No memory leaks or explosion during high-load tests.
