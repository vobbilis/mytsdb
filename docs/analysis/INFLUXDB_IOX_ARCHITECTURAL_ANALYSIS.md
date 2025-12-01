# InfluxDB IOx (3.0) Architectural Analysis

**Generated:** November 2025
**Focus:** High Cardinality Solutions & Parquet Integration

## 1. Executive Summary

InfluxDB IOx (now the core of InfluxDB 3.0) represents a paradigm shift in Time Series Database (TSDB) architecture. It moves away from the traditional "Inverted Index + LSM Tree" model (used by Prometheus, Cortex, and InfluxDB 1.x/2.x) to a **Columnar Store** model built on the **Apache Arrow** (memory) and **Apache Parquet** (disk) ecosystem.

**Key Innovation:** It solves the "Cardinality Explosion" problem not by building a better index, but by **removing the need for a global inverted index entirely** for many use cases, relying instead on extremely fast columnar scans, partitioning, and compression.

## 2. The Cardinality Problem (The "Old" Way)

In systems like Prometheus/Cortex (and MyTSDB currently):
- **Tags (Labels)** are indexed in an Inverted Index (Postings List).
- **Fields (Samples)** are stored in chunks.
- **Problem:** If you have a tag `request_id` with 1 billion unique values, the Inverted Index grows larger than RAM, causing OOMs and massive write amplification.
- **Limit:** Typically ~10-100 million active series per node.

## 3. The IOx Solution (The "New" Way)

### 3.1 Architecture Components
1.  **Ingester (Hot Tier)**:
    -   Receives writes (gRPC/Line Protocol).
    -   Buffers data in **Apache Arrow RecordBatches** (in-memory columnar format).
    -   Persists to a Write-Ahead Log (WAL).
    -   **No global index** is updated synchronously.
2.  **Object Store (Cold Tier)**:
    -   Data is flushed to **Apache Parquet** files in S3/Object Storage.
    -   Parquet files are immutable and heavily compressed.
3.  **Querier (Stateless)**:
    -   Uses **DataFusion** (Rust-based SQL engine) to query Parquet files directly from S3 (with local caching).
    -   Performs "Late Materialization" and vectorized execution.

### 3.2 Solving High Cardinality
IOx treats "Tags" and "Fields" almost identically at the storage layer—they are just **Columns** in a Parquet file.

-   **No Global Inverted Index:** There is no massive `map<tag_value, list<series_id>>` to maintain.
-   **Scan-based Filtering:** To find `request_id="123"`, the engine scans the `request_id` column.
-   **Why is this fast?**
    -   **Columnar Layout:** Only the `request_id` column is read from disk.
    -   **Vectorization (SIMD)**: Modern CPUs can scan billions of values per second.
    -   **Zone Maps / Statistics**: Parquet headers store min/max values for each chunk. If `request_id="123"` is not in the min/max range of a chunk, the entire chunk is skipped (Partition Pruning).
    -   **Dictionary Encoding**: High-cardinality string columns are dictionary-encoded, so scans happen on integers, not strings.

## 4. Key Technologies

### 4.1 Apache Parquet (Storage)
-   **Columnar:** Excellent compression (RLE, Dictionary, ZSTD).
-   **Self-Describing:** Schema is embedded in the file.
-   **Ecosystem:** Compatible with Spark, Pandas, DuckDB, Snowflake.

### 4.2 Apache Arrow (Memory)
-   **Zero-Copy:** Data format in memory matches the wire format.
-   **Vectorized:** Optimized for SIMD operations.

### 4.3 DataFusion (Query Engine)
-   **Extensible:** Rust-based query planner and execution engine.
-   **SQL Support:** Native SQL support alongside PromQL (via transpilation or native implementation).

## 5. Comparison: MyTSDB vs. Cortex vs. IOx

| Feature | Cortex / Prometheus | MyTSDB (Current) | InfluxDB IOx |
| :--- | :--- | :--- | :--- |
| **Storage Model** | TSDB Blocks (Inverted Index) | TSDB Blocks (Inverted Index) | **Parquet Files (Columnar)** |
| **High Cardinality** | ❌ Fails (Index Explosion) | ⚠️ Limited (Roaring Bitmaps help) | ✅ **Solved (Columnar Scan)** |
| **Query Engine** | PromQL (Scatter-Gather) | PromQL (Scatter-Gather) | **DataFusion (SQL + Vectorized)** |
| **Compression** | Gorilla / XOR | Gorilla / XOR | **Parquet (RLE, Dict, ZSTD)** |
| **Ecosystem** | Prometheus-only | Prometheus-only | **Open (Arrow/Parquet)** |

## 6. Recommendations for MyTSDB

To achieve "Infinite Cardinality" and modern interoperability, MyTSDB should evolve towards the IOx model:

1.  **Adopt Parquet:** Move the "Cold Tier" storage format from custom blocks to Apache Parquet.
2.  **Hybrid Indexing:** Keep the Inverted Index for "Tags" (low cardinality) for ultra-fast lookups, but allow "Fields" (high cardinality) to be stored as unindexed Parquet columns.
3.  **Vectorized Execution:** Shift the query engine to process Arrow RecordBatches instead of iterating samples one by one.

## 7. References
-   [InfluxDB IOx Architecture](https://github.com/influxdata/influxdb_iox)
-   [Apache Arrow](https://arrow.apache.org/)
-   [Apache Parquet](https://parquet.apache.org/)
