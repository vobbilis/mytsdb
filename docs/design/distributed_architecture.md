# Distributed Scale-Out Architecture Proposal for MyTSDB

**Status:** Draft v2 (Multi-Tenancy Integrated)
**Date:** 2025-11-30
**Author:** Antigravity

## 1. Executive Summary

To evolve MyTSDB from a single-node high-performance database into a cloud-native, horizontally scalable system, we propose a **Distributed Microservices Architecture**. This design separates the concerns of ingestion, querying, and long-term storage, allowing each to scale independently based on workload demands.

This proposal adopts the **Shared Storage** pattern (popularized by Cortex, Thanos, and InfluxDB IOx), where a scalable Object Store (S3, GCS, MinIO) serves as the single source of truth for historical data, while stateless/semi-stateful microservices handle compute and coordination.

**Multi-Tenancy:** This architecture is designed from the ground up to be multi-tenant. Every component is "Tenant-Aware," ensuring strict data isolation and fair resource usage.

## 2. Architecture Overview

The monolithic `StorageImpl` will be refactored into distinct services.

### Core Components

1.  **Distributor (Stateless)**
    -   **Role:** Entry point for writes.
    -   **Function:** Validates data, hashes series (by labels), and routes them to the appropriate Ingester(s) using a consistent hash ring.
    -   **Multi-Tenancy:**
        -   Extracts `X-Scope-OrgID` from requests.
        -   Enforces per-tenant rate limits (requests/sec, bytes/sec).
        -   Forwards `TenantID` to Ingesters.
    -   **Scalability:** Horizontal (add more replicas behind a load balancer).

2.  **Ingester (Semi-Stateful)**
    -   **Role:** Handles high-throughput writes and buffers "hot" data.
    -   **Function:**
        -   Writes to local WAL for durability.
        -   Builds in-memory blocks (Head blocks).
        -   Serves read queries for recent data (in-memory).
        -   Periodically flushes sealed blocks to Object Storage.
    -   **Multi-Tenancy:**
        -   Maintains a map of `TenantID -> TenantTSDB` (isolated memory structures).
        -   Ensures data from Tenant A never touches Tenant B's structures.
        -   Flushes blocks to tenant-specific paths in S3 (`s3://bucket/tenant-A/...`).
    -   **Scalability:** Horizontal (sharded by series hash).

3.  **Querier (Stateless)**
    -   **Role:** Entry point for PromQL queries.
    -   **Function:**
        -   Parses and plans queries.
        -   **Fan-out:** Requests recent data from **Ingesters** and historical data from **Store Gateways**.
        -   **Deduplication:** Merges and deduplicates results from overlapping sources.
        -   Executes PromQL engine logic (aggregation, functions).
    -   **Multi-Tenancy:**
        -   Extracts `X-Scope-OrgID` from query requests.
        -   Propagates `TenantID` in gRPC calls to Ingesters and Store Gateways.
        -   Enforces per-tenant query limits (max series, max samples).
    -   **Scalability:** Horizontal (CPU bound).

4.  **Store Gateway (Stateless/Cached)**
    -   **Role:** Gateway to historical data in Object Storage.
    -   **Function:**
        -   Indexes the content of the Object Store.
        -   Downloads only relevant block headers/indices (lazy loading).
        -   Serves series data from S3 to Queriers.
        -   Uses local disk/memory for caching hot blocks.
    -   **Multi-Tenancy:**
        -   Only loads blocks belonging to the requested `TenantID`.
        -   Scans specific S3 prefixes: `s3://bucket/<tenant_id>/...`.
    -   **Scalability:** Horizontal (sharded by block ID or time range).

5.  **Compactor (Background Job)**
    -   **Role:** Optimization and retention.
    -   **Function:**
        -   Merges small blocks in Object Storage into larger, more efficient blocks.
        -   Applies retention policies (deletes old data).
        -   Downsamples data (optional future feature).
    -   **Multi-Tenancy:**
        -   Iterates over all tenants in the bucket.
        -   Runs compaction jobs per-tenant (Tenant A's blocks are never merged with Tenant B's).
    -   **Scalability:** Vertical/Horizontal (worker pool).

6.  **Object Storage (The "Truth")**
    -   **Role:** Infinite durability and storage.
    -   **Function:** Stores immutable data blocks (Parquet or custom TSDB format) and indices.
    -   **Layout:** `/data/<tenant_id>/<block_id>/...`

---

## 3. Data Flow

### 3.1 Write Path (Hot Path)
```mermaid
graph LR
    Client[Client/Agent] -->|Remote Write + OrgID| LB[Load Balancer]
    LB --> Distributor
    Distributor -->|Hash Ring + OrgID| Ingester1[Ingester A]
    Distributor -->|Replication + OrgID| Ingester2[Ingester B]
    
    subgraph "Ingester Node"
        Ingester1 -->|Log| WAL[Local WAL (Tenant A)]
        Ingester1 -->|Buffer| Mem[Head Block (Tenant A)]
    end
    
    Mem -->|Flush (Every 2h)| S3[Object Storage / Tenant A]
```
1.  **Client** sends write request with `X-Scope-OrgID`.
2.  **Distributor** validates limits for that OrgID and hashes series labels.
3.  **Ingester** receives the write, looks up the `TenantTSDB` for that OrgID, and appends to its specific WAL and Head Block.
4.  **Persistence**: Ingester flushes blocks to `s3://bucket/<OrgID>/...`.

### 3.2 Read Path
```mermaid
graph TD
    Client[Grafana/User] -->|PromQL + OrgID| Querier
    
    Querier -->|gRPC (Recent Data) + OrgID| Ingester
    Querier -->|gRPC (Historical) + OrgID| StoreGW[Store Gateway]
    
    Ingester -->|Read| Mem[Memory/WAL (Tenant A)]
    StoreGW -->|Fetch| S3[Object Storage / Tenant A]
    
    Querier -->|Merge & Agg| Result
```
1.  **Querier** receives query with `X-Scope-OrgID`.
2.  **Fan-out**: Sends gRPC requests including the `OrgID` metadata.
3.  **Ingester**: Reads only from the in-memory structures of that specific `OrgID`.
4.  **Store Gateway**: Scans only the S3 prefix for that `OrgID`.

---

## 4. Component Implementation Strategy

### 4.1 Refactoring `StorageImpl`
Currently, `StorageImpl` is a monolith. We will refactor it into a library `libtsdb` that can be configured for different modes.

-   **Ingester Mode**:
    -   Enable WAL.
    -   Enable `TimeSeriesPool`.
    -   Disable `BlockManager` disk persistence (replace with S3 uploader).
    -   Retention: Keep data only until flushed to S3.

-   **Store Gateway Mode**:
    -   Disable WAL.
    -   Disable active writes.
    -   Enable `BlockManager` with S3 backend.
    -   Enable heavy Caching (L2/L3) for S3 data.

### 4.2 New Services (gRPC)
We need to define Protobuf service definitions for internal communication:

```protobuf
// Internal API for Querier -> Ingester/StoreGateway
service StorageQueryService {
  // Metadata: X-Scope-OrgID required
  rpc Query (QueryRequest) returns (stream TimeSeriesChunk);
  rpc LabelNames (LabelNamesRequest) returns (LabelNamesResponse);
  rpc LabelValues (LabelValuesRequest) returns (LabelValuesResponse);
}

// Internal API for Distributor -> Ingester
service IngestService {
  // Metadata: X-Scope-OrgID required
  rpc Push (WriteRequest) returns (WriteResponse);
}
```

### 4.3 Storage Format Evolution
Currently, we use a custom block format. To leverage the ecosystem (like InfluxDB IOx), we should consider:
-   **Current**: Custom TSDB block (Header + Compressed Chunks). Good for now.
-   **Future**: **Apache Parquet**.
    -   Self-describing.
    -   Columnar (excellent for OLAP/Analytics).
    -   Compatible with tools like DuckDB, Spark, Pandas.
    -   *Recommendation*: Stick to custom format for Phase 1 (speed), migrate to Parquet for Phase 2 (interoperability).

---

## 5. Migration Roadmap

### Phase 1: "The Split" (Monolith -> Microservices)
1.  **Define gRPC Interfaces**: Create `proto/internal.proto`.
2.  **Create `cmd/ingester`**: Wraps `StorageImpl` (write-only mode).
3.  **Create `cmd/querier`**: Wraps `PromQLEngine` + gRPC Client (read-only).
4.  **Create `cmd/store-gateway`**: Wraps `BlockManager` (read-only from disk for now).
5.  **Manual Wiring**: Run 1 Ingester, 1 Querier, 1 Store Gateway locally.

### Phase 2: "The Cloud" (Disk -> S3)
1.  **Implement `ObjectStore` Interface**: Abstract file system operations.
2.  **S3 Backend**: Implement S3/MinIO adapter.
3.  **Ingester Flush**: Implement "Upload to S3" logic.
4.  **Store Gateway Fetch**: Implement "Download/Cache from S3" logic.

### Phase 3: "The Scale" (Sharding & Replication)
1.  **Implement Distributor**: Consistent hash ring.
2.  **Replication**: Write to N=3 Ingesters.
3.  **Compactor**: Merge blocks in S3.

## 6. Conclusion
This architecture aligns MyTSDB with modern, proven designs like Cortex and Thanos. It solves the "single node bottleneck" and allows us to scale ingestion (add Ingesters) and query performance (add Queriers) independently, while leveraging low-cost Object Storage for infinite retention.
