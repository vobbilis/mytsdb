#!/bin/bash

# Test runner with timeout mechanism
# Usage: ./run_tests_with_timeout.sh <test_executable> <timeout_seconds> [test_filter]
# Usage: ./run_tests_with_timeout.sh <test_executable> <timeout_seconds> [test_filter] [additional_args]

if [ $# -lt 2 ]; then
    echo "Usage: $0 <test_executable> <timeout_seconds> [test_filter] [additional_args]"
    echo "Example: $0 ./test/unit/tsdb_background_processor_unit_tests 30"
    echo "Example: $0 ./test/unit/tsdb_background_processor_unit_tests 30 'BackgroundProcessorTest.BasicInitialization'"
    echo "Example: $0 ./test/integration/tsdb_integration_test_suite 60 'StorageTest.*' '--gtest_also_run_disabled_tests'"
    exit 1
fi

TEST_EXECUTABLE="$1"
TIMEOUT_SECONDS="$2"
TEST_FILTER="$3"
ADDITIONAL_ARGS="$4"

echo "Running test: $TEST_EXECUTABLE with timeout: ${TIMEOUT_SECONDS}s"
if [ -n "$TEST_FILTER" ]; then
    echo "Test filter: $TEST_FILTER"
fi
if [ -n "$ADDITIONAL_ARGS" ]; then
    echo "Additional args: $ADDITIONAL_ARGS"
fi
echo "=========================================="

# Build the command
CMD="$TEST_EXECUTABLE"
if [ -n "$TEST_FILTER" ]; then
    CMD="$CMD --gtest_filter=$TEST_FILTER"
fi
if [ -n "$ADDITIONAL_ARGS" ]; then
    CMD="$CMD $ADDITIONAL_ARGS"
fi

# Run the test with timeout
timeout "${TIMEOUT_SECONDS}s" $CMD
EXIT_CODE=$?

if [ $EXIT_CODE -eq 124 ]; then
    echo ""
    echo "❌ TEST TIMED OUT after ${TIMEOUT_SECONDS} seconds!"
    echo "   This test is hanging and needs to be fixed."
    echo "   Test executable: $TEST_EXECUTABLE"
    exit 124
elif [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "✅ TEST PASSED successfully"
    exit 0
else
    echo ""
    echo "❌ TEST FAILED with exit code: $EXIT_CODE"
    echo "   Test executable: $TEST_EXECUTABLE"
    exit $EXIT_CODE
fi
