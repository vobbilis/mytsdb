# Hot Tier In-Memory Optimizations

**Context:**
We are adopting a **Hybrid Storage Model**:
-   **Cold Tier:** Apache Parquet (Columnar, On-Disk).
-   **Hot Tier:** Custom C++ Structures (Row-based, In-Memory).

**Goal:**
Since the Hot Tier handles all writes and recent queries, it must be extremely memory-efficient and fast. We will apply advanced data structures to optimize this layer.

## 1. The Inverted Index (Roaring Bitmaps)

The Inverted Index maps `Tag -> List<SeriesID>`.
*   **Current State:** Likely `std::map<std::string, std::vector<uint64_t>>`.
*   **Problem:** `std::vector<uint64_t>` is memory-inefficient for sparse data and slow for set intersections (AND queries).
*   **Optimization:** **Roaring Bitmaps**.
    *   **What:** A compressed bitmap data structure.
    *   **Benefit:**
        *   **Compression:** Can compress lists of integers by 10x-100x.
        *   **Speed:** Set operations (AND, OR, NOT) are up to 100x faster than vector intersections.
    *   **Implementation:** Use `CRoaring` (C implementation) or a C++ wrapper.

## 2. String Interning (Dictionary Encoding)

*   **Current State:** Each `TimeSeries` object likely stores its own copy of label strings (e.g., `host="server-1"`).
*   **Problem:** If you have 1M series with `region="us-east-1"`, you store that string 1M times.
*   **Optimization:** **Global String Pool**.
    *   **What:** A central registry of unique strings. Series store `uint32_t` IDs instead of `std::string`.
    *   **Benefit:** Massive memory reduction for high-cardinality tags with repeated values.
    *   **Implementation:** `StringPool` class with `get_id(string)` and `get_string(id)`.

## 3. Memory Layout (Structure of Arrays)

*   **Current State:** `vector<Sample>` where `Sample { int64 t; double v; }`.
*   **Problem:** Poor cache locality for some operations.
*   **Optimization:** **Chunked Arrays**.
    *   **What:** Store data in blocks of 128 or 256 samples.
    *   **Benefit:** Better CPU cache utilization and easier integration with SIMD instructions.

## 4. Off-Heap Memory (Arena Allocation)

*   **Current State:** `new TimeSeries()` (Heap allocation).
*   **Problem:** Memory fragmentation and GC pressure (if we were Java/Go, but in C++ it's allocator overhead).
*   **Optimization:** **Arena / Slab Allocator**.
    *   **What:** Allocate large blocks of memory (Arenas) and carve out objects from them.
    *   **Benefit:** Zero-overhead allocation/deallocation for batch operations.

## 5. Summary

| Optimization | Target | Benefit |
| :--- | :--- | :--- |
| **Roaring Bitmaps** | Inverted Index | 10x Memory Reduction, 100x Query Speed |
| **String Interning** | Label Storage | 50-90% Memory Reduction |
| **Arena Allocation** | Object Creation | Reduced Fragmentation, Faster Writes |

**Conclusion:**
Even with Parquet for the Cold Tier, these optimizations are **critical** for the Hot Tier to sustain high write throughput (~260k/sec) and low-latency queries for recent data.
