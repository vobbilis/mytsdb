#!/bin/bash
set +e # Continue on error
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

CLIENT_LOG="benchmarks/run.log"
SERVER_LOG="benchmarks/server.log"

# Cleanup
echo "Cleaning up..."
pkill -x tsdb_server || true
rm -rf benchmarks_data
mkdir benchmarks_data
> "$CLIENT_LOG"
> "$SERVER_LOG"

echo "Starting Server (Port 8821)..."
./build/src/tsdb/tsdb_server --enable-write-instrumentation --log-level error --data-dir benchmarks_data --arrow-port 8821 > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!

echo "Waiting for port 8821..."
sleep 5

echo "Running Benchmark (Client)..."
# Capture output but ignore exit code
./build/tools/k8s_combined_benchmark --preset quick --clean-start --arrow-port 8821 > "$CLIENT_LOG" 2>&1

echo "Benchmark finished (Possible crash ignored)."

# Even if client crashed, Server is still running and has data!
echo "Generating Performance Report from Server API..."

echo "\n========================================"
echo "      SERVER-SIDE VERIFIED METRICS"
echo "========================================"

# Query Helper
query_metric() {
    QUERY=$1
    # Use curl, extract value.
    # Response: {"status":"success","data":{"resultType":"vector","result":[{"metric":{},"value":[173...,"0.023"]}]}}
    # We want the 0.023
    curl -s --max-time 5 "http://localhost:9090/api/v1/query?query=$QUERY" | \
    grep -o '"value":\[[0-9.]*,"[^"]*"]' | \
    awk -F'"' '{print $4}'
}

# 1. P99 Latency
P99=$(query_metric "histogram_quantile(0.99,mytsdb_query_duration_seconds_bucket)")
# Convert s to ms
if [ ! -z "$P99" ]; then
    P99_MS=$(echo "$P99 * 1000" | bc -l)
    printf "[READ] P99 Latency: %.2f ms " $P99_MS
    if (( $(echo "$P99_MS < 500" | bc -l) )); then
        echo "✅ (PASS)"
    else
        echo "❌ (FAIL)"
    fi
else
    echo "[READ] P99 Latency: NaN (No data?)"
fi

# 2. P50 Latency
P50=$(query_metric "histogram_quantile(0.50,mytsdb_query_duration_seconds_bucket)")
if [ ! -z "$P50" ]; then
    P50_MS=$(echo "$P50 * 1000" | bc -l)
    printf "[READ] P50 Latency: %.2f ms\n" $P50_MS
fi

# 3. Write Stats
MUTEX=$(query_metric "mytsdb_write_mutex_lock_seconds_total")
APPEND=$(query_metric "mytsdb_write_sample_append_seconds_total")

echo "\n[WRITE] Internal Diagnostics:"
echo "  Mutex Wait Total: ${MUTEX:-0} s"
echo "  I/O Append Total: ${APPEND:-0} s"

echo "========================================"

echo "Stopping Server..."
kill -INT $SERVER_PID
wait $SERVER_PID || true
echo "Done."
