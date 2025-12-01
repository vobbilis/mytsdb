# Use Case Analysis: High Cardinality Network Telemetry

**Scenario:** Tracking TCP connection metrics (latency, throughput, retransmits).
**Dimensions:** `src_ip`, `dst_ip`, `src_port`, `dst_port`, `protocol`.
**Challenge:** The combination of ephemeral ports (`src_port`) creates an explosion of unique time series (Cardinality).

## 1. The Problem with Traditional TSDBs (Inverted Index)

In a system like Prometheus or our current MyTSDB:
- Every unique combination of `{src_ip, dst_ip, src_port, dst_port}` creates a new **Series ID**.
- This Series ID is added to the **Inverted Index** (Postings List).
- **Result:**
    -   If you have 10k servers talking to 10k clients on random ports, you might generate **billions** of unique series per day.
    -   The Index grows larger than RAM.
    -   Ingestion slows to a crawl (Index Lock contention).
    -   Querying "Total traffic by Subnet" becomes slow because it has to merge millions of individual series.

## 2. The Solution: Parquet (Columnar Storage)

The Parquet/IOx approach is **perfect** for this use case because it treats dimensions as **Data**, not just **Metadata**.

### 2.1 Storage Layout
Instead of creating a series object for every connection, we store data in a table-like structure:

| Time | SrcIP | DstIP | SrcPort | DstPort | Protocol | Bytes | Latency |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| t1 | 10.0.0.1 | 192.168.1.5 | 12345 | 80 | TCP | 500 | 12ms |
| t1 | 10.0.0.1 | 192.168.1.5 | 12346 | 80 | TCP | 200 | 10ms |
| ... | ... | ... | ... | ... | ... | ... | ... |

### 2.2 Why this works
1.  **No Index Overhead:** We do *not* index `SrcPort`. It is just an integer column. Writing a new port is as cheap as writing the byte count.
2.  **Compression:**
    -   `SrcIP` / `DstIP`: Highly repetitive. Dictionary Encoding reduces 1000s of IPs to small integers.
    -   `Protocol`: Run-Length Encoding (RLE) compresses "TCP, TCP, TCP..." to almost nothing.
    -   `SrcPort`: Delta encoding works well if ports are sequential.
3.  **Aggregation (Up-sampling):**
    -   **Query:** `SELECT sum(Bytes) FROM traffic WHERE SrcIP = '10.0.0.1'`
    -   **Execution:** The engine scans the `SrcIP` column (fast vector scan). It ignores `SrcPort` entirely.
    -   **Speed:** Aggregating 1 billion rows takes milliseconds/seconds on modern CPUs because it's just summing a contiguous array of integers.

## 3. Aggregation & Up-sampling Strategy

To handle long-term retention of this data, you don't want to keep every single packet/connection forever.

### 3.1 Raw Data (Parquet)
-   Keep full fidelity (all ports) for e.g., 3-7 days.
-   Stored in Parquet files partitioned by Time (e.g., 2-hour chunks).

### 3.2 Rollups (Materialized Views)
-   Create a background job (using DuckDB/Acero) that reads the Raw Parquet files.
-   **Group By:** `SrcIP`, `DstIP`, `Protocol` (Drop `SrcPort`, `DstPort`).
-   **Aggregate:** `sum(Bytes)`, `avg(Latency)`, `max(Latency)`.
-   **Write:** New "Daily Rollup" Parquet files.
-   **Result:** 100x reduction in data size. You lose "per-port" visibility for old data, but keep "per-IP" visibility forever.

## 4. Conclusion

**Yes, this is the ideal use case for the Parquet engine.**
It solves the cardinality explosion by removing the index, and enables efficient aggregation by using columnar scans.
