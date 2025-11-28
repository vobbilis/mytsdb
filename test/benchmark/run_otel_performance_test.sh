#!/bin/bash

# OTEL/gRPC Performance Test Script
# This script starts the TSDB server and runs OTEL/gRPC write performance benchmarks

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
PORT=${OTEL_PORT:-4317}
DATA_DIR=${OTEL_SERVER_DATA_DIR:-$(mktemp -d /tmp/tsdb_otel_test_XXXXXX)}
RESULTS_DIR=${OTEL_RESULTS_DIR:-test_results/baseline}
BUILD_DIR=${BUILD_DIR:-build}
LOG_FILE="$RESULTS_DIR/otel_performance_test.log"

# Create results directory
mkdir -p "$RESULTS_DIR"

# Cleanup function
cleanup() {
    if [ -n "${SERVER_PID:-}" ]; then
        echo -e "${YELLOW}Cleaning up...${NC}"
        echo "Stopping TSDB server (PID: $SERVER_PID)..."
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        echo -e "${GREEN}Cleanup complete${NC}"
    fi
}

trap cleanup EXIT

echo "=========================================="
echo "OTEL/gRPC Performance Test"
echo "=========================================="
echo "Server Address: 0.0.0.0:$PORT"
echo "Data Directory: $DATA_DIR"
echo "Results Directory: $RESULTS_DIR"
echo ""

# Start TSDB server
echo "Starting TSDB server on 0.0.0.0:$PORT..."
# Try different possible locations for tsdb_server
SERVER_BINARY=""
for path in "$BUILD_DIR/src/tsdb/tsdb_server" "$BUILD_DIR/tsdb_server" "build/src/tsdb/tsdb_server" "build/tsdb_server"; do
    if [ -f "$path" ]; then
        SERVER_BINARY="$path"
        break
    fi
done

if [ -z "$SERVER_BINARY" ]; then
    echo -e "${RED}ERROR: tsdb_server binary not found!${NC}"
    exit 1
fi

echo "Using server binary: $SERVER_BINARY"
"$SERVER_BINARY" \
    --data-dir "$DATA_DIR" \
    --address "0.0.0.0:$PORT" \
    > "$LOG_FILE" 2>&1 &
SERVER_PID=$!

# Wait for server to be ready
echo "Waiting for server to be ready..."
MAX_WAIT=30
WAITED=0
while [ $WAITED -lt $MAX_WAIT ]; do
    if nc -z localhost $PORT 2>/dev/null; then
        echo -e "${GREEN}Server is ready!${NC}"
        break
    fi
    sleep 1
    WAITED=$((WAITED + 1))
done

if [ $WAITED -ge $MAX_WAIT ]; then
    echo -e "${RED}ERROR: Server failed to start within $MAX_WAIT seconds${NC}"
    echo "Server logs:"
    tail -50 "$LOG_FILE"
    exit 1
fi

# Export environment variables for benchmark
export OTEL_SERVER_ADDRESS="localhost:$PORT"

echo ""
echo "Running OTEL/gRPC write performance benchmarks with read verification..."
echo ""

# Run benchmarks - redirect both stdout and stderr to log file AND terminal
"$BUILD_DIR/test/benchmark/otel_write_perf_bench" \
    --benchmark_out_format=json \
    --benchmark_out="$RESULTS_DIR/otel_write_performance.json" \
    --benchmark_min_time=1s \
    2>&1 | tee -a "$LOG_FILE"

BENCHMARK_EXIT_CODE=${PIPESTATUS[0]}

echo ""
echo -e "${GREEN}Benchmarks completed!${NC}"
echo "Results saved to: $RESULTS_DIR/otel_write_performance.json"
echo "Log saved to: $LOG_FILE"
echo ""

# Wait a bit for verification to complete
echo "Waiting for verification to complete..."
sleep 3

# Check verification results in log
if grep -q "VERIFICATION SUCCESS" "$LOG_FILE"; then
    echo -e "${GREEN}✅ Verification successful!${NC}"
    grep -A 5 "VERIFICATION SUCCESS" "$LOG_FILE" | head -10
elif grep -q "VERIFICATION" "$LOG_FILE"; then
    echo -e "${YELLOW}⚠️  Verification ran but may have issues${NC}"
    grep -A 10 "VERIFICATION" "$LOG_FILE" | tail -20
else
    echo -e "${RED}❌ No verification output found${NC}"
    echo "Last 50 lines of log:"
    tail -50 "$LOG_FILE"
fi

echo ""
echo "To compare with native writes, run:"
echo "  $BUILD_DIR/test/benchmark/write_perf_bench --benchmark_out_format=json --benchmark_out=$RESULTS_DIR/write_performance.json"

exit $BENCHMARK_EXIT_CODE
