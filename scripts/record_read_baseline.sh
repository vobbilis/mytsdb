#!/bin/bash
# =============================================================================
# record_read_baseline.sh
# =============================================================================
# Records baseline read performance metrics for comparison after optimization.
# Run this BEFORE making changes to establish ground truth.
#
# Usage:
#   ./scripts/record_read_baseline.sh [tag]
#
# Example:
#   ./scripts/record_read_baseline.sh baseline
#   ./scripts/record_read_baseline.sh after-phase1
# =============================================================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
RESULTS_DIR="$PROJECT_ROOT/benchmark_results"

TAG="${1:-baseline}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
GIT_SHA=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")

echo -e "${GREEN}=== Read Performance Baseline Recording ===${NC}"
echo "Date: $(date)"
echo "Git SHA: $GIT_SHA"
echo "Tag: $TAG"
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR"

# Check if we have a build
BUILD_DIR="$PROJECT_ROOT/build"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}No build directory found. Building...${NC}"
    cmake -DCMAKE_BUILD_TYPE=Release -B "$BUILD_DIR" "$PROJECT_ROOT"
fi

echo -e "${GREEN}Building with Release optimizations...${NC}"
cmake --build "$BUILD_DIR" -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)

# Output file for this run
OUTPUT_FILE="$RESULTS_DIR/${TAG}_${TIMESTAMP}.log"

echo -e "${GREEN}Recording results to: $OUTPUT_FILE${NC}"
echo ""

# Header
cat > "$OUTPUT_FILE" << EOF
=============================================================================
READ PERFORMANCE BASELINE
=============================================================================
Date: $(date)
Git SHA: $GIT_SHA
Tag: $TAG
Host: $(hostname)
OS: $(uname -s) $(uname -r)
CPU: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || cat /proc/cpuinfo | grep "model name" | head -1 | cut -d: -f2)
=============================================================================

EOF

# -----------------------------------------------------------------------------
# Test 1: Micro-benchmarks (if available)
# -----------------------------------------------------------------------------
MICRO_BENCH="$BUILD_DIR/test/read_performance_benchmark"
if [ -x "$MICRO_BENCH" ]; then
    echo -e "${GREEN}Running micro-benchmarks...${NC}"
    echo "" >> "$OUTPUT_FILE"
    echo "=== MICRO-BENCHMARKS ===" >> "$OUTPUT_FILE"
    "$MICRO_BENCH" --benchmark_format=console 2>&1 | tee -a "$OUTPUT_FILE"
else
    echo -e "${YELLOW}Micro-benchmark not found, skipping...${NC}"
fi

# -----------------------------------------------------------------------------
# Test 2: E2E Query Benchmarks
# -----------------------------------------------------------------------------
E2E_BENCH="$BUILD_DIR/test/e2e_query_benchmark"
if [ -x "$E2E_BENCH" ]; then
    echo -e "${GREEN}Running E2E query benchmarks...${NC}"
    echo "" >> "$OUTPUT_FILE"
    echo "=== E2E QUERY BENCHMARKS ===" >> "$OUTPUT_FILE"
    "$E2E_BENCH" --gtest_filter="*" 2>&1 | tee -a "$OUTPUT_FILE"
else
    echo -e "${YELLOW}E2E benchmark not found, skipping...${NC}"
fi

# -----------------------------------------------------------------------------
# Test 3: K8s Combined Benchmark
# -----------------------------------------------------------------------------
K8S_BENCH="$BUILD_DIR/tools/k8s_combined_benchmark"
if [ -x "$K8S_BENCH" ]; then
    echo -e "${GREEN}Running K8s combined benchmark (quick preset)...${NC}"
    echo "" >> "$OUTPUT_FILE"
    echo "=== K8S COMBINED BENCHMARK (quick) ===" >> "$OUTPUT_FILE"
    
    # Start server in background
    SERVER_BIN="$BUILD_DIR/src/tsdb/tsdb_server"
    if [ -x "$SERVER_BIN" ]; then
        # Clean up any existing data
        rm -rf /tmp/tsdb_benchmark_data
        mkdir -p /tmp/tsdb_benchmark_data
        
        echo "Starting TSDB server..."
        "$SERVER_BIN" --data-dir=/tmp/tsdb_benchmark_data --port=9090 &
        SERVER_PID=$!
        sleep 5
        
        # Run benchmark
        "$K8S_BENCH" --preset=quick --http-address=localhost:9090 2>&1 | tee -a "$OUTPUT_FILE"
        
        # Stop server
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    else
        echo "Server binary not found, running benchmark only..."
        "$K8S_BENCH" --preset=quick 2>&1 | tee -a "$OUTPUT_FILE"
    fi
else
    echo -e "${YELLOW}K8s benchmark not found, skipping...${NC}"
fi

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
echo "" >> "$OUTPUT_FILE"
echo "==============================================================================" >> "$OUTPUT_FILE"
echo "SUMMARY" >> "$OUTPUT_FILE"
echo "==============================================================================" >> "$OUTPUT_FILE"

# Extract key metrics from the log
echo "" >> "$OUTPUT_FILE"
echo "Key Metrics:" >> "$OUTPUT_FILE"

# Try to extract p50/p99 from output
grep -i "p50\|p99\|throughput\|qps" "$OUTPUT_FILE" | head -20 >> "$OUTPUT_FILE" 2>/dev/null || true

echo "" >> "$OUTPUT_FILE"
echo "==============================================================================" >> "$OUTPUT_FILE"

echo ""
echo -e "${GREEN}=== Baseline Recording Complete ===${NC}"
echo "Results saved to: $OUTPUT_FILE"
echo ""

# Create/update symlink to latest
ln -sf "$(basename "$OUTPUT_FILE")" "$RESULTS_DIR/latest_${TAG}.log"

echo "To compare with a previous run:"
echo "  diff $RESULTS_DIR/latest_baseline.log $RESULTS_DIR/latest_after-phase1.log"
