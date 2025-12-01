# Impact of Parquet Integration on Future Roadmap

**Generated:** November 2025
**Context:** Moving to a Hybrid Storage Model (Hot Tier: Custom In-Memory, Cold Tier: Apache Parquet).

## 1. Executive Summary

The shift to Apache Parquet is **highly synergistic** with your roadmap. While it introduces a new storage format, it significantly simplifies the implementation of advanced features like Analytics and Vector Search by leveraging the rich Apache Arrow ecosystem.

| Feature Area | Impact | Key Benefit |
| :--- | :--- | :--- |
| **Alert Manager** | 🟢 Neutral / Positive | Alerts mostly query Hot Tier (unaffected). Historical alerts gain SQL capability. |
| **Semantic Vector** | 🔵 High Positive | Parquet/Arrow is the standard for vector data. Enables easy integration with vector libraries. |
| **Metric Analytics** | 🟣 Transformative | Unlocks OLAP capabilities. Enables "Bring Your Own Engine" (DuckDB/Spark) analytics. |

---

## 2. Impact on Alert Manager Integration

*Context: You plan to add Prometheus Alert Manager functionality.*

### 2.1 Real-Time Alerts (The 90% Case)
*   **Workflow:** "Alert if CPU > 90% for 5m".
*   **Impact:** **None.**
*   **Reasoning:** Real-time alerts query the **Hot Tier** (In-Memory/WAL). Since we are *not* changing the Hot Tier in Phase 1, your alert evaluation engine will continue to read from the fast in-memory structures.

### 2.2 Historical / Trend Alerts (The 10% Case)
*   **Workflow:** "Alert if today's traffic is 50% lower than last week".
*   **Impact:** **Positive.**
*   **Reasoning:** This requires reading from the **Cold Tier**.
    *   **Old Way:** We would have to write custom C++ logic to scan old blocks efficiently.
    *   **New Way (Parquet):** We can use **Vectorized Arrow Readers**. For complex conditions, we could even use an embedded SQL engine (DataFusion) to execute the "Last Week" aggregation efficiently.

---

## 3. Impact on Semantic Vector Search

*Context: You plan to embed metric names/tags to allow semantic searching (e.g., "Show me latency metrics" -> finds `http_request_duration_seconds`).*

### 3.1 Metadata Extraction
*   **Impact:** **Positive.**
*   **Reasoning:** To generate embeddings, you need to scan all metric metadata. Parquet is a columnar format. You can read *just* the `metric_name` and `tags` columns from historical files at incredible speeds (GB/s) without reading the heavy sample values. This makes re-indexing metadata fast and cheap.

### 3.2 Vector Storage
*   **Impact:** **High Positive.**
*   **Reasoning:**
    *   **Standard Format:** Apache Arrow defines a standard memory layout for `FixedSizeList<Float>`, which is exactly what a vector embedding is.
    *   **Storage:** We can store the generated vectors *directly in the Parquet files* (or sidecar Parquet files) as a column.
    *   **Ecosystem:** Modern vector databases (like LanceDB) are built on top of Arrow/Parquet. By adopting Parquet, we align our data format with the vector search ecosystem, potentially allowing us to use off-the-shelf libraries for indexing instead of writing our own.

---

## 4. Impact on Metric Analytics

*Context: You want to perform deep analysis (e.g., "P99 latency by customer ID over the last year").*

### 4.1 The "OLAP" Advantage
*   **Impact:** **Transformative.**
*   **Reasoning:** TSDBs (LSM trees) are great for writing, but often mediocre for scanning huge datasets for analytics (OLAP). Parquet is the *industry standard* for OLAP.
    *   **Columnar Access:** If you only need `latency`, you don't read `status_code` or `method`. IO is reduced by 90%+.
    *   **Compression:** Dictionary encoding on high-cardinality tags (like `customer_id`) makes grouping and aggregation extremely fast.

### 4.2 "Bring Your Own Engine"
*   **Impact:** **New Capability.**
*   **Reasoning:** Because our data is in open Parquet files, you don't need to build a complex analytics engine inside MyTSDB.
    *   You can point **DuckDB** (embedded OLAP DB) at our `data/parquet/` directory and run complex SQL queries instantly.
    *   You can mount the data in **Spark** or **Pandas** for data science workflows.
    *   This allows MyTSDB to focus on *storage and ingestion*, while offloading heavy ad-hoc analytics to specialized tools.

## 5. Future: Delta Lake & Iceberg

*Context: You asked about "Resilient Data Sets" (likely referring to Delta Lake's ACID/Time Travel features).*

### 5.1 Raw Parquet vs. Delta Lake
*   **Raw Parquet:** Just files. Fast, simple, but no transaction guarantees (ACID) if multiple writers try to modify the same file.
*   **Delta Lake:** A layer *on top* of Parquet that adds a **Transaction Log** (JSON files). This enables:
    *   **ACID Transactions:** Safe multi-writer concurrency.
    *   **Time Travel:** "Query data as it looked yesterday."
    *   **Schema Evolution:** Safely changing columns over time.

### 5.2 Recommendation
*   **Start with Raw Parquet.** For MyTSDB, we have a single writer (the Storage Engine) per block, so we don't need complex multi-writer coordination yet.
*   **Upgrade Later.** Since Delta Lake *is* just Parquet + Logs, we can upgrade our "Cold Tier" to be a valid Delta Table in the future without rewriting the data files.

## 6. Summary Recommendation

Proceed with the Parquet integration. It does not block your future plans; rather, it provides the **foundational data layer** that makes implementing Semantic Search and Advanced Analytics significantly easier and more robust.
