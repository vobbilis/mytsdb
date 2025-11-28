include(GoogleTest)
enable_testing()

# Common test settings
add_library(tsdb_test_common INTERFACE)
target_include_directories(tsdb_test_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR} # for test_config.h.in resolution by tests
    ${CMAKE_CURRENT_BINARY_DIR} # for generated test_config.h
    ${CMAKE_SOURCE_DIR}/include # to find tsdb headers from tests
    ${CMAKE_SOURCE_DIR}/src # Potentially too broad, let specific targets add what they need from src
)
target_link_libraries(tsdb_test_common INTERFACE
    GTest::GTest
    GTest::Main
    tsdb_lib       # Link against the main library
)

# Test data configuration
set(TSDB_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/test_config.h
)

# Unit tests
add_subdirectory(unit)

# Integration tests
add_subdirectory(integration)
add_subdirectory(comprehensive_promql)


# Performance benchmark tests
add_subdirectory(benchmark)

# OTEL/gRPC tests (only if gRPC is available)
if(HAVE_GRPC)
    add_subdirectory(otel_grpc)
endif()

# Phase 1: Memory Access Pattern Optimization Tests




set(INTEGRATION_TEST_TARGETS
    tsdb_integration_test_suite
    tsdb_core_storage_integration_test
    tsdb_storage_histogram_integration_test
    tsdb_config_integration_test
    tsdb_otel_integration_test
    tsdb_grpc_integration_test
    tsdb_end_to_end_workflow_test
    tsdb_multi_component_test
    tsdb_error_handling_test
)

set(PARSER_TEST_TARGETS
    # No parser tests currently available
)

# =============================================================================
# TEST EXECUTION TARGETS
# =============================================================================

# Individual test category targets
add_custom_target(test-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running all unit tests - 357 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1 
        ctest --test-dir test/unit --output-junit ${CMAKE_BINARY_DIR}/test-results/unit/unit_tests.xml --timeout 30
    DEPENDS ${UNIT_TEST_TARGETS}
    COMMENT "Running all unit tests - 357 tests (30s timeout per test)"
)

add_custom_target(test-integration
    COMMAND ${CMAKE_COMMAND} -E echo "Running all integration tests - 177 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/integration
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1 
        ctest --test-dir test/integration --output-junit ${CMAKE_BINARY_DIR}/test-results/integration/integration_tests.xml --timeout 60
    DEPENDS ${INTEGRATION_TEST_TARGETS}
    COMMENT "Running all integration tests - 177 tests (60s timeout per test)"
)

add_custom_target(test-parser
    COMMAND ${CMAKE_COMMAND} -E echo "Running PromQL parser tests - 34 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/parser
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1 
        ctest --test-dir test --output-junit ${CMAKE_BINARY_DIR}/test-results/parser/parser_tests.xml --timeout 15
    DEPENDS ${PARSER_TEST_TARGETS}
    COMMENT "Running PromQL parser tests - 34 tests (15s timeout per test)"
)

# Comprehensive test targets
add_custom_target(test-all
    COMMAND ${CMAKE_COMMAND} -E echo "Running ALL tests - 632+ test cases"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/all
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1 
        ctest --test-dir test --output-junit ${CMAKE_BINARY_DIR}/test-results/all/all_tests.xml --timeout 120
    DEPENDS ${UNIT_TEST_TARGETS} ${INTEGRATION_TEST_TARGETS} ${PARSER_TEST_TARGETS}
    COMMENT "Running comprehensive test suite - 632+ tests (120s timeout per test)"
)

# Main integration test suite (124 tests)
add_custom_target(test-main-integration
    COMMAND ${CMAKE_COMMAND} -E echo "Running main integration test suite - 124 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/main-integration
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 300 ./test/integration/tsdb_integration_test_suite --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/main-integration/main_integration_tests.xml
    DEPENDS tsdb_integration_test_suite
    COMMENT "Running main integration test suite - 124 tests (5min timeout)"
)

# StorageImpl phases tests (64 tests)
add_custom_target(test-storageimpl-phases
    COMMAND ${CMAKE_COMMAND} -E echo "Running StorageImpl phases tests - 64 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/storageimpl-phases
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 600 ./test/integration/tsdb_integration_test_suite --gtest_filter="Phase*:*" --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/storageimpl-phases/storageimpl_phases_tests.xml
    DEPENDS tsdb_integration_test_suite
    COMMENT "Running StorageImpl phases tests - 64 tests (10min timeout)"
)

# Individual test executables
add_custom_target(test-individual-integration
    COMMAND ${CMAKE_COMMAND} -E echo "Running individual integration tests - 53 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/individual-integration
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ./test/integration/tsdb_core_storage_integration_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/individual-integration/core_storage.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ./test/integration/tsdb_storage_histogram_integration_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/individual-integration/storage_histogram.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ./test/integration/tsdb_config_integration_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/individual-integration/config.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ./test/integration/tsdb_otel_integration_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/individual-integration/otel.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ./test/integration/tsdb_grpc_integration_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/individual-integration/grpc.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ./test/integration/tsdb_end_to_end_workflow_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/individual-integration/end_to_end.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ./test/integration/tsdb_multi_component_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/individual-integration/multi_component.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        ./test/integration/tsdb_error_handling_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/individual-integration/error_handling.xml
    DEPENDS ${INTEGRATION_TEST_TARGETS}
    COMMENT "Running individual integration tests - 53 tests"
)

# WAL Performance Benchmark
add_custom_target(test-wal-performance
    COMMAND ${CMAKE_COMMAND} -E echo "Running WAL performance benchmarks - 6 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/wal-performance
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 60 ./test/benchmark/tsdb_wal_performance_benchmark --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/wal-performance/wal_performance_tests.xml
    DEPENDS tsdb_wal_performance_benchmark
    COMMENT "Running WAL performance benchmarks - 6 tests (1min timeout)"
)

# Memory Efficiency Performance Test
add_custom_target(test-memory-efficiency
    COMMAND ${CMAKE_COMMAND} -E echo "Running memory efficiency performance tests - 6 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/memory-efficiency
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 120 ./test/benchmark/tsdb_memory_efficiency_performance_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/memory-efficiency/memory_efficiency_tests.xml
    DEPENDS tsdb_memory_efficiency_performance_test
    COMMENT "Running memory efficiency performance tests - 6 tests (2min timeout)"
)

# Indexing Performance Test
add_custom_target(test-indexing-performance
    COMMAND ${CMAKE_COMMAND} -E echo "Running indexing performance tests - 8 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/indexing-performance
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 120 ./test/benchmark/tsdb_indexing_performance_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/indexing-performance/indexing_performance_tests.xml
    DEPENDS tsdb_indexing_performance_test
    COMMENT "Running indexing performance tests - 8 tests (2min timeout)"
)

# Concurrency Performance Test
add_custom_target(test-concurrency-performance
    COMMAND ${CMAKE_COMMAND} -E echo "Running concurrency performance tests - 8 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/concurrency-performance
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 180 ./test/benchmark/tsdb_concurrency_performance_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/concurrency-performance/concurrency_performance_tests.xml
    DEPENDS tsdb_concurrency_performance_test
    COMMENT "Running concurrency performance tests - 8 tests (3min timeout)"
)

# All Performance Tests
add_custom_target(test-all-performance
    COMMAND ${CMAKE_COMMAND} -E echo "Running all performance tests - 28 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/all-performance
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 300 ./test/benchmark/tsdb_wal_performance_benchmark --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/all-performance/wal_performance_tests.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 300 ./test/benchmark/tsdb_memory_efficiency_performance_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/all-performance/memory_efficiency_tests.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 300 ./test/benchmark/tsdb_indexing_performance_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/all-performance/indexing_performance_tests.xml
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 300 ./test/benchmark/tsdb_concurrency_performance_test --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/all-performance/concurrency_performance_tests.xml
    DEPENDS tsdb_wal_performance_benchmark tsdb_memory_efficiency_performance_test tsdb_indexing_performance_test tsdb_concurrency_performance_test
    COMMENT "Running all performance tests - 28 tests (5min timeout)"
)

# Unit test categories
add_custom_target(test-core-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running core unit tests - 38 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/core-unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 60 ./test/unit/tsdb_core_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/core-unit/core_unit_tests.xml
    DEPENDS tsdb_core_unit_tests
    COMMENT "Running core unit tests - 38 tests (1min timeout)"
)

add_custom_target(test-storage-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running storage unit tests - 60 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/storage-unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 120 ./test/unit/tsdb_storage_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/storage-unit/storage_unit_tests.xml
    DEPENDS tsdb_storage_unit_tests
    COMMENT "Running storage unit tests - 60 tests (2min timeout)"
)

add_custom_target(test-sharded-wal-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running ShardedWAL unit tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/sharded-wal-unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 60 ./test/unit/tsdb_sharded_wal_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/sharded-wal-unit/sharded_wal_unit_tests.xml
    DEPENDS tsdb_sharded_wal_unit_tests
    COMMENT "Running ShardedWAL unit tests (1min timeout)"
)

add_custom_target(test-sharded-index-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running ShardedIndex unit tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/sharded-index-unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 60 ./test/unit/tsdb_sharded_index_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/sharded-index-unit/sharded_index_unit_tests.xml
    DEPENDS tsdb_sharded_index_unit_tests
    COMMENT "Running ShardedIndex unit tests (1min timeout)"
)

add_custom_target(test-cache-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running cache unit tests - 28 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/cache-unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 60 ./test/unit/tsdb_cache_hierarchy_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/cache-unit/cache_unit_tests.xml
    DEPENDS tsdb_cache_hierarchy_unit_tests
    COMMENT "Running cache unit tests - 28 tests (1min timeout)"
)

add_custom_target(test-compression-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running compression unit tests - 19 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/compression-unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 60 ./test/unit/tsdb_adaptive_compressor_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/compression-unit/compression_unit_tests.xml
    DEPENDS tsdb_adaptive_compressor_unit_tests
    COMMENT "Running compression unit tests - 19 tests (1min timeout)"
)

add_custom_target(test-histogram-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running histogram unit tests - 22 tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/histogram-unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 60 ./test/unit/tsdb_histogram_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/histogram-unit/histogram_unit_tests.xml
    DEPENDS tsdb_histogram_unit_tests
    COMMENT "Running histogram unit tests - 22 tests (1min timeout)"
)

# OTEL/gRPC E2E Tests
if(HAVE_GRPC)
    add_custom_target(test-otel-grpc-e2e
        COMMAND ${CMAKE_COMMAND} -E echo "Running OTEL/gRPC E2E tests"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/otel-grpc-e2e
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
            ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1 
            ctest --test-dir test/otel_grpc/e2e --output-junit ${CMAKE_BINARY_DIR}/test-results/otel-grpc-e2e/e2e_tests.xml --timeout 60
        DEPENDS tsdb_grpc_server_e2e_test
        COMMENT "Running OTEL/gRPC E2E tests (60s timeout per test)"
    )
    
    add_custom_target(test-otel-grpc
        COMMAND ${CMAKE_COMMAND} -E echo "Running all OTEL/gRPC tests"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/otel-grpc
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
            ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1 
            ctest --test-dir test/otel_grpc --output-junit ${CMAKE_BINARY_DIR}/test-results/otel-grpc/all_tests.xml --timeout 60
        COMMENT "Running all OTEL/gRPC tests (60s timeout per test)"
    )
endif()

# Test result summary
add_custom_target(test-summary
    COMMAND ${CMAKE_COMMAND} -E echo "=== TEST RESULTS SUMMARY ==="
    COMMAND ${CMAKE_COMMAND} -E echo "Test results are available in: ${CMAKE_BINARY_DIR}/test-results/"
    COMMAND ${CMAKE_COMMAND} -E echo "XML reports generated for each test category"
    COMMAND ${CMAKE_COMMAND} -E echo "All tests have timeout protection to prevent hanging"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available test targets with timeouts:"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-all                    - Run all 632+ tests - 120s timeout per test"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-main-integration      - Run main integration suite - 124 tests - 5min timeout"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-storageimpl-phases    - Run StorageImpl phases - 64 tests - 10min timeout"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-unit                  - Run all unit tests - 357 tests - 30s timeout per test"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-integration           - Run all integration tests - 177 tests - 60s timeout per test"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-parser                - Run PromQL parser tests - 34 tests - 15s timeout per test"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-core-unit             - Run core unit tests - 38 tests - 1min timeout"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-storage-unit          - Run storage unit tests - 60 tests - 2min timeout"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-cache-unit            - Run cache unit tests - 28 tests - 1min timeout"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-compression-unit      - Run compression unit tests - 19 tests - 1min timeout"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-histogram-unit        - Run histogram unit tests - 22 tests - 1min timeout"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-individual-integration - Run individual integration tests - 53 tests"
    if(HAVE_GRPC)
        COMMAND ${CMAKE_COMMAND} -E echo "  make test-otel-grpc-e2e         - Run OTEL/gRPC E2E tests - 60s timeout per test"
        COMMAND ${CMAKE_COMMAND} -E echo "  make test-otel-grpc             - Run all OTEL/gRPC tests - 60s timeout per test"
    endif()
    COMMAND ${CMAKE_COMMAND} -E echo "  make test-clean                 - Clean test results"
    COMMENT "Displaying test results summary"
)

add_custom_target(test-prometheus-unit
    COMMAND ${CMAKE_COMMAND} -E echo "Running Prometheus unit tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results/prometheus-unit
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} 
        timeout 60 ./test/unit/tsdb_prometheus_unit_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/prometheus-unit/prometheus_unit_tests.xml
    DEPENDS tsdb_prometheus_unit_tests
    COMMENT "Running Prometheus unit tests (1min timeout)"
)

# =============================================================================
# TEST CLEANUP TARGETS
# =============================================================================

add_custom_target(test-clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/test-results
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaned test results directory"
    COMMENT "Cleaning test results"
)
