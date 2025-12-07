#!/bin/bash
echo "Stopping any existing server..."
pkill -f tsdb_server
sleep 5

echo "Starting server..."
../build/src/tsdb/tsdb_server > tsdb_bench_run.log 2>&1 &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"

echo "Waiting for server validation..."
timeout=600
elapsed=0
while ! grep -q "Arrow Flight server listening" tsdb_bench_run.log; do
    sleep 5
    elapsed=$((elapsed+5))
    if [ $elapsed -ge $timeout ]; then
        echo "Timeout waiting for server startup (WAL replay might be too slow)"
        kill $SERVER_PID
        exit 1
    fi
    echo "Waiting... ${elapsed}s (WAL replay in progress)"
done

echo "Server is ready! Waiting 10s for socket stability..."
sleep 10
echo "Running benchmark..."
# Use --preset medium explicitly compatible with my code change
../build/tools/k8s_combined_benchmark --preset medium --write-workers 0 --read-workers 8 --duration 60 > benchmark_results.txt 2>&1
EXIT_CODE=$?

echo "Benchmark finished with exit code $EXIT_CODE"

echo "Stopping server..."
kill $SERVER_PID
exit $EXIT_CODE
