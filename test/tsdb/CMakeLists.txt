# CMakeLists.txt for test/tsdb - Orphaned tsdb tests
# This directory contains nested test files that need integration

add_subdirectory(storage)
add_subdirectory(histogram)
add_subdirectory(otel)
# Note: benchmark subdirectory contains standalone benchmark, not gtest
