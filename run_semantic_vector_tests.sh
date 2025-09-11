#!/bin/bash

# Semantic Vector Test Runner
# This script builds and runs all semantic vector tests

set -e  # Exit on any error

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Semantic Vector Test Runner ===${NC}"
echo "Starting from directory: $(pwd)"

# Navigate to project root if not already there
if [[ ! -f "CMakeLists.txt" ]]; then
    echo "Navigating to project root..."
    cd /Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb
    echo "Now in: $(pwd)"
fi

# Create build directory
BUILD_DIR="build_test"
echo -e "${YELLOW}Creating build directory: ${BUILD_DIR}${NC}"
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake -DBUILD_TESTS=ON -DTSDB_SEMVEC=ON .. > cmake_output.log 2>&1
CMAKE_RESULT=$?

if [ $CMAKE_RESULT -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    echo "Last 20 lines of output:"
    tail -20 cmake_output.log
    exit 1
fi

echo -e "${GREEN}CMake configuration successful!${NC}"

# Build the tests
echo -e "${YELLOW}Building tests...${NC}"
make -j4 > build_output.log 2>&1
BUILD_RESULT=$?

if [ $BUILD_RESULT -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    echo "Last 20 lines of output:"
    tail -20 build_output.log
    exit 1
fi

echo -e "${GREEN}Build successful!${NC}"

# List all semantic vector test executables
echo -e "${YELLOW}Available semantic vector test executables:${NC}"
find . -name "tsdb_semvec_*" -type f -executable | sort

# Run each test individually with detailed output
echo -e "\n${BLUE}=== Running Semantic Vector Tests ===${NC}"

# Function to run a test with proper output
run_test() {
    local test_path=$1
    local test_name=$(basename $test_path)
    
    echo -e "\n${YELLOW}Running test: ${test_name}${NC}"
    echo "----------------------------------------"
    
    # Run the test with verbose output
    $test_path --gtest_color=yes
    local result=$?
    
    if [ $result -eq 0 ]; then
        echo -e "${GREEN}✓ ${test_name} passed!${NC}"
    else
        echo -e "${RED}✗ ${test_name} failed!${NC}"
        FAILED_TESTS="${FAILED_TESTS} ${test_name}"
    fi
    
    return $result
}

# Initialize test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=""

# Run all semantic vector tests
for test in $(find . -name "tsdb_semvec_*" -type f -executable | sort); do
    TOTAL_TESTS=$((TOTAL_TESTS+1))
    run_test $test
    if [ $? -eq 0 ]; then
        PASSED_TESTS=$((PASSED_TESTS+1))
    fi
done

# Print summary
echo -e "\n${BLUE}=== Test Summary ===${NC}"
echo -e "Total tests: ${TOTAL_TESTS}"
echo -e "Passed: ${GREEN}${PASSED_TESTS}${NC}"
FAILED_COUNT=$((TOTAL_TESTS-PASSED_TESTS))
if [ $FAILED_COUNT -eq 0 ]; then
    echo -e "Failed: ${GREEN}0${NC}"
    echo -e "\n${GREEN}All tests passed successfully!${NC}"
    exit 0
else
    echo -e "Failed: ${RED}${FAILED_COUNT}${NC}"
    echo -e "\nFailed tests: ${RED}${FAILED_TESTS}${NC}"
    exit 1
fi