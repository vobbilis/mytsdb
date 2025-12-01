# PromQL on Parquet: Compatibility Analysis

**Objective:** Ensure the new Parquet-based Cold Tier remains fully compatible with the standard PromQL interface.

## 1. The Short Answer
**Yes.** You will still use standard PromQL. The underlying storage format (Parquet) will be completely transparent to the user and the PromQL query engine.

## 2. How It Works: The Adapter Pattern

The PromQL Engine interacts with storage via a standard interface (conceptually):
```go
Select(mint, maxt int64, matchers ...*labels.Matcher) SeriesSet
```

### 2.1 The Translation Layer
We implement a `ParquetSeriesSet` that adapts Parquet data to this interface.

1.  **Input:** PromQL Query `http_requests_total{status="500", method="POST"}`
2.  **Matchers:** `status="500"`, `method="POST"`
3.  **Pushdown:** The Storage Engine translates these matchers into **Parquet Filters**:
    *   `col("status") == "500"`
    *   `col("method") == "POST"`
4.  **Scan:** The Parquet Reader scans the file.
    *   It uses **Zone Maps** (min/max) to skip chunks that don't match.
    *   It uses **Dictionary Filtering** to quickly find rows.
5.  **Output:** The Reader constructs `Series` objects on the fly from the matching rows.
    *   To the PromQL engine, these look exactly like series fetched from memory.

## 3. Handling High Cardinality (The "Port" Example)

This is where the magic happens.

**Scenario:** You have a `src_port` column with billions of unique values. It is **not indexed** in an Inverted Index.

**Query:** `tcp_packets_total{src_port="12345"}`

**Execution Flow:**
1.  **PromQL Engine:** Sees `src_port` as just another label. It asks storage: *"Give me series where src_port=12345"*.
2.  **Storage Layer:**
    *   **Hot Tier:** Checks Inverted Index. (Likely empty or small if we only index "Tags").
    *   **Cold Tier (Parquet):** Receives the request. It does **not** look up an index. Instead, it pushes `src_port == 12345` to the Parquet Scanner.
3.  **Parquet Scanner:**
    *   Reads the `src_port` column.
    *   Efficiently skips 99.9% of data that doesn't match.
    *   Returns the few rows that match.
4.  **Result:** The user gets their answer.

**Key Takeaway:** You can query by "Unindexed Fields" (like ports) using standard PromQL syntax.

## 4. The Hybrid Query Path (Hot + Cold)

You are correct that **Hot Data is NOT in Parquet**. It lives in RAM (C++ objects). The Storage Engine handles this transparently:

### 4.1 Execution Flow
When a query arrives (e.g., `rate(http_requests[5m])`):

1.  **Step A: Query Hot Tier (Memory)**
    *   The engine looks up series in the **In-Memory Map**.
    *   *Note:* For high-cardinality queries (e.g., `src_port`), it performs a **Memory Scan** of the active series. This is fast because the "Hot Window" (e.g., last 2 hours) is small enough to fit in CPU cache.
2.  **Step B: Query Cold Tier (Parquet)**
    *   The engine scans the **Parquet Files** on disk/S3.
    *   It uses Columnar Scanning to filter data efficiently.
3.  **Step C: Merge**
    *   The engine combines the samples from Step A and Step B into a single `Series` object.
    *   The PromQL Engine (and the user) sees one continuous stream of data.

## 5. Advanced: Vectorized PromQL (Future Optimization)

Standard PromQL processes data point-by-point. With Parquet/Arrow, we can eventually upgrade the engine to process **Chunks** of data (Vectorization).

*   **Standard:** `for point in series: sum += point.value`
*   **Vectorized:** `sum = arrow_compute.sum(column_array)`

This would make aggregations like `sum by (src_ip)` orders of magnitude faster, but standard PromQL compatibility is achieved even without this optimization.

## 5. Summary

| User Action | Standard TSDB | Parquet Hybrid |
| :--- | :--- | :--- |
| **Write Query** | `metric{label="val"}` | `metric{label="val"}` (Identical) |
| **Execution** | Index Lookup -> Series Fetch | Index Lookup (Hot) + Column Scan (Cold) |
| **Result** | Series Object | Series Object (Identical) |

You do **not** need to learn SQL or change your dashboards.
