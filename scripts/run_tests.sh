#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure and build
echo -e "${GREEN}Configuring project...${NC}"
cmake ..
if [ $? -ne 0 ]; then
    echo -e "${RED}Configuration failed${NC}"
    exit 1
fi

echo -e "${GREEN}Building project...${NC}"
cmake --build .
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# Create test results directory
mkdir -p test_results/logs

# Run tests
echo -e "${GREEN}Running tests...${NC}"
ctest --output-on-failure --verbose 2>&1 | tee test_results/logs/test_run.log

# Check if any tests failed
if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo -e "${RED}Some tests failed. Check test_results/failures.txt for details${NC}"
    exit 1
fi

# Display test summary
echo -e "${GREEN}Test Summary:${NC}"
cat test_results/summary.txt

# Check for failures
if [ -s test_results/failures.txt ]; then
    echo -e "${RED}Test Failures:${NC}"
    cat test_results/failures.txt
fi

# Generate coverage report if gcov is available
if command -v gcovr &> /dev/null; then
    echo -e "${GREEN}Generating coverage report...${NC}"
    gcovr -r .. . --html --html-details -o test_results/coverage.html
    echo "Coverage report generated at test_results/coverage.html"
fi

echo -e "${GREEN}Test results and logs are available in build/test_results/${NC}" 