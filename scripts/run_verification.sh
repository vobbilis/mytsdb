#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

CLIENT_LOG="benchmarks/run.log"
SERVER_LOG="benchmarks/server.log"

# Cleanup
pkill -x tsdb_server || true
rm -rf benchmarks_data
mkdir benchmarks_data
> "$CLIENT_LOG"
> "$SERVER_LOG"

echo "Starting Server (Port 8820)..."
./build/src/tsdb/tsdb_server --enable-write-instrumentation --log-level error --data-dir benchmarks_data --arrow-port 8820 > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!

echo "Waiting for port 8820..."
for i in {1..30}; do
    if lsof -i :8820 > /dev/null; then
        echo "Port 8820 is open."
        break
    fi
    sleep 1
done

echo "Running Benchmark (Client)..."
# Capture combined output
./build/tools/k8s_combined_benchmark --preset quick --clean-start --arrow-port 8820 --duration 5 > "$CLIENT_LOG" 2>&1

echo "Benchmark Complete."
echo "Stoping Server..."
kill -INT $SERVER_PID
wait $SERVER_PID || true

echo "---------------------------------------------------"
echo "FULL BENCHMARK REPORT:"
cat "$CLIENT_LOG"
echo "---------------------------------------------------"
