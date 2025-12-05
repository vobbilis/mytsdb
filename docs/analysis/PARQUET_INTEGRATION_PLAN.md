# Parquet Integration Plan for MyTSDB

**Status:** Draft Proposal
**Goal:** Adopt Apache Parquet as the native storage format for the Cold Tier to solve high cardinality issues and enable interoperability.

## 1. Strategy: The "Hybrid" Evolution

We will not rewrite the entire engine immediately. Instead, we will implement a **Hybrid Storage Model**:

-   **Hot Tier (Recent Data):** Remains as-is (In-memory, WAL, specialized TSDB blocks) for low-latency writes and recent queries.
    -   *Note:* We will **NOT** use Apache Arrow for the mutable in-memory store initially. Arrow is immutable and expensive for single-point inserts. We will stick to our optimized C++ structures for ingestion and convert to Arrow only during flush.
-   **Cold Tier (Historical Data):** Replaces the custom block format with **Apache Parquet**.

## 2. Implementation Phases

### Phase 1: The Parquet Exporter (Tooling)
*Goal: Validate we can write valid Parquet files from our data.*

1.  **Dependency:** Add `apache-arrow-cpp` and `parquet-cpp` to `CMakeLists.txt`.
2.  **Tool:** Create `tools/tsdb_to_parquet.cpp`.
    -   Reads MyTSDB blocks.
    -   Converts `TimeSeries` -> `Arrow Table`.
    -   Writes `Parquet` file.
3.  **Schema Mapping:**
    -   `timestamp` (Int64) -> Column `time`
    -   `value` (Double) -> Column `value`
    -   `labels` (Map) -> Columns `tag_key1`, `tag_key2`... (Sparse columns or Map type)
    -   **Histograms:** Mapped to a `Struct` column containing:
        -   `sum` (Double)
        -   `count` (Int64)
        -   `buckets` (List<Struct<le: Double, count: Int64>>)

### Phase 2: The Cold Tier Archiver
*Goal: Automatically move old data to Parquet.*

1.  **Background Job:** Update `StorageImpl` background processor.
2.  **Logic:**
    -   When a Block is "sealed" and "old" (e.g., > 2 hours).
    -   Instead of flushing to disk as `.block`, flush as `.parquet`.
    -   Upload to S3 (or local disk `data/parquet/`).

### Phase 3: The Parquet Reader (Query Path)
*Goal: Query historical data from Parquet.*

1.  **Reader:** Implement `ParquetBlockReader`.
    -   Uses `parquet::arrow::FileReader`.
    -   Supports "Projection" (read only needed columns).
    -   Supports "Predicate Pushdown" (filter by time/tags at the file scan level).
2.  **Integration:** Hook into `StorageImpl::query()`.
    -   If query range overlaps with Hot Tier -> Read Memory/WAL.
    -   If query range overlaps with Cold Tier -> Read Parquet.
    -   Merge results.

### Phase 4: High Cardinality Support (The Payoff)
*Goal: Support "Fields" that are not indexed.*

1.  **Schema Change:** Allow `TimeSeries` to have "Unindexed Tags" (Fields).
2.  **Storage:**
    -   Hot Tier: Store these in a sidecar structure (not the Inverted Index).
    -   Cold Tier: Write them as standard Parquet columns.
3.  **Query:**
    -   When querying by an Unindexed Tag (e.g., `request_id="123"`):
    -   Skip Inverted Index lookup.
    -   Scan Parquet files directly (using SIMD/Arrow).

## 3. Technical Challenges & Solutions

### 3.1 Schema Evolution
*Challenge:* Different series have different labels. Parquet likes a fixed schema.
*Solution:*
-   **Option A (Sparse):** One giant table with a column for every label ever seen. (Good for < 1000 unique label keys).
-   **Option B (Map Type):** Use Parquet's `Map<String, String>` type for labels. (Flexible, but slower to query specific tags).
-   **Option C (Partitioning):** Group series with similar label sets into the same Parquet file.

*Recommendation:* **Option A** for "Common Tags" (host, region) + **Option B** for "Rare/Dynamic Tags".

### 3.2 Performance
*Challenge:* Parquet is slower to write than our custom append-only format.
*Solution:* This is why we keep the **Hot Tier**. We only write Parquet asynchronously in the background (Compaction), so write latency is unaffected.

### 3.3 Complex Types (Histograms)
*Challenge:* Histograms are complex structures (buckets, sum, count) that don't fit a single float column.
*Solution:* Parquet natively supports **Nested Types** (Structs, Lists, Maps).
-   We will store Histograms as a **Struct Column**.
-   This avoids "schema explosion" (creating a column for every bucket bound).
-   Query engines (like **DuckDB** or **Apache Arrow Acero**) can easily query nested fields (e.g., `histogram.sum` or `histogram.buckets[0]`).

## 4. Benefits

1.  **Infinite Cardinality:** Store `trace_id` or `request_id` without blowing up memory.
2.  **Compression:** Parquet's dictionary encoding is world-class.
3.  **Ecosystem:** Open MyTSDB data in **DuckDB**, **Pandas**, or **Spark** for deep analytics.
4.  **Future Proof:** Aligns with the industry standard (InfluxDB 3.0, VictoriaMetrics, GreptimeDB).
