# Multi-Tenancy Design for MyTSDB

**Status:** Draft
**Date:** 2025-11-30
**Author:** Antigravity

## 1. Executive Summary

To support SaaS use cases and team isolation, MyTSDB needs **Multi-Tenancy**. This design proposes a "Hard Multi-tenancy" model similar to Cortex and Mimir, where data is logically and physically isolated per tenant throughout the stack.

## 2. Architecture Overview

We will adopt the **Tenant-per-Request** model:
1.  **Identification**: Tenants are identified by a string ID (e.g., `X-Scope-OrgID` header).
2.  **Isolation**: Each tenant has its own isolated "View" of the database (Index, WAL, Blocks).
3.  **Storage**: Data in Object Storage is partitioned by tenant prefix.

## 3. Detailed Design

### 3.1 API Layer (Authentication)
We already have `HeaderAuthenticator` which extracts `X-Scope-OrgID`.
-   **Change**: The `WriteHandler` and `ReadHandler` must extract this `tenant_id` from the context/auth and pass it to the Storage layer.
-   **Default**: If no header is present (and auth is disabled), use a default tenant `anonymous`.

### 3.2 Storage Layer (The Core Change)

Currently, `StorageImpl` is a monolith. We will refactor it to manage multiple "Tenant Storage" instances.

#### New Interface
```cpp
class MultiTenantStorage : public Storage {
public:
    // Tenant-aware methods
    core::Result<void> write(const std::string& tenant_id, const core::TimeSeries& series);
    
    core::Result<core::TimeSeries> read(
        const std::string& tenant_id,
        const core::Labels& labels,
        int64_t start, int64_t end);
        
    // ... other methods take tenant_id
};
```

#### Internal Structure
`StorageImpl` will become a **Registry** of tenants.

```cpp
class StorageImpl {
private:
    // Map of Tenant ID -> Tenant's private TSDB
    tbb::concurrent_hash_map<std::string, std::shared_ptr<TenantTSDB>> tenants_;
    
    // Configuration for new tenants
    core::StorageConfig default_config_;
    
    // Get or Create Tenant
    std::shared_ptr<TenantTSDB> get_tenant(const std::string& tenant_id) {
        // Double-checked locking or concurrent map logic to create on demand
    }
};

// Represents a single tenant's isolated world
class TenantTSDB {
    std::unique_ptr<Index> index_;
    std::unique_ptr<WriteAheadLog> wal_;
    std::unique_ptr<BlockManager> block_manager_;
    std::unique_ptr<CacheHierarchy> cache_;
    // ...
};
```

### 3.3 Data Layout

#### On Disk (WAL)
```
/data/wal/
  ├── tenant-A/
  │   ├── 000001.log
  │   └── ...
  ├── tenant-B/
  │   └── ...
```

#### In Object Storage (S3)
```
s3://mytsdb-bucket/
  ├── tenant-A/
  │   ├── blocks/
  │   │   ├── 01H8.../
  │   │   └── ...
  │   └── index/
  ├── tenant-B/
  │   └── ...
```

### 3.4 Resource Management (Limits)

With `TenantTSDB` separated, we can easily implement per-tenant limits:
-   **Max Active Series**: Check `tenant->index_->size()` before insert.
-   **Ingestion Rate**: Rate limiter per `TenantTSDB`.
-   **Retention**: Different retention policies per tenant (configurable).

## 4. Migration Plan

1.  **Refactor `StorageImpl`**: Rename current `StorageImpl` to `TenantTSDB`.
2.  **Create Wrapper**: Create new `StorageImpl` that manages a map of `TenantTSDB`.
3.  **Update Handlers**: Update `WriteHandler` to pass `tenant_id`.
4.  **Update Tests**: All tests currently assume single tenant. Update them to use "default" tenant.

## 5. Pros/Cons

**Pros:**
-   **Strong Isolation**: One tenant's index corruption doesn't affect others.
-   **Easy Deletion**: To delete a tenant, just drop their `TenantTSDB` and delete their directory.
-   **Fairness**: Easier to enforce limits.

**Cons:**
-   **Overhead**: Each tenant has its own WAL, Index, Cache overhead. (Mitigation: Shared Cache, but separate Index/WAL is worth it).
-   **Complexity**: Managing lifecycle of dynamic tenants.

## 6. Conclusion
This "Hard Multi-tenancy" design is the industry standard for scalable TSDBs (Cortex, Thanos). It provides the safety and management features required for a production-grade system.
