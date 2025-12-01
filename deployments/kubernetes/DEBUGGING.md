# MyTSDB Debugging Guide

## Current Debugging Capabilities

### ✅ What We Have

1. **Comprehensive Logging in WriteHandler**
   - Request ID tracking (`[REQ:123]`)
   - Timing metrics (ms per request)
   - Series and sample counts
   - Authentication results
   - Decompression details
   - Error tracking with context

2. **Kubernetes Log Access**
   - Real-time log streaming
   - Historical log viewing
   - Multi-pod log aggregation

3. **Shell Access**
   - Direct pod access
   - File system inspection
   - Process debugging

4. **Health Checks**
   - Liveness probes
   - Readiness probes
   - `/health` endpoint

### ⚠️ Limitations

- ReadHandler logging (partially added, needs full implementation)
- No metrics endpoint yet
- No debug endpoints yet

## Live Debugging During Tests

### Terminal Setup (Recommended)

Open 4 terminals for comprehensive visibility:

**Terminal 1: MyTSDB Logs (Filtered)**
```bash
cd deployments/kubernetes
make logs-mytsdb-filtered
```

**Terminal 2: OTEL Collector Logs**
```bash
make logs-otel
```

**Terminal 3: Sample App Logs**
```bash
make logs-sample-app
```

**Terminal 4: Commands**
```bash
# Run commands here
make status
make debug-requests
```

### Quick Debugging Commands

```bash
# View all logs with request IDs
make logs-mytsdb | grep "REQ:"

# View only errors
make logs-mytsdb | grep "ERROR"

# View only warnings
make logs-mytsdb | grep "WARN"

# View authentication issues
make logs-mytsdb | grep "Authentication"

# View performance (timing)
make logs-mytsdb | grep "ms"

# Follow specific request
make logs-mytsdb | grep "REQ:123"
```

## Log Format

### WriteHandler Logs

**Successful Request:**
```
[INFO] [REQ:1] Remote Write request received
[DEBUG] [REQ:1] Method: POST, Content-Length: 1234
[DEBUG] [REQ:1] Authentication successful
[DEBUG] [REQ:1] Decompressing Snappy payload (1234 bytes)
[DEBUG] [REQ:1] Decompressed to 5678 bytes
[INFO] [REQ:1] Parsed 10 time series
[DEBUG] [REQ:1] Converted to internal format
[INFO] [REQ:1] Successfully wrote 10 series, 100 samples in 45ms
```

**Failed Request (Auth):**
```
[INFO] [REQ:2] Remote Write request received
[DEBUG] [REQ:2] Method: POST, Content-Length: 1234
[WARN] [REQ:2] Authentication failed: Invalid password
[INFO] [REQ:2] Completed with auth error in 2ms
```

**Failed Request (Storage Error):**
```
[INFO] [REQ:3] Remote Write request received
[DEBUG] [REQ:3] Method: POST, Content-Length: 1234
[DEBUG] [REQ:3] Authentication successful
[INFO] [REQ:3] Parsed 5 time series
[DEBUG] [REQ:3] Converted to internal format
[ERROR] [REQ:3] Write failed for series 2: Storage full
```

## Debugging Scenarios

### Scenario 1: No Metrics Appearing in Grafana

**Steps:**

1. **Check Sample App is Generating Metrics**
   ```bash
   make logs-sample-app
   # Look for: "Generated: GET /api/users - 200"
   ```

2. **Check OTEL is Receiving**
   ```bash
   make logs-otel | grep "Metric"
   # Look for metric data
   ```

3. **Check OTEL is Sending to MyTSDB**
   ```bash
   make logs-otel | grep "successfully sent"
   # Or grep "error"
   ```

4. **Check MyTSDB is Receiving**
   ```bash
   make logs-mytsdb | grep "REQ:"
   # Should see: [REQ:X] Remote Write request received
   ```

5. **Check for Errors**
   ```bash
   make logs-mytsdb | grep "ERROR"
   ```

### Scenario 2: Authentication Failures

**Steps:**

1. **Check Auth Logs**
   ```bash
   make logs-mytsdb | grep "Authentication"
   # Look for: "Authentication failed: <reason>"
   ```

2. **Verify Credentials**
   ```bash
   # Check deployment config
   kubectl describe deployment mytsdb -n mytsdb | grep args
   # Should show: ["9090", "none"] or auth type
   ```

3. **Check OTEL Config**
   ```bash
   kubectl get configmap otel-collector-config -n mytsdb -o yaml
   # Verify auth settings match
   ```

### Scenario 3: Slow Performance

**Steps:**

1. **Check Request Timing**
   ```bash
   make logs-mytsdb | grep "ms"
   # Look for: "Successfully wrote X series, Y samples in Zms"
   ```

2. **Identify Slow Requests**
   ```bash
   make logs-mytsdb | grep "ms" | awk '{print $NF}' | sort -n
   # Shows timing distribution
   ```

3. **Check Resource Usage**
   ```bash
   kubectl top pod -n mytsdb
   # Shows CPU/memory usage
   ```

4. **Check for Errors**
   ```bash
   make logs-mytsdb | grep "ERROR"
   ```

### Scenario 4: Storage Errors

**Steps:**

1. **Check Write Errors**
   ```bash
   make logs-mytsdb | grep "Write failed"
   ```

2. **Check Disk Space**
   ```bash
   kubectl exec -it -n mytsdb $(kubectl get pod -l app=mytsdb -n mytsdb -o jsonpath='{.items[0].metadata.name}') -- df -h /data
   ```

3. **Check PVC Status**
   ```bash
   kubectl get pvc -n mytsdb
   kubectl describe pvc mytsdb-data -n mytsdb
   ```

### Scenario 5: Pod Crashes/Restarts

**Steps:**

1. **Check Pod Status**
   ```bash
   kubectl get pods -n mytsdb
   # Look for restart count
   ```

2. **Check Previous Logs**
   ```bash
   kubectl logs -n mytsdb -l app=mytsdb --previous
   # Shows logs from crashed container
   ```

3. **Check Events**
   ```bash
   kubectl get events -n mytsdb --sort-by='.lastTimestamp'
   # Shows recent events
   ```

4. **Describe Pod**
   ```bash
   kubectl describe pod -l app=mytsdb -n mytsdb
   # Shows detailed status
   ```

## Advanced Debugging

### Shell into MyTSDB Pod

```bash
make shell-mytsdb

# Inside pod:
# Check if server is running
ps aux | grep prometheus_remote_storage_server

# Check listening ports
netstat -tlnp | grep 9090

# Check data directory
ls -lah /data

# Test health endpoint
curl localhost:9090/health

# Exit
exit
```

### Manual Log Filtering

```bash
# Get logs with timestamps
kubectl logs -f -l app=mytsdb -n mytsdb --timestamps

# Get logs since 5 minutes ago
kubectl logs -l app=mytsdb -n mytsdb --since=5m

# Get last 100 lines
kubectl logs -l app=mytsdb -n mytsdb --tail=100

# Save logs to file
kubectl logs -l app=mytsdb -n mytsdb > mytsdb-logs.txt
```

### Request Correlation

Track a request across all components:

```bash
# 1. Find request in sample app
make logs-sample-app | grep "Generated"
# Note the timestamp

# 2. Find in OTEL logs
make logs-otel | grep "<timestamp>"

# 3. Find in MyTSDB logs
make logs-mytsdb | grep "REQ:" | grep "<around-timestamp>"
```

## Performance Monitoring

### Request Rate

```bash
# Count requests per minute
make logs-mytsdb | grep "Remote Write request received" | wc -l
```

### Average Latency

```bash
# Extract timing and calculate average
make logs-mytsdb | grep "Successfully wrote" | grep -oP '\d+ms' | sed 's/ms//' | awk '{sum+=$1; count++} END {print sum/count "ms average"}'
```

### Error Rate

```bash
# Count errors
make logs-mytsdb | grep "ERROR" | wc -l

# Error types
make logs-mytsdb | grep "ERROR" | cut -d':' -f4 | sort | uniq -c
```

## Troubleshooting Checklist

### Before Testing

- [ ] All pods running: `make status`
- [ ] No errors in logs: `make logs-mytsdb | grep ERROR`
- [ ] Health check passing: `make test-mytsdb-health`
- [ ] Sample app generating metrics: `make logs-sample-app`

### During Testing

- [ ] Monitor MyTSDB logs: `make logs-mytsdb`
- [ ] Watch for errors: `grep ERROR`
- [ ] Track request IDs: `grep "REQ:"`
- [ ] Monitor timing: `grep "ms"`

### After Issues

- [ ] Check all component logs
- [ ] Review error messages
- [ ] Check resource usage
- [ ] Inspect pod events
- [ ] Review configuration

## Log Levels

MyTSDB uses spdlog with these levels:

- **TRACE** - Very detailed debugging
- **DEBUG** - Detailed information
- **INFO** - General information (default)
- **WARN** - Warning messages
- **ERROR** - Error messages
- **CRITICAL** - Critical errors

### Change Log Level

```bash
# Edit deployment
kubectl edit deployment mytsdb -n mytsdb

# Add environment variable:
env:
- name: LOG_LEVEL
  value: "debug"  # or trace, info, warn, error

# Restart
make restart-mytsdb
```

## Best Practices

1. **Always use request IDs** - Track `[REQ:X]` through logs
2. **Monitor timing** - Watch for slow requests (>100ms)
3. **Check errors immediately** - Don't let them accumulate
4. **Use multiple terminals** - See all components simultaneously
5. **Save logs** - Capture logs when issues occur
6. **Correlate timestamps** - Match logs across components

## Quick Reference

```bash
# View logs
make logs-mytsdb           # All logs
make logs-mytsdb-filtered  # Filtered view
make logs-otel             # OTEL logs
make logs-sample-app       # Sample app logs

# Debug
make shell-mytsdb          # Shell access
make describe-mytsdb       # Pod details
make events                # Recent events

# Test
make test-mytsdb-health    # Health check
make test-connectivity     # Service connectivity

# Status
make status                # All pods
make quick-status          # Quick view
```

## Next Steps

To enhance debugging further:

1. Add full logging to ReadHandler (similar to WriteHandler)
2. Add `/debug/requests` endpoint for recent requests
3. Add `/debug/metrics` endpoint for statistics
4. Add Prometheus metrics export
5. Add distributed tracing (OpenTelemetry)

For now, the WriteHandler logging provides comprehensive visibility into write operations, which is the primary data path during testing.
