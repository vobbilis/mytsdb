# Performance Benchmark Configuration

# Try to find benchmark package, but make it optional
find_package(benchmark QUIET)

if(benchmark_FOUND)
    # Main benchmark executable
    add_executable(tsdb_benchmarks
        core_benchmark.cpp
        storage_benchmark.cpp
        histogram_benchmark.cpp
        storage_histogram_benchmark.cpp
        otel_benchmark.cpp
        end_to_end_benchmark.cpp
        concurrent_benchmark.cpp
        wal_performance_benchmark.cpp
        memory_efficiency_performance_test.cpp
        indexing_performance_test.cpp
        concurrency_performance_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_benchmarks
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(tsdb_benchmarks
        PRIVATE
            tsdb_test_common
            tsdb_lib
            benchmark::benchmark
            benchmark::benchmark_main
            GTest::gtest
            GTest::gtest_main
    )

    # Individual benchmark executables for specific testing
    add_executable(tsdb_core_benchmark
        core_benchmark.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_core_benchmark
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(tsdb_core_benchmark
        PRIVATE
            tsdb_test_common
            tsdb_core_impl
            benchmark::benchmark
            benchmark::benchmark_main
    )

    add_executable(tsdb_storage_benchmark
        storage_benchmark.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_storage_benchmark
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(tsdb_storage_benchmark
        PRIVATE
            tsdb_test_common
            tsdb_storage_impl
            tsdb_core_impl
            benchmark::benchmark
            benchmark::benchmark_main
    )

    add_executable(tsdb_histogram_benchmark
        histogram_benchmark.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_histogram_benchmark
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(tsdb_histogram_benchmark
        PRIVATE
            tsdb_test_common
            tsdb_histogram_impl
            benchmark::benchmark
            benchmark::benchmark_main
    )

    # WAL Performance Benchmark
    add_executable(tsdb_wal_performance_benchmark
        wal_performance_benchmark.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_wal_performance_benchmark
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(tsdb_wal_performance_benchmark
        PRIVATE
            tsdb_test_common
            tsdb_lib
            benchmark::benchmark
            benchmark::benchmark_main
            GTest::gtest
            GTest::gtest_main
    )

    # Memory Efficiency Performance Test
    add_executable(tsdb_memory_efficiency_performance_test
        memory_efficiency_performance_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_memory_efficiency_performance_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(tsdb_memory_efficiency_performance_test
        PRIVATE
            tsdb_test_common
            tsdb_lib
            benchmark::benchmark
            benchmark::benchmark_main
            GTest::gtest
            GTest::gtest_main
    )

    # Indexing Performance Test
    add_executable(tsdb_indexing_performance_test
        indexing_performance_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_indexing_performance_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(tsdb_indexing_performance_test
        PRIVATE
            tsdb_test_common
            tsdb_lib
            benchmark::benchmark
            benchmark::benchmark_main
            GTest::gtest
            GTest::gtest_main
    )

    # Concurrency Performance Test
    add_executable(tsdb_concurrency_performance_test
        concurrency_performance_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_concurrency_performance_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(tsdb_concurrency_performance_test
        PRIVATE
            tsdb_test_common
            tsdb_lib
            benchmark::benchmark
            benchmark::benchmark_main
            GTest::gtest
            GTest::gtest_main
    )

    # Test fixtures
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
        add_custom_command(TARGET tsdb_benchmarks POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/../data
                ${CMAKE_CURRENT_BINARY_DIR}/data
        )
        add_custom_command(TARGET tsdb_core_benchmark POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/../data
                ${CMAKE_CURRENT_BINARY_DIR}/data
        )
        add_custom_command(TARGET tsdb_storage_benchmark POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/../data
                ${CMAKE_CURRENT_BINARY_DIR}/data
        )
        add_custom_command(TARGET tsdb_histogram_benchmark POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/../data
                ${CMAKE_CURRENT_BINARY_DIR}/data
        )
        add_custom_command(TARGET tsdb_wal_performance_benchmark POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/../data
                ${CMAKE_CURRENT_BINARY_DIR}/data
        )
    endif()

    # Register benchmark tests
    add_test(NAME tsdb_benchmarks COMMAND tsdb_benchmarks --benchmark_min_time=1)
    add_test(NAME tsdb_core_benchmark COMMAND tsdb_core_benchmark --benchmark_min_time=1)
    add_test(NAME tsdb_storage_benchmark COMMAND tsdb_storage_benchmark --benchmark_min_time=1)
    add_test(NAME tsdb_histogram_benchmark COMMAND tsdb_histogram_benchmark --benchmark_min_time=1)
    add_test(NAME tsdb_wal_performance_benchmark COMMAND tsdb_wal_performance_benchmark --benchmark_min_time=1)
    add_test(NAME tsdb_memory_efficiency_performance_test COMMAND tsdb_memory_efficiency_performance_test --benchmark_min_time=1)
    add_test(NAME tsdb_indexing_performance_test COMMAND tsdb_indexing_performance_test --benchmark_min_time=1)
    add_test(NAME tsdb_concurrency_performance_test COMMAND tsdb_concurrency_performance_test --benchmark_min_time=1)

    message(STATUS "Google Benchmark found - Performance benchmarks enabled")
else()
    message(STATUS "Google Benchmark not found - Performance benchmarks disabled")
    message(STATUS "To enable benchmarks, install Google Benchmark: https://github.com/google/benchmark")
endif() 