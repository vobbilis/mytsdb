# Verification Investigation Findings

**Date:** 2025-11-23  
**Status:** 🔍 **In Progress** - Root Cause Identified

## Summary

Investigation of verification failures in OTEL/gRPC benchmark reveals that **WAL file exists but is 0 bytes**, preventing verification storage from replaying written data.

## Findings

### ✅ What Works
1. **gRPC Export**: Server receives and processes requests correctly
2. **Bridge Conversion**: 40 attributes → 41 labels correctly converted
3. **Storage Write**: Data is written to in-memory index (server logs show "Added series ... with 41 labels")
4. **Debug Test**: Single metric write/verify works correctly

### ❌ What's Broken
1. **WAL File**: Exists but is **0 bytes** - no data written to disk
2. **Verification**: 0% success rate - queries find 0 series
3. **WAL Replay**: Verification storage replays empty WAL, so index is empty

## Root Cause

**WAL file is created but never written to.**

Evidence:
- WAL file exists: `/tmp/tsdb_verify_debug3_*/wal/wal_000000.log`
- File size: **0 bytes**
- Benchmark shows `successful_writes > 0` (gRPC Export succeeds)
- No WAL write errors reported

## Investigation Details

### Code Path
1. `MetricsService::Export()` receives request ✅
2. `Bridge::ConvertMetrics()` converts to TimeSeries ✅
3. `StorageImpl::write()` is called ✅
4. `wal_->log(series)` is called ✅
5. `write_to_segment()` should write to file ❌ (file stays 0 bytes)

### Possible Causes
1. **File not synced to disk**: `current_file_.flush()` only flushes C++ stream buffer, not necessarily to disk
2. **File handle issue**: File might be opened but writes aren't persisting
3. **Error swallowing**: WAL write errors might be caught and ignored somewhere
4. **Race condition**: File might be closed/reset before writes complete

## Next Steps

1. **Add fsync() to WAL writes**: Ensure data is synced to disk
2. **Add error logging**: Log WAL write failures to identify issues
3. **Check file handle state**: Verify file is open and writable before writes
4. **Test WAL writes directly**: Create minimal test to isolate WAL write issue

## Impact

- **Verification**: Cannot verify data integrity (0% success rate)
- **Performance**: Cannot measure true performance (data might not be persisted)
- **Reliability**: Data might be lost if server crashes before WAL is synced

## Related Issues

- Block persistence errors in logs (separate issue, but related to disk I/O)
- Performance degradation with 40-label metrics (separate optimization issue)

