# HTTP Query Verification Investigation Summary

## Date: November 24, 2025

## Problem
HTTP query verification was failing with "Time range exceeds maximum allowed" error, even though:
1. The error message didn't exist in the source code
2. Clean rebuilds were performed
3. Extensive debugging was added

## Investigation Timeline
The investigation took approximately 24 hours and explored many dead ends:
- Tracing the full write path
- Tracing the full read path  
- Adding file-based logging
- Checking storage unit tests (which PASS)
- Adding debug prints everywhere
- Checking regex pattern matching
- Verifying storage instance sharing

## Root Cause
**Multiple old `tsdb_server` processes were running** from previous test runs.

These zombie processes were:
- Listening on port 9090
- Responding to HTTP requests
- Returning stale error messages from OLD compiled code

The new binary with fixes was never actually handling the requests.

## Evidence
```bash
$ lsof -i :9090
COMMAND     PID     USER   FD   TYPE
tsdb_serv  4289 vobbilis   11u  IPv4  ...
tsdb_serv  6063 vobbilis   11u  IPv4  ...
tsdb_serv  6147 vobbilis   11u  IPv4  ...
tsdb_serv  8557 vobbilis   11u  IPv4  ...
# 9+ old processes still running!
```

## Solution
1. Kill all existing `tsdb_server` processes before starting tests:
   ```bash
   pkill -9 -f tsdb_server
   ```

2. Added automatic cleanup to `run_otel_performance_test.sh`:
   ```bash
   # CRITICAL: Kill any existing tsdb_server processes
   pkill -9 -f tsdb_server 2>/dev/null || true
   sleep 2
   ```

## Verification
After killing old processes, the verification succeeds:
```
✅ VERIFICATION SUCCESS: Metric verified correctly!
   Metric: test_metric_1
   Value: 0.000000
   Labels: 41
```

## Lessons Learned

### 1. Always Check for Zombie Processes
When debugging server issues, ALWAYS check if old server processes are running:
```bash
lsof -i :<PORT>
ps aux | grep tsdb_server
```

### 2. Trust the Unit Tests
The storage unit tests (`StorageTest.MultipleSeries`) pass and verify write+query works. If unit tests pass but integration fails, the issue is likely environmental.

### 3. Binary/Source Mismatch Signs
If you see error messages that don't exist in source code, you're running an old binary or process.

### 4. Test Scripts Should Clean Up Aggressively
Test scripts should kill any existing server processes BEFORE starting, not just rely on cleanup after.

## Working Components Verified
1. ✅ gRPC MetricsService - receives OTEL metrics
2. ✅ OTELBridge - converts to TimeSeries
3. ✅ StorageImpl - writes to index
4. ✅ Index - stores series with labels
5. ✅ HTTP Server - receives queries
6. ✅ QueryHandler - parses matchers
7. ✅ StorageImpl::query - retrieves from index
8. ✅ JSON response - properly formatted

## Performance Results
After fixing, benchmark shows:
- SingleThreadedWrite: 681 μs per write
- BatchWrite/8: 4666 μs for 8 metrics
- All writes successful
- Verification: 41 labels, 1 sample, correct value

