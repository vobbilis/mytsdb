#!/bin/bash
set -e

# Baseline Benchmark Runner
# This script builds and runs all performance benchmarks to establish a baseline.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
RESULTS_DIR="$PROJECT_ROOT/test_results/baseline"

mkdir -p "$RESULTS_DIR"
LOG_FILE="$RESULTS_DIR/baseline_run_$(date +%Y%m%d_%H%M%S).log"

echo "==========================================" | tee -a "$LOG_FILE"
echo "Starting Baseline Benchmark Run" | tee -a "$LOG_FILE"
echo "Date: $(date)" | tee -a "$LOG_FILE"
echo "Log: $LOG_FILE" | tee -a "$LOG_FILE"
echo "==========================================" | tee -a "$LOG_FILE"

# 1. Build Benchmarks
echo "" | tee -a "$LOG_FILE"
echo "Building benchmarks..." | tee -a "$LOG_FILE"
cd "$BUILD_DIR"
make write_perf_bench read_perf_bench mixed_perf_bench latency_perf_bench concurrency_perf_bench >> "$LOG_FILE" 2>&1

if [ $? -ne 0 ]; then
    echo "❌ Build failed! Check log for details." | tee -a "$LOG_FILE"
    exit 1
fi
echo "✅ Build successful." | tee -a "$LOG_FILE"

# Function to run a benchmark
run_benchmark() {
    local name=$1
    local executable=$2
    local args=$3
    
    echo "" | tee -a "$LOG_FILE"
    echo "------------------------------------------" | tee -a "$LOG_FILE"
    echo "Running $name..." | tee -a "$LOG_FILE"
    echo "------------------------------------------" | tee -a "$LOG_FILE"
    
    if [ -f "$BUILD_DIR/test/benchmark/$executable" ]; then
        "$BUILD_DIR/test/benchmark/$executable" $args 2>&1 | tee -a "$LOG_FILE"
    else
        echo "❌ Executable $executable not found!" | tee -a "$LOG_FILE"
    fi
}

# 2. Run Benchmarks
run_benchmark "Write Throughput (Single Threaded)" "write_perf_bench" "--benchmark_format=console"
run_benchmark "Read Throughput (Single Threaded)" "read_perf_bench" "--benchmark_format=console"
run_benchmark "Mixed Workload (50/50)" "mixed_perf_bench" "--benchmark_format=console"
run_benchmark "Write Latency" "latency_perf_bench" "--benchmark_format=console"
run_benchmark "Concurrency Scaling" "concurrency_perf_bench" "--benchmark_format=console"

echo "" | tee -a "$LOG_FILE"
echo "==========================================" | tee -a "$LOG_FILE"
echo "Baseline Run Completed" | tee -a "$LOG_FILE"
echo "Results saved to: $LOG_FILE" | tee -a "$LOG_FILE"
echo "==========================================" | tee -a "$LOG_FILE"

