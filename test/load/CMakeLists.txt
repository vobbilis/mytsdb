# Load Testing Configuration

# Check for Google Benchmark


# Scale Test (Benchmark)
add_executable(tsdb_scale_test
    scale_test.cpp
)

target_include_directories(tsdb_scale_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_scale_test
    PRIVATE
        tsdb_lib
        benchmark::benchmark
        benchmark::benchmark_main
        Threads::Threads
)

# Register as a test so CTest picks it up
add_test(NAME tsdb_scale_test COMMAND tsdb_scale_test)