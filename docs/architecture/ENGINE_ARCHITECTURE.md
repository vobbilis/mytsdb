# Engine Architecture

## 🧠 **System Overview**

MyTSDB's engine is a high-performance, vectorized query processing system designed to execute PromQL (Prometheus Query Language) queries with extreme efficiency. It decouples **Compute** (Query Engine) from **Storage** (Data Layer), allowing for independent scaling and optimization.

The engine follows a **Volcano Iterator Model** with vectorized execution, enabling it to process millions of samples per second with minimal overhead.

---

## 🏗️ **Query Engine Architecture**

### **1. PromQL Processing Pipeline**

The query execution flow transforms a raw PromQL string into an optimized execution plan:

```mermaid
graph TD
    A[Raw PromQL Query] -->|Lexer/Parser| B[Abstract Syntax Tree (AST)]
    B -->|Optimizer| C[Optimized Plan]
    C -->|Executor| D[Vectorized Execution]
    D -->|Storage Adapter| E[Data Retrieval]
    E -->|Result Builder| F[Final Response]
```

1.  **Parsing**: The `PromQLParser` converts the query string into an AST.
2.  **Optimization**: The `QueryPlanner` analyzes the AST to identify optimization opportunities (e.g., aggregation pushdown, time-range pruning).
3.  **Execution**: The `Evaluator` executes the plan using a pull-based iterator model.

### **2. Buffered Storage Adapter**

The `BufferedStorageAdapter` is the critical bridge between the Query Engine and the Storage Layer. It implements intelligent caching and data pre-fetching.

#### **PromQL Query Cache (O(1) Lookup)**

To handle high-cardinality workloads (1M+ active series), the adapter uses a sophisticated two-level cache structure:

*   **Level 1: Matcher Map (O(1))**
    *   Key: Serialized Label Matchers (e.g., `{job="api", method="GET"}`)
    *   Value: Vector of Time Ranges
    *   **Benefit**: Constant-time lookup regardless of total cache size.

*   **Level 2: Time Range Vector (O(M))**
    *   Stores `(Start, End)` intervals for the specific matcher.
    *   **Benefit**: Efficiently handles disjoint time ranges (e.g., dashboard refreshes).

**Performance Impact:**
*   **Legacy**: O(N) linear scan -> ~550µs latency (at N=10k)
*   **Optimized**: O(1) map lookup -> **~0.48µs latency**
*   **Speedup**: **>1000x** improvement for high-cardinality queries.

### **3. Aggregation Pushdown**

For heavy aggregations (e.g., `sum(rate(http_requests[5m]))`), the engine pushes the computation down to the storage layer when possible.

*   **Mechanism**: The `StorageImpl` exposes specialized iterators that pre-aggregate data at the block level.
*   **Supported Operators**: `SUM`, `COUNT`, `MIN`, `MAX`, `AVG`.
*   **Benefit**: Reduces data transfer from Storage to Engine by orders of magnitude (e.g., 1000 raw samples -> 1 aggregated point).

---

## ⚡ **Execution Model**

### **Vectorized Processing**
Instead of processing one sample at a time, the engine operates on **Vectors** (batches) of samples. This maximizes CPU cache locality and allows for SIMD optimizations.

### **Concurrency**
*   **Query Level**: Multiple queries are executed concurrently via a thread pool.
*   **Shard Level**: Queries touching multiple shards are scattered-gathered in parallel.

---

## 💾 **Storage Integration**

The engine interacts with the storage layer through a strict interface, abstracting the underlying complexity of the Multi-Tier Storage (Hot/Warm/Cold).

### **Hybrid Query Path**
The engine transparently merges data from all storage tiers:

1.  **Hot Tier (L1 Cache/MemTable)**: Immediate access to the last 2 hours of data.
2.  **Warm Tier (L2 Cache)**: Memory-mapped blocks for recent history (2h - 24h).
3.  **Cold Tier (Parquet)**: Columnar storage for long-term history (>24h).

### **WAL Safety & Recovery**
The engine ensures data consistency during startup and runtime:

*   **Init Phase**: `replay_at_init()` replays the Write-Ahead Log (WAL) without locks to prevent deadlocks during single-threaded startup.
*   **Runtime Phase**: `replay_runtime()` uses strict locking to safely restore data during hot restarts or failure recovery.

---

## 📊 **Performance Characteristics**

| Component | Metric | Value |
|-----------|--------|-------|
| **Query Latency** | P99 (Cached) | < 5ms |
| **Cache Lookup** | Latency | **< 0.5µs** (O(1)) |
| **Throughput** | QPS | > 10,000 QPS |
| **Aggregation** | Speedup | ~785x (Pushdown) |

---

## 🔮 **Future Improvements**

1.  **Distributed Query Execution**: Scatter-gather across multiple nodes.
2.  **Cost-Based Optimizer**: More intelligent selection of pushdown vs. pull-up.
3.  **JIT Compilation**: Compile PromQL expressions to machine code for extreme performance.
