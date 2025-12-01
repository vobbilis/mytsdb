# Test Plan: Modern Monolith Refactor

**Status:** Draft
**Goal:** Ensure correctness, performance, and observability of the new "Modern Monolith" architecture.

## 1. Unit Testing Strategy
*Goal: Verify individual components in isolation.*

- [ ] **S3Store Tests**
    -   Mock the AWS SDK.
    -   Verify `Put`, `Get`, `Range`, `Delete` handle errors (network, auth) correctly.
    -   Verify retry logic.

- [ ] **Parquet Integration Tests**
    -   Create sample `TimeSeries` data.
    -   Write to Parquet buffer using `ParquetBlockWriter`.
    -   Read back using `ParquetBlockReader`.
    -   **Assert**: Data matches exactly (timestamps, values, labels).
    -   **Assert**: Compression ratio is within expected bounds.

- [ ] **Async WAL Tests**
    -   Simulate high-concurrency writes.
    -   Verify no data loss on "crash" (simulate flush interruption).
    -   Verify group commit logic (batching).

- [ ] **Tenant Isolation Tests**
    -   Create two tenants: `TenantA` and `TenantB`.
    -   Write series `metric_1` to `TenantA`.
    -   Attempt to read `metric_1` from `TenantB`.
    -   **Assert**: Series not found in `TenantB`.
    -   **Assert**: Data directories are separate (`data/TenantA` vs `data/TenantB`).

## 2. Integration Testing Strategy
*Goal: Verify components work together (End-to-End).*

- [ ] **Hot/Cold Tier E2E**
    -   **Setup**: Start MyTSDB with `FilesystemStore` (mocking S3).
    -   **Action**: Write data for 2 hours (force block rotation).
    -   **Wait**: Wait for `Archiver` to upload sealed blocks.
    -   **Action**: Query data from 1 hour ago (Hot Tier).
    -   **Action**: Query data from 3 hours ago (Cold Tier).
    -   **Assert**: Both queries return correct data.
    -   **Assert**: "Cold" data files exist in the mock S3 directory.

- [ ] **Self-Monitoring Verification**
    -   **Setup**: Start MyTSDB.
    -   **Action**: Perform 1000 writes and 100 queries.
    -   **Action**: Scrape `/metrics`.
    -   **Assert**: `mytsdb_wal_write_bytes_total` > 0.
    -   **Assert**: `mytsdb_http_requests_total` reflects the traffic.
    -   **Assert**: No "missing metric" errors.

## 3. Performance Benchmarking
*Goal: Validate the "High Performance" claim.*

- [ ] **Write Throughput Benchmark**
    -   **Tool**: `tsbs` (Time Series Benchmark Suite) or custom load generator.
    -   **Scenario**: 1M unique series, high churn.
    -   **Target**: > 500k samples/sec on local dev machine.
    -   **Profile**: Use `perf` / `Instruments` to identify CPU hotspots during test.

- [ ] **Query Latency Benchmark**
    -   **Scenario**: High-cardinality query (regex match).
    -   **Target**: P99 < 100ms for hot data.
    -   **Target**: P99 < 500ms for cold data (S3).

## 4. Failure Injection Testing
*Goal: Ensure resilience.*

- [ ] **S3 Outage**
    -   Simulate S3 being unreachable.
    -   **Assert**: `Archiver` retries and does not crash.
    -   **Assert**: Hot Tier continues to accept writes (WAL).

- [ ] **Disk Full**
    -   Simulate full disk.
    -   **Assert**: System rejects writes gracefully (no corruption).

## 5. Continuous Integration (CI)
-   Add `S3Store` mock tests to GitHub Actions.
-   Run `Self-Monitoring Verification` on every PR.
