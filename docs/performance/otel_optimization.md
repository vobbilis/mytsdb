# OTEL Conversion Optimization Plan

## Problem Analysis
Granular instrumentation revealed that **99.4%** of the OTEL conversion time is spent in `otel_point_conversion_us`. Crucially, this time **includes** the underlying storage write operations (WAL, Cache, Index).

The current implementation of `ConvertGaugeWithAttributes` (and Sum/Histogram) iterates through data points and calls `storage_->write()` for **every single point**:

```cpp
for (const auto& point : gauge.data_points()) {
    // ... convert labels ...
    auto series = core::TimeSeries(core::Labels(std::move(labels)));
    series.add_sample(...);
    storage_->write(series); // <--- EXPENSIVE: WAL lock, Cache lock, Index lookup per point!
}
```

If an OTEL batch contains multiple samples for the same series (e.g., a time series of CPU usage), we incur the storage overhead $N$ times instead of once.

## Proposed Solution: Batching by Series
We will refactor the conversion logic to group data points by their unique label set (Series ID) before writing to storage.

### Algorithm
1.  Create a temporary map: `std::map<core::Labels, core::TimeSeries>`.
2.  Iterate through all data points in the OTEL metric.
3.  For each point:
    *   Convert attributes to labels.
    *   Merge with base labels.
    *   Look up the label set in the map.
    *   If not found, create a new `TimeSeries` entry.
    *   Add the sample to the `TimeSeries`.
4.  After iterating all points, iterate the map.
5.  Call `storage_->write(series)` for each aggregated `TimeSeries`.

### Expected Impact
- **Reduced WAL Writes**: One WAL entry per series batch instead of per sample.
- **Reduced Cache Lookups**: One L1/L2 cache lookup/insert per series batch.
- **Reduced Index Lookups**: One index lookup per series batch.
- **Reduced Lock Contention**: Fewer acquisitions of WAL and Cache locks.

## Implementation Details

### Files to Modify
- `src/tsdb/otel/bridge.cpp`

### Changes
1.  Modify `ConvertGaugeWithAttributes`, `ConvertSumWithAttributes`, and `ConvertHistogramWithAttributes`.
2.  Introduce a helper to merge labels and find/create the series in the batch map.
3.  Ensure `core::Labels` has a proper comparison operator for use as a map key (it likely does).

### Verification
- Run `benchmark_tool` (unchanged).
- Expect `otel_point_conversion_us` to decrease significantly.
- Expect `wal_write_us`, `cache_update_us`, etc., to decrease significantly (or rather, their count will decrease, reducing total time).

## Risks
- **Memory Usage**: Buffering samples in a map increases temporary memory usage during conversion. Given the batch sizes are typically small (hundreds or thousands), this should be negligible.
- **Label Comparison Cost**: Using `Labels` as a map key involves string comparisons. However, this is likely cheaper than the storage overhead we are avoiding.
