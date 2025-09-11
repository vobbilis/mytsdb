#!/bin/bash

# Test cmake configuration and build script
echo "=== Semantic Vector Tests - CMake & Build ==="
echo "Starting from directory: $(pwd)"
echo ""

# Navigate to project root if not already there
if [[ ! -f "CMakeLists.txt" ]]; then
    echo "Navigating to project root..."
    cd /Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb
    echo "Now in: $(pwd)"
fi

# Clean and create build directory
echo "Cleaning and setting up build directory..."
rm -rf build_test
mkdir -p build_test
cd build_test

echo ""
echo "=== Running CMake Configuration ==="
cmake -DBUILD_TESTS=ON -DTSDB_SEMVEC=ON .. 2>&1 | tee cmake_test_output.log

CMAKE_EXIT_CODE=$?
echo ""
echo "=== CMake Configuration Results ==="
if [ $CMAKE_EXIT_CODE -eq 0 ]; then
    echo "✅ CMAKE CONFIGURATION SUCCESS"
    echo ""
    echo "Available semantic vector targets:"
    make help | grep "tsdb_semvec" | head -10
    echo ""
    echo "=== Attempting Build ==="
    make tsdb_semvec_architecture_validation_tests -j2 2>&1 | tee build_test_output.log
    BUILD_EXIT_CODE=$?
    if [ $BUILD_EXIT_CODE -eq 0 ]; then
        echo "✅ BUILD SUCCESS - Running test..."
        ./tsdb_semvec_architecture_validation_tests --gtest_brief=1
    else
        echo "❌ BUILD FAILED"
        echo "Last 20 lines of build error:"
        tail -20 build_test_output.log
    fi
else
    echo "❌ CMAKE CONFIGURATION FAILED"
    echo ""
    echo "Last 20 lines of cmake error:"
    tail -20 cmake_test_output.log
fi
