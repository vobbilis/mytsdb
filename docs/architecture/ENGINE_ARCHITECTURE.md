# Storage Engine

MyTSDB is a high-performance time series database designed for massive scale, durability, and interoperability. While it shares the fundamental goal of storing timestamped data with other TSDBs, its architecture is distinguished by a unique "Lakehouse-native" design that bridges the gap between low-latency operational monitoring and long-term analytical storage.

This document details the internal architecture of the MyTSDB storage engine, focusing on its data layout, write path, and read path.

## Architecture Overview

The storage engine is best understood as a hybrid system that manages data across three distinct tiers, each optimized for a specific stage in the data lifecycle:

1.  **Hot Tier (Memory)**: Optimized for sub-millisecond writes and immediate query availability.
2.  **Warm Tier (Memory-Mapped)**: Optimized for recent data access with moderate compression.
3.  **Cold Tier (Parquet/Object Storage)**: Optimized for massive scale, high compression, and analytical interoperability.

Unlike traditional LSM-based engines that rely on proprietary file formats for all data, MyTSDB natively integrates **Apache Parquet** for its cold storage. This design choice eliminates the "data silo" problem, allowing historical data to be directly queried by analytical tools like Spark, Presto, or DuckDB without ETL processes.

## Data Layout

### In-Memory Object Layout

At the core of the Hot Tier is the **Block**, the fundamental unit of storage. A Block represents a fixed time window (e.g., 2 hours) of data for a specific shard.

Inside a Block, data is organized to maximize cache locality and minimize lock contention:

*   **Series Index**: A sharded, lock-free hash map that maps Series IDs to their in-memory structures.
*   **Active Buffer**: A write-optimized buffer for incoming data points. It uses a specialized "Gorilla-like" compression stream that compresses timestamps and values on the fly.
*   **L1 Cache (Working Set)**: A highly optimized LRU cache storing the most frequently accessed series. It boasts a hit ratio of >98% for typical workloads, delivering data in <0.1ms.

### Persistent Storage (The "Lakehouse" Layer)

When data "cools down," it is flushed to the Cold Tier. Here, MyTSDB diverges from conventional designs:

*   **Format**: **Apache Parquet**.
*   **Compression**: **ZSTD** + **Dictionary Encoding**.
*   **Schema**: A "wide-table" schema where unique label combinations map to rows, and time/value pairs are stored in compressed columnar arrays.

This layout offers significant advantages:
*   **Schema Evolution**: New labels can be added without rewriting old files, solving the "cardinality explosion" issue common in other engines.
*   **Compression**: Columnar storage allows for type-specific compression (e.g., RLE for constant labels, Delta for timestamps), achieving 20-60% better compression than row-based formats.

## Write Path

The lifecycle of a write in MyTSDB is designed for high concurrency and zero data loss.

1.  **Request Arrival**: A client sends a `writeBatch` request containing a namespace, series ID, timestamp, and value.
2.  **Sharded WAL (Write Ahead Log)**:
    *   The request is immediately appended to a **Sharded WAL**.
    *   The WAL is partitioned into 16 independent shards, each with its own background writer. This allows writes to scale linearly with core count, avoiding the global lock contention seen in monolithic WAL designs.
3.  **In-Memory Buffer**:
    *   The engine hashes the series ID to determine the owning shard.
    *   It looks up the series in the **Series Index**. If the series is new, it is atomically created.
    *   The data point is encoded into the active **compression stream**.
4.  **Completion**: The write is acknowledged to the client.

This path is fully non-blocking for the client (except for the WAL append, which is essentially a memory copy followed by an async flush).

## Read Path

The read path implements a **Hybrid Query Engine** that transparently merges data from memory and disk.

1.  **Query Analysis**: The engine determines which time blocks overlap with the query range.
2.  **Tiered Lookup**:
    *   **L1 Cache Check**: The engine first checks the L1 Hot Cache. If the data is found, it is returned immediately (<0.1ms).
    *   **L2 Cache Check**: If missed, it checks the Memory-Mapped L2 Cache (1-5ms).
    *   **Bloom Filter**: For data potentially on disk, the engine consults a Bloom Filter to avoid unnecessary I/O.
3.  **Data Retrieval & Merging**:
    *   **Hot Data**: Retrieved from the active in-memory buffer.
    *   **Cold Data**: If necessary, the engine opens the relevant Parquet files. It uses "projection pushdown" to read *only* the necessary columns (e.g., just the timestamp and value columns, skipping massive label columns).
4.  **Result Assembly**:
    *   Streams from the Hot Buffer and Cold Parquet files are merged.
    *   If a series has updates in the buffer that supersede disk data, the engine resolves the conflict in favor of the newer data.
    *   The result is streamed back to the client.

## Background Processes

To maintain performance over time, the engine runs several autonomous background processes:

*   **Flushing**: Periodically moves data from the active buffer to immutable memory-mapped files (Warm Tier).
*   **Compaction**: Merges small Parquet files into larger, more efficient ones in the Cold Tier. This process also applies heavier compression (e.g., increasing ZSTD levels) to older data.
*   **Retention Enforcement**: Deletes data that has exceeded its configured retention period (TTL).

## Summary

MyTSDB's storage engine combines the raw speed of an in-memory database with the scalability and openness of a data lake. By using a sharded, lock-free write path and a Parquet-backed cold store, it delivers a unique blend of high ingestion rates, low-latency queries, and seamless analytical integration.
