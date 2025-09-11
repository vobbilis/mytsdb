#!/bin/bash

# Debug script to verify build setup

echo "Current directory: $(pwd)"
echo "Checking for CMakeLists.txt: $(ls -la CMakeLists.txt 2>/dev/null || echo 'Not found')"

echo -e "\nCreating fresh build directory..."
rm -rf debug_build
mkdir -p debug_build
cd debug_build
echo "Build directory: $(pwd)"

echo -e "\nRunning CMake..."
cmake -DBUILD_TESTS=ON -DTSDB_SEMVEC=ON .. 2>&1 | tee cmake_output.log

echo -e "\nChecking build directory contents:"
ls -la

echo -e "\nDone."
