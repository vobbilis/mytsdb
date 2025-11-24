# Performance Benchmark Configuration

include(FetchContent)

# Try to find benchmark package
find_package(benchmark QUIET)

if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found locally, fetching from GitHub...")
    
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    
    # Suppress benchmark's tests and install targets
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(benchmark)
    message(STATUS "Google Benchmark fetched and configured")
endif()

message(STATUS "Performance benchmarks enabled")

# Individual executables for targeted benchmarking

# Write Performance
add_executable(write_perf_bench write_performance_benchmark.cpp)
target_include_directories(write_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(write_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)

# Read Performance
add_executable(read_perf_bench read_performance_benchmark.cpp)
target_include_directories(read_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(read_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)

# Mixed Workload
add_executable(mixed_perf_bench mixed_workload_benchmark.cpp)
target_include_directories(mixed_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mixed_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)

# Latency
add_executable(latency_perf_bench latency_benchmark.cpp)
target_include_directories(latency_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(latency_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)

# Concurrency
add_executable(concurrency_perf_bench concurrency_benchmark.cpp)
target_include_directories(concurrency_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(concurrency_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)

# New vs Update Comparison
add_executable(new_vs_update_perf_bench write_new_vs_update_benchmark.cpp)
target_include_directories(new_vs_update_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(new_vs_update_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)

# Register tests
add_test(NAME benchmark_write COMMAND write_perf_bench)
add_test(NAME benchmark_read COMMAND read_perf_bench)
add_test(NAME benchmark_mixed COMMAND mixed_perf_bench)
add_test(NAME benchmark_latency COMMAND latency_perf_bench)
add_test(NAME benchmark_concurrency COMMAND concurrency_perf_bench)
add_test(NAME benchmark_new_vs_update COMMAND new_vs_update_perf_bench)
