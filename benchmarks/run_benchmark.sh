#!/bin/bash

# Get absolute path to script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BENCH_BIN="$SCRIPT_DIR/../build/tools/k8s_combined_benchmark"
LOG_FILE="server.log"

if ! pgrep -f "tsdb_server" > /dev/null; then
    echo "Error: TSDB Server is not running. Please run ./start_server.sh first."
    exit 1
fi

echo "Server detected. Running benchmark..."
echo "Command: $BENCH_BIN $@"

$BENCH_BIN $@ 2>&1 | tee benchmark_results.txt

EXIT_CODE=${PIPESTATUS[0]}
echo "Benchmark completed (Exit Code: $EXIT_CODE)"
exit $EXIT_CODE
