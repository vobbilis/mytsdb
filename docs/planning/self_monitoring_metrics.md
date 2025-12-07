# MyTSDB Self-Monitoring Metrics Reference

This document describes all internal performance metrics exposed by MyTSDB's `SelfMonitor` component via the Prometheus-compatible API at `:9090/api/v1/query`.

## Overview

MyTSDB exposes **35+ internal metrics** covering:
- Query execution performance
- Write path instrumentation
- Read path instrumentation  
- Storage engine statistics

All metrics use the prefix `mytsdb_` and are scraped every 1 second by the `SelfMonitor` thread.

---

## Query Metrics

Track PromQL query execution performance.

| Metric | Type | Description | PromQL Example |
|--------|------|-------------|----------------|
| `mytsdb_query_count_total` | counter | Total queries executed | `mytsdb_query_count_total` |
| `mytsdb_query_errors_total` | counter | Total query errors | `mytsdb_query_errors_total` |
| `mytsdb_query_duration_seconds_total` | counter | Total query time (s) | `rate(mytsdb_query_duration_seconds_total[1m])` |
| `mytsdb_query_parse_duration_seconds_total` | counter | Total parse time | `mytsdb_query_parse_duration_seconds_total` |
| `mytsdb_query_eval_duration_seconds_total` | counter | Total evaluation time | `mytsdb_query_eval_duration_seconds_total` |
| `mytsdb_query_exec_duration_seconds_total` | counter | Total execution time | `mytsdb_query_exec_duration_seconds_total` |
| `mytsdb_query_storage_read_duration_seconds_total` | counter | Storage read time | `mytsdb_query_storage_read_duration_seconds_total` |
| `mytsdb_query_samples_scanned_total` | counter | Samples scanned | `rate(mytsdb_query_samples_scanned_total[1m])` |
| `mytsdb_query_series_scanned_total` | counter | Series scanned | `mytsdb_query_series_scanned_total` |
| `mytsdb_query_bytes_scanned_total` | counter | Bytes scanned | `mytsdb_query_bytes_scanned_total` |
| `mytsdb_query_duration_seconds_bucket` | histogram | Query latency histogram | See P99 section |

### Derived Queries

```promql
# Average query latency
mytsdb_query_duration_seconds_total / mytsdb_query_count_total

# Query error rate
rate(mytsdb_query_errors_total[5m]) / rate(mytsdb_query_count_total[5m])

# Query P99 latency (requires histogram)
histogram_quantile(0.99, mytsdb_query_duration_seconds_bucket)

# Query P50 latency
histogram_quantile(0.50, mytsdb_query_duration_seconds_bucket)
```

---

## Write Metrics

Track write path performance including lock contention.

| Metric | Type | Description | PromQL Example |
|--------|------|-------------|----------------|
| `mytsdb_write_mutex_lock_seconds_total` | counter | **Mutex wait time** | `mytsdb_write_mutex_lock_seconds_total` |
| `mytsdb_write_sample_append_seconds_total` | counter | Sample append I/O | `mytsdb_write_sample_append_seconds_total` |
| `mytsdb_write_wal_write_seconds_total` | counter | WAL write time | `mytsdb_write_wal_write_seconds_total` |
| `mytsdb_write_series_id_calc_seconds_total` | counter | Series ID hash calc | `mytsdb_write_series_id_calc_seconds_total` |
| `mytsdb_write_index_insert_seconds_total` | counter | Index insertion | `mytsdb_write_index_insert_seconds_total` |
| `mytsdb_write_series_creation_seconds_total` | counter | New series creation | `mytsdb_write_series_creation_seconds_total` |
| `mytsdb_write_map_insert_seconds_total` | counter | Map insertion | `mytsdb_write_map_insert_seconds_total` |
| `mytsdb_write_block_seal_seconds_total` | counter | Block seal time | `mytsdb_write_block_seal_seconds_total` |
| `mytsdb_write_block_persist_seconds_total` | counter | Block persist I/O | `mytsdb_write_block_persist_seconds_total` |
| `mytsdb_write_cache_update_seconds_total` | counter | Cache update time | `mytsdb_write_cache_update_seconds_total` |

### OTEL/gRPC Overhead Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `mytsdb_write_otel_conversion_seconds_total` | counter | OTEL proto conversion |
| `mytsdb_write_grpc_handling_seconds_total` | counter | gRPC handling overhead |
| `mytsdb_write_otel_resource_processing_seconds_total` | counter | Resource processing |
| `mytsdb_write_otel_scope_processing_seconds_total` | counter | Scope processing |
| `mytsdb_write_otel_metric_processing_seconds_total` | counter | Metric processing |
| `mytsdb_write_otel_label_conversion_seconds_total` | counter | Label conversion |
| `mytsdb_write_otel_point_conversion_seconds_total` | counter | Point conversion |

### Derived Queries

```promql
# Mutex contention percentage (KEY BOTTLENECK INDICATOR)
mytsdb_write_mutex_lock_seconds_total / (
  mytsdb_write_mutex_lock_seconds_total +
  mytsdb_write_sample_append_seconds_total +
  mytsdb_write_wal_write_seconds_total +
  mytsdb_write_block_persist_seconds_total
) * 100

# Write I/O rate
rate(mytsdb_write_sample_append_seconds_total[1m])

# Block persist rate
rate(mytsdb_write_block_persist_seconds_total[1m])
```

---

## Read Metrics

Track storage read path performance.

| Metric | Type | Description | PromQL Example |
|--------|------|-------------|----------------|
| `mytsdb_read_total` | counter | Total read operations | `rate(mytsdb_read_total[1m])` |
| `mytsdb_read_duration_seconds_total` | counter | Total read time | `mytsdb_read_duration_seconds_total` |
| `mytsdb_read_index_search_seconds_total` | counter | Index search time | `mytsdb_read_index_search_seconds_total` |
| `mytsdb_read_block_lookup_seconds_total` | counter | Block lookup time | `mytsdb_read_block_lookup_seconds_total` |
| `mytsdb_read_block_read_seconds_total` | counter | Block read I/O | `mytsdb_read_block_read_seconds_total` |
| `mytsdb_read_decompression_seconds_total` | counter | Decompression time | `mytsdb_read_decompression_seconds_total` |
| `mytsdb_read_samples_scanned_total` | counter | Samples scanned | `mytsdb_read_samples_scanned_total` |
| `mytsdb_read_blocks_accessed_total` | counter | Blocks accessed | `mytsdb_read_blocks_accessed_total` |
| `mytsdb_read_cache_hits_total` | counter | Cache hits | `mytsdb_read_cache_hits_total` |

### Secondary Index Metrics (Phase A: B+ Tree)

Track the B+ tree secondary index performance for cold storage (Parquet) queries.

| Metric | Type | Description | PromQL Example |
|--------|------|-------------|----------------|
| `mytsdb_secondary_index_lookups_total` | counter | Total index lookups | `rate(mytsdb_secondary_index_lookups_total[1m])` |
| `mytsdb_secondary_index_hits_total` | counter | Series found via index | `mytsdb_secondary_index_hits_total` |
| `mytsdb_secondary_index_misses_total` | counter | Index misses (fallback to scan) | `mytsdb_secondary_index_misses_total` |
| `mytsdb_secondary_index_lookup_seconds_total` | counter | Index lookup time | `mytsdb_secondary_index_lookup_seconds_total` |
| `mytsdb_secondary_index_build_seconds_total` | counter | Index build time | `mytsdb_secondary_index_build_seconds_total` |
| `mytsdb_secondary_index_row_groups_selected_total` | counter | Row groups selected via index | `mytsdb_secondary_index_row_groups_selected_total` |

### Derived Queries

```promql
# Average read latency
mytsdb_read_duration_seconds_total / mytsdb_read_total

# Read cache hit rate
mytsdb_read_cache_hits_total / mytsdb_read_total * 100

# Read throughput (samples/sec)
rate(mytsdb_read_samples_scanned_total[1m])

# Secondary Index hit rate (key efficiency metric)
mytsdb_secondary_index_hits_total / mytsdb_secondary_index_lookups_total * 100

# Average index lookup time (microseconds)
mytsdb_secondary_index_lookup_seconds_total * 1e6 / mytsdb_secondary_index_lookups_total

# Row groups skipped via index (efficiency gain)
mytsdb_secondary_index_row_groups_selected_total / mytsdb_read_total
```

---

## Storage Metrics

Track overall storage engine statistics.

| Metric | Type | Description | PromQL Example |
|--------|------|-------------|----------------|
| `mytsdb_storage_writes_total` | counter | Total storage writes | `rate(mytsdb_storage_writes_total[1m])` |
| `mytsdb_storage_reads_total` | counter | Total storage reads | `mytsdb_storage_reads_total` |
| `mytsdb_storage_cache_hits_total` | counter | Cache hits | `mytsdb_storage_cache_hits_total` |
| `mytsdb_storage_cache_misses_total` | counter | Cache misses | `mytsdb_storage_cache_misses_total` |
| `mytsdb_storage_bytes_written_total` | counter | Total bytes written | `rate(mytsdb_storage_bytes_written_total[1m])` |
| `mytsdb_storage_bytes_read_total` | counter | Total bytes read | `mytsdb_storage_bytes_read_total` |
| `mytsdb_storage_net_memory_usage_bytes` | gauge | Current memory usage | `mytsdb_storage_net_memory_usage_bytes / 1024 / 1024` |

### Derived Queries

```promql
# Storage cache hit rate
mytsdb_storage_cache_hits_total / (mytsdb_storage_cache_hits_total + mytsdb_storage_cache_misses_total) * 100

# Write throughput (bytes/sec)
rate(mytsdb_storage_bytes_written_total[1m])

# Memory usage in MB
mytsdb_storage_net_memory_usage_bytes / 1024 / 1024
```

---

## Grafana Dashboard Panels (Suggested)

### 1. Overview Row
| Panel | Query | Visualization |
|-------|-------|---------------|
| Query Rate | `rate(mytsdb_query_count_total[1m])` | Stat |
| Query P99 | `histogram_quantile(0.99, mytsdb_query_duration_seconds_bucket)` | Gauge (threshold: 50ms) |
| Write Mutex % | `mytsdb_write_mutex_lock_seconds_total / (...) * 100` | Gauge (threshold: 50%) |
| Memory Usage | `mytsdb_storage_net_memory_usage_bytes / 1024 / 1024` | Stat |

### 2. Query Performance Row
| Panel | Query |
|-------|-------|
| Query Latency Distribution | `histogram_quantile(0.99, ...)` and `histogram_quantile(0.50, ...)` |
| Query Error Rate | `rate(mytsdb_query_errors_total[1m])` |
| Samples Scanned/sec | `rate(mytsdb_query_samples_scanned_total[1m])` |

### 3. Write Performance Row
| Panel | Query |
|-------|-------|
| Write Breakdown (stacked) | All `mytsdb_write_*_seconds_total` |
| Mutex Contention % | See derived query above |
| Block Persist Rate | `rate(mytsdb_write_block_persist_seconds_total[1m])` |

### 4. Storage Row
| Panel | Query |
|-------|-------|
| Cache Hit Rate | See derived query |
| Bytes Written/Read | `rate(mytsdb_storage_bytes_*[1m])` |
| Memory Usage | `mytsdb_storage_net_memory_usage_bytes` |

---

## API Endpoint

```bash
# Query a single metric
curl 'http://localhost:9090/api/v1/query?query=mytsdb_query_count_total'

# Query with label filter
curl 'http://localhost:9090/api/v1/query?query={job="mytsdb_self_monitor"}'

# Query histogram percentile
curl 'http://localhost:9090/api/v1/query?query=histogram_quantile(0.99,mytsdb_query_duration_seconds_bucket)'
```

---

## Source Code Reference

- **SelfMonitor Implementation**: `src/tsdb/server/self_monitor.cpp`
- **QueryMetrics**: `include/tsdb/prometheus/promql/query_metrics.h`
- **WritePerformanceInstrumentation**: `include/tsdb/storage/write_performance_instrumentation.h`
- **Histogram**: `include/tsdb/histogram/histogram.h`
