#!/bin/bash
# Script to run previously failing tests individually to verify they pass
# These tests fail during parallel execution but pass when run individually

set -e

BUILD_DIR="${1:-build}"
cd "$(dirname "$0")/.."

echo "=========================================="
echo "Running Previously Failing Tests"
echo "=========================================="
echo ""

FAILED=0
PASSED=0

run_test() {
    local test_name="$1"
    echo -n "Running $test_name... "
    if ctest --test-dir "$BUILD_DIR" -R "^${test_name}$" --output-on-failure -j1 > /tmp/test_output_$$.log 2>&1; then
        echo "PASSED"
        ((PASSED++))
    else
        echo "FAILED"
        cat /tmp/test_output_$$.log
        ((FAILED++))
    fi
    rm -f /tmp/test_output_$$.log
}

# Benchmark tests that were SEGFAULTing
echo "=== Benchmark Tests ==="
run_test "benchmark_read"
run_test "benchmark_e2e_query"

# HTTP Server test
echo ""
echo "=== HTTP Server Tests ==="
run_test "HttpServerTest.CustomHandler"
run_test "HttpServerTest.HandlerError"
run_test "HttpServerTest.ServerTimeout"

# Storage tests
echo ""
echo "=== Storage Tests ==="
run_test "StorageTest.LabelOperations"
run_test "StorageTest.ConcurrentOperations"
run_test "StorageTest.CompactionAndFlush"
run_test "StorageTest.MultipleSeries"
run_test "StorageTest.TimestampBoundaries"

# WAL tests
echo ""
echo "=== WAL Tests ==="
run_test "AsyncWALShardTest.BasicLogAndReplay"
run_test "AsyncWALShardTest.ConcurrentWrites"

# Integration tests
echo ""
echo "=== Integration Tests ==="
run_test "SecondaryIndexIntegrationTest.SecondaryIndexIsActuallyConsulted"
run_test "ConfigIntegrationTest.HistogramConfigIntegration"
run_test "ConfigIntegrationTest.GranularityConfiguration"

# Background processor
echo ""
echo "=== Background Processor Tests ==="
run_test "BackgroundProcessorTest.StressTest"

# PromQL tests
echo ""
echo "=== PromQL Tests ==="
run_test "ComprehensivePromQLTest.RateFunction"
run_test "ComprehensivePromQLTest.SelectorRegexMatch"
run_test "ComprehensivePromQLTest.AggregationSum"

echo ""
echo "=========================================="
echo "Summary: $PASSED passed, $FAILED failed"
echo "=========================================="

if [ $FAILED -gt 0 ]; then
    exit 1
fi
