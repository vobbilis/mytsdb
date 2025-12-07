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

# OTEL gRPC Write Performance Benchmark (only if gRPC is available)
if(HAVE_GRPC AND ENABLE_OTEL)
    add_executable(otel_write_perf_bench otel_write_performance_benchmark.cpp)
    target_include_directories(otel_write_perf_bench PRIVATE 
        ${CMAKE_SOURCE_DIR}/src 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
    )
    
    # Don't add proto source files directly - they're already linked via tsdb_otel_impl
    # Adding them again causes protobuf descriptor database conflicts
    target_compile_definitions(otel_write_perf_bench PRIVATE HAVE_GRPC=1)
    target_link_libraries(otel_write_perf_bench PRIVATE 
        tsdb_test_common 
        tsdb_lib 
        benchmark::benchmark 
        benchmark::benchmark_main 
        gRPC::grpc++ 
        protobuf::libprotobuf
    )
    add_test(NAME benchmark_otel_write COMMAND otel_write_perf_bench)
endif()

# Register tests
add_test(NAME benchmark_write COMMAND write_perf_bench)
# NOTE: benchmark_read is disabled due to flaky SEGFAULT during parallel test execution
# The test passes when run individually. This is a known issue with resource contention.
# add_test(NAME benchmark_read COMMAND read_perf_bench)
add_test(NAME benchmark_mixed COMMAND mixed_perf_bench)
add_test(NAME benchmark_latency COMMAND latency_perf_bench)
add_test(NAME benchmark_concurrency COMMAND concurrency_perf_bench)
add_test(NAME benchmark_new_vs_update COMMAND new_vs_update_perf_bench)

# Parquet Performance
if(HAVE_ARROW AND HAVE_PARQUET)
    add_executable(parquet_perf_bench parquet/parquet_benchmark.cpp)
    target_include_directories(parquet_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(parquet_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest Arrow::arrow_shared Parquet::parquet_shared)
    add_test(NAME benchmark_parquet COMMAND parquet_perf_bench)

    add_executable(parquet_high_cardinality_bench parquet_high_cardinality_benchmark.cpp)
    target_include_directories(parquet_high_cardinality_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(parquet_high_cardinality_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest Arrow::arrow_shared Parquet::parquet_shared)
    # Temporarily disabled - flaky/slow
    # add_test(NAME benchmark_parquet_high_cardinality COMMAND parquet_high_cardinality_bench)
endif()

# Rule Manager Performance
add_executable(rule_manager_perf_bench rule_manager_benchmark.cpp)
target_include_directories(rule_manager_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(rule_manager_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)
add_test(NAME benchmark_rule_manager COMMAND rule_manager_perf_bench)

# Filtering Storage Performance
add_executable(filtering_storage_perf_bench filtering_storage_benchmark.cpp)
target_include_directories(filtering_storage_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(filtering_storage_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)
add_test(NAME benchmark_filtering_storage COMMAND filtering_storage_perf_bench)

# Scale Test
add_executable(scale_test ../load/scale_test.cpp)
target_include_directories(scale_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(scale_test PRIVATE
    tsdb_test_common
    tsdb_lib
    benchmark::benchmark
    benchmark::benchmark_main
    GTest::gtest
)
add_test(NAME benchmark_scale COMMAND scale_test)

# PromQL Cache Performance
add_executable(promql_cache_perf_bench promql_cache_benchmark.cpp)
target_include_directories(promql_cache_perf_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(promql_cache_perf_bench PRIVATE tsdb_test_common tsdb_lib benchmark::benchmark benchmark::benchmark_main GTest::gtest)
add_test(NAME benchmark_promql_cache COMMAND promql_cache_perf_bench)

# E2E Query Benchmark (for read performance optimization validation)
add_executable(e2e_query_benchmark e2e_query_benchmark.cpp)
target_include_directories(e2e_query_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(e2e_query_benchmark PRIVATE tsdb_test_common tsdb_lib GTest::gtest GTest::gtest_main)
add_test(NAME benchmark_e2e_query COMMAND e2e_query_benchmark)



