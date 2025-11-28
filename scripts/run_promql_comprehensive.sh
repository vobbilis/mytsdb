#!/bin/bash
# Script to run comprehensive PromQL tests with data generation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DATA_DIR="$PROJECT_ROOT/data/comprehensive_test_data"

echo "=========================================="
echo "PromQL Comprehensive Test Runner"
echo "=========================================="
echo ""

# 1. Build required components
echo "Step 1: Building components..."
cd "$PROJECT_ROOT"
make build

# 2. Prepare data directory
echo "Step 2: Preparing data directory..."
if [ -d "$DATA_DIR" ]; then
    echo "Cleaning existing data directory..."
    rm -rf "$DATA_DIR"
fi
mkdir -p "$DATA_DIR"

# 3. Start TSDB Server
echo "Step 3: Starting TSDB Server..."
SERVER_BIN="$BUILD_DIR/src/tsdb/tsdb_server"
if [ ! -f "$SERVER_BIN" ]; then
    echo "Error: TSDB server binary not found at $SERVER_BIN"
    exit 1
fi

"$SERVER_BIN" --address 127.0.0.1:9090 --data-dir "$DATA_DIR" > "$PROJECT_ROOT/tsdb_server_test.log" 2>&1 &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"

# Wait for server to be ready (simple sleep for now)
sleep 2

# 4. Generate Data
echo "Step 4: Generating test data..."
GENERATOR_BIN="$BUILD_DIR/tools/data_gen/synthetic_cluster_generator"
if [ ! -f "$GENERATOR_BIN" ]; then
    echo "Error: Generator binary not found at $GENERATOR_BIN"
    kill $SERVER_PID
    exit 1
fi

# Generate a larger dataset for proper testing and performance benchmarking
# 50 nodes * 20 pods = 1000 series per metric
"$GENERATOR_BIN" --address 127.0.0.1:9090 --nodes 50 --pods 20 --hours 2

echo "Data generation complete."

# 5. Stop TSDB Server
echo "Step 5: Stopping TSDB Server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true
echo "Server stopped."

# 6. Run Test Harness
echo "Step 6: Running PromQL Test Harness..."
HARNESS_BIN="$BUILD_DIR/test/comprehensive_promql/comprehensive_promql_test_harness"
if [ ! -f "$HARNESS_BIN" ]; then
    echo "Error: Test harness binary not found at $HARNESS_BIN"
    exit 1
fi

"$HARNESS_BIN"

echo ""
echo "=========================================="
echo "PromQL Comprehensive Tests Completed"
echo "=========================================="
