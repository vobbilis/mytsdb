#!/bin/bash
set -e

# Build tests
echo "Building tests..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DTSDB_SEMVEC=OFF -DENABLE_PARQUET=ON
make -j12 tsdb_integration_test_suite

# Run the persistence test
echo "Running persistence test..."
./test/integration/tsdb_integration_test_suite --gtest_filter="StoragePersistenceTest.*"
