#!/bin/bash
# Full Test Suite Runner with Clean Build
# This script performs a clean build and runs the full test suite in the background
# It uses caffeinate to prevent laptop hibernation during the test run

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "TSDB Full Test Suite Runner"
echo "=========================================="
echo ""

# Create test_results directory if it doesn't exist
mkdir -p test_results

# Clean up any old PID files
if [ -f "test_results/full_test_suite.pid" ]; then
    OLD_PID=$(cat test_results/full_test_suite.pid 2>/dev/null || echo "")
    if [ -n "$OLD_PID" ] && ps -p "$OLD_PID" > /dev/null 2>&1; then
        echo "⚠️  Found existing test run (PID: $OLD_PID)"
        echo "   Stopping it first..."
        kill "$OLD_PID" 2>/dev/null || true
        sleep 2
    fi
fi

# Generate timestamp for log file
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="test_results/full_test_suite_${TIMESTAMP}.log"
PID_FILE="test_results/full_test_suite.pid"

echo "Step 1: Performing clean build..."
echo "-----------------------------------"
echo "This will remove the build directory and rebuild everything from scratch."
echo ""

# Perform clean build
echo "Cleaning previous build..."
make clean-all

echo ""
echo "Building project..."
make build

echo ""
echo "Step 2: Starting full test suite in background..."
echo "-----------------------------------"
echo "Log file: $LOG_FILE"
echo "PID file: $PID_FILE"
echo ""

# Run tests in background with caffeinate to prevent hibernation
# caffeinate -d prevents display sleep
# caffeinate -i prevents idle sleep
# caffeinate -m prevents disk sleep
# We use -d -i -m to prevent all types of sleep

# Create log file first (ensure directory exists)
mkdir -p test_results
echo "=== Full Test Suite Run Started at $(date) ===" > "$LOG_FILE"
echo "Build completed, starting tests..." >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

# Start the background process and capture PID
(
    # Use caffeinate to prevent laptop hibernation
    # -d: prevent display sleep
    # -i: prevent idle sleep  
    # -m: prevent disk sleep
    # Run caffeinate with the command directly
    caffeinate -d -i -m bash -c "
        echo 'Running full test suite with caffeinate (prevents hibernation)...' >> '$LOG_FILE'
        echo 'Test start time: \$(date)' >> '$LOG_FILE'
        echo '' >> '$LOG_FILE'
        
        # Run the full test suite
        make test-all >> '$LOG_FILE' 2>&1
        TEST_EXIT_CODE=\$?
        
        echo '' >> '$LOG_FILE'
        echo 'Test end time: \$(date)' >> '$LOG_FILE'
        echo \"Test exit code: \$TEST_EXIT_CODE\" >> '$LOG_FILE'
        
        if [ \$TEST_EXIT_CODE -eq 0 ]; then
            echo '=== ALL TESTS PASSED ===' >> '$LOG_FILE'
        else
            echo '=== SOME TESTS FAILED ===' >> '$LOG_FILE'
        fi
        
        exit \$TEST_EXIT_CODE
    "
) &
TEST_PID=$!

# Save PID to file immediately
echo $TEST_PID > "$PID_FILE"

echo "✅ Test suite started in background (PID: $TEST_PID)"
echo ""
echo "Step 3: Monitoring test status..."
echo "-----------------------------------"
sleep 3

# Check if process is still running
if ps -p $TEST_PID > /dev/null 2>&1; then
    echo "✅ Test process is running"
    echo ""
    echo "Last 10 lines of log:"
    echo "---"
    tail -10 "$LOG_FILE" 2>/dev/null || echo "(Log file not yet created or empty)"
    echo "---"
else
    echo "⚠️  Test process exited immediately - check log file for errors"
    if [ -f "$LOG_FILE" ]; then
        echo ""
        echo "Log file contents:"
        cat "$LOG_FILE"
    fi
    exit 1
fi

echo ""
echo "=========================================="
echo "Full test suite is now running!"
echo "=========================================="
echo ""
echo "Test Information:"
echo "  PID: $TEST_PID"
echo "  Log file: $LOG_FILE"
echo "  PID file: $PID_FILE"
echo ""
echo "Useful commands:"
echo "  tail -f $LOG_FILE              - Monitor test progress in real-time"
echo "  ps -p $TEST_PID                - Check if tests are still running"
echo "  kill $TEST_PID                 - Stop the test run (if needed)"
echo "  cat $PID_FILE                  - View the PID"
echo ""
echo "The test will continue running even if you:"
echo "  - Close this terminal"
echo "  - Put your laptop to sleep (caffeinate prevents hibernation)"
echo "  - Lock your screen"
echo ""
echo "To check test status later, run:"
echo "  ps -p \$(cat $PID_FILE) && echo 'Tests still running' || echo 'Tests completed'"
echo ""
echo "To view the final results:"
echo "  tail -100 $LOG_FILE"
echo ""

