# Simplified Architecture: The High-Performance Monolith

**Status:** Draft (Alternative Proposal)
**Date:** 2025-11-30
**Author:** Antigravity

## 1. The Philosophy: Scale Up, Not Out

The previous proposal (Cortex-style) optimizes for *infinite* scale at the cost of massive complexity. However, modern hardware (NVMe, 100+ core CPUs) allows a **Single Node** to handle millions of metrics per second if the software is efficient.

**Our Thesis:** We can achieve 99% of the required scale with a **Single, Highly-Optimized C++ Binary**, avoiding the "Microservices Tax" entirely.

## 2. Architecture: The "Modern Monolith"

Instead of splitting into Ingesters, Queriers, and Distributors, we have **One Binary** (`mytsdb`) that does it all.

### 2.1 Tiered Storage Engine
We adopt a "Hot/Cold" architecture within a single process:

1.  **Hot Tier (NVMe/SSD)**:
    -   **Write-Ahead Log (WAL)**: For durability.
    -   **Head Block**: In-memory, mutable buffer.
    -   **Recent Blocks**: Immutable, compressed blocks stored on fast local disk.
    -   **Format**: Custom Row-based or Columnar (fast append).

2.  **Cold Tier (Object Storage / S3)**:
    -   **Async Archival**: Background threads transparently upload sealed blocks to S3.
    -   **Format**: **Apache Parquet** (for interoperability and compression).
    -   **Access**: Queries seamlessly fetch data from S3 if not found locally.

### 2.2 Data Flow

```mermaid
graph LR
    Client -->|Write| Node[MyTSDB Node]
    Client -->|Query| Node
    
    subgraph "MyTSDB Node (Single Process)"
        WAL[WAL (NVMe)]
        Mem[Memory Table]
        Local[Local Blocks (NVMe)]
    end
    
    Node -->|Async Upload| S3[Object Storage (Parquet)]
    Node -->|Async Read| S3
```

## 3. How It Scales (Without Microservices)

### 3.1 Vertical Scaling (The Primary Strategy)
Since MyTSDB is written in C++, we optimize for **Single-Node Performance**:
-   **SIMD Instructions**: Use AVX-512 for compression/decompression and filtering.
-   **Async I/O**: Use `io_uring` (Linux) for non-blocking disk/network I/O.
-   **Lock-Free Structures**: Minimize contention.

**Target**: 1 Million writes/sec on a single 64-core instance. (This covers almost all use cases).

### 3.2 Simple Clustering (The "ClickHouse" Model)
If you *must* scale beyond one node:
1.  **Sharding**: Run N independent `mytsdb` nodes.
2.  **Routing**: A simple Load Balancer (Nginx) or smart client hashes series to nodes.
3.  **Shared Nothing**: Nodes don't talk to each other. They just share the S3 bucket for backup.
4.  **Query Federation**: A lightweight "Query Proxy" (or any node) can scatter-gather queries if needed.

## 4. Multi-Tenancy in the Monolith

We keep the **Hard Multi-tenancy** design (Tenant-per-directory), but it's managed within the single process.
-   `data/tenant-A/`
-   `data/tenant-B/`

## 5. Comparison: Cortex vs. Modern Monolith

| Feature | Cortex / Microservices | Modern Monolith (Proposed) |
| :--- | :--- | :--- |
| **Complexity** | High (5+ services, KV store, etc.) | **Low** (1 binary, S3) |
| **Deployment** | Kubernetes, Helm, complex wiring | `systemd` or simple Docker |
| **Efficiency** | Lower (RPC overhead, serialization) | **High** (Zero-copy, in-process) |
| **Scalability** | Infinite (add nodes) | High (add cores/RAM, then shard) |
| **Maintenance** | Hard (distributed debugging) | Easy (single process) |

## 6. Conclusion

We should **reject** the Cortex clone approach. It is over-engineered for our needs.

**Recommendation:** Build the **High-Performance Monolith**.
1.  Focus on **Parquet** integration for S3 storage.
2.  Optimize the C++ engine for raw speed.
3.  Keep deployment simple: One Binary, One Config, S3 Backend.
