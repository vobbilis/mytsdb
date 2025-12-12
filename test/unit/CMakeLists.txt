# gtest_discover_tests runs test executables at *build time* to enumerate tests.
# Some binaries can exceed CMake's default discovery timeout on slower/loaded
# machines, causing flaky build failures.
set(CTEST_DISCOVERY_TIMEOUT 600)

# gtest_discover_tests runs test executables at *build time* to enumerate tests.
# On some machines, discovery can take longer than CMake's default timeout,
# causing flaky build failures. Force an explicit timeout for every discovery call.
function(tsdb_gtest_discover_tests target)
    gtest_discover_tests(${target} ${ARGN} DISCOVERY_TIMEOUT 600)
endfunction()

# Unit test executables
add_executable(tsdb_core_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/core/result_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/core/error_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/core/config_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Storage unit tests
add_executable(tsdb_storage_unit_tests
    ${CMAKE_SOURCE_DIR}/test/main.cpp
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/storage_test.cpp
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/block_management_test.cpp
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/compression_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/storage/object_pool_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/storage/working_set_cache_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/storage/working_set_cache_test.cpp
)

# Reproduction test for Result crash
add_executable(tsdb_result_repro_test
    ${CMAKE_SOURCE_DIR}/test/unit/core/result_test_repro.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/core/internal/types.cpp
)
target_include_directories(tsdb_result_repro_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(tsdb_result_repro_test PRIVATE tsdb_core_impl tsdb_test_common)
tsdb_gtest_discover_tests(tsdb_result_repro_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Parquet Schema Mapper unit tests
if(HAVE_ARROW)
    add_executable(tsdb_schema_mapper_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/schema_mapper_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_schema_mapper_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_schema_mapper_unit_tests PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared)
    tsdb_gtest_discover_tests(tsdb_schema_mapper_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(tsdb_parquet_writer_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/writer_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_parquet_writer_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_parquet_writer_unit_tests PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared Parquet::parquet_shared)
    tsdb_gtest_discover_tests(tsdb_parquet_writer_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(tsdb_parquet_reader_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/reader_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_parquet_reader_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_parquet_reader_unit_tests PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared Parquet::parquet_shared)
    tsdb_gtest_discover_tests(tsdb_parquet_reader_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Secondary Index (B+ Tree) unit tests - Phase A
    add_executable(tsdb_secondary_index_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/secondary_index_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_secondary_index_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_secondary_index_unit_tests PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared Parquet::parquet_shared)
    tsdb_gtest_discover_tests(tsdb_secondary_index_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Bloom Filter unit tests - Phase 0
    add_executable(tsdb_bloom_filter_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/bloom_filter_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_bloom_filter_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_bloom_filter_unit_tests PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared Parquet::parquet_shared)
    tsdb_gtest_discover_tests(tsdb_bloom_filter_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(tsdb_parquet_advanced_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/advanced_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_parquet_advanced_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_parquet_advanced_unit_tests PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared Parquet::parquet_shared)
    tsdb_gtest_discover_tests(tsdb_parquet_advanced_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(tsdb_schema_mapper_roundtrip_test
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/schema_mapper_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_schema_mapper_roundtrip_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_schema_mapper_roundtrip_test PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared Parquet::parquet_shared)
    tsdb_gtest_discover_tests(tsdb_schema_mapper_roundtrip_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(tsdb_block_impl_test
        ${CMAKE_SOURCE_DIR}/test/unit/storage/internal/block_impl_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_block_impl_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_block_impl_test PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl)
    tsdb_gtest_discover_tests(tsdb_block_impl_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(tsdb_parquet_integration_test
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/integration_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_parquet_integration_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_parquet_integration_test PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared Parquet::parquet_shared)
    tsdb_gtest_discover_tests(tsdb_parquet_integration_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(tsdb_schema_evolution_test
        ${CMAKE_SOURCE_DIR}/test/unit/storage/parquet/schema_evolution_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_schema_evolution_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(tsdb_schema_evolution_test PRIVATE tsdb_test_common tsdb_storage_impl tsdb_core_impl Arrow::arrow_shared Parquet::parquet_shared)
    tsdb_gtest_discover_tests(tsdb_schema_evolution_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()



# Object pool unit tests
add_executable(tsdb_object_pool_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/object_pool_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Working set cache unit tests
add_executable(tsdb_working_set_cache_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/working_set_cache_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Adaptive compressor unit tests

# Delta-of-delta encoder unit tests
add_executable(tsdb_delta_of_delta_encoder_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/delta_of_delta_encoder_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Atomic metrics unit tests
add_executable(tsdb_atomic_metrics_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/atomic_metrics_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Performance config unit tests
add_executable(tsdb_performance_config_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/performance_config_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Sharded write buffer unit tests
add_executable(tsdb_sharded_wal_unit_tests
    ${CMAKE_SOURCE_DIR}/test/storage/sharded_wal_test.cpp
    ${CMAKE_SOURCE_DIR}/test/storage/async_wal_test.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/storage/sharded_wal.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/storage/wal.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_sharded_wal_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_sharded_wal_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_core_impl
)

tsdb_gtest_discover_tests(tsdb_sharded_wal_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
)

# Sharded index unit tests
add_executable(tsdb_sharded_index_unit_tests
    ${CMAKE_SOURCE_DIR}/test/storage/sharded_index_test.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/storage/sharded_index.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/storage/index.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_sharded_index_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_sharded_index_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_core_impl
)

tsdb_gtest_discover_tests(tsdb_sharded_index_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

# Index Roaring Bitmap Performance Tests
add_executable(tsdb_index_roaring_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/index_roaring_test.cpp
)

target_include_directories(tsdb_index_roaring_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_index_roaring_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

tsdb_gtest_discover_tests(tsdb_index_roaring_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

# Background processor unit tests
add_executable(tsdb_background_processor_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/background_processor_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Lock-free queue unit tests
add_executable(tsdb_lock_free_queue_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/lock_free_queue_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Cache hierarchy unit tests
add_executable(tsdb_cache_hierarchy_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/cache_hierarchy_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Cache hierarchy reproduction test
add_executable(tsdb_cache_hierarchy_repro_test
    ${CMAKE_SOURCE_DIR}/test/storage/cache_hierarchy_repro_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_cache_hierarchy_repro_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_cache_hierarchy_repro_test
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

tsdb_gtest_discover_tests(tsdb_cache_hierarchy_repro_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

# Predictive cache unit tests
add_executable(tsdb_predictive_cache_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/predictive_cache_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Atomic reference counted unit tests
add_executable(tsdb_atomic_ref_counted_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/atomic_ref_counted_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Prometheus unit tests
if(HAVE_RAPIDJSON)
    find_package(absl QUIET)

    add_executable(tsdb_prometheus_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/types_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/http_server_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/labels_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/promql_engine_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/promql_parser_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/vector_matching_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/functions_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/aggregation_advanced_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/label_manipulation_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/utility_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/remaining_functions_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/remote/test_converter.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/auth/test_authenticators.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/promql/vectorized_evaluator_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/result_minimal_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_include_directories(tsdb_prometheus_unit_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
            ${RAPIDJSON_INCLUDE_DIRS}

    )
    
    target_link_libraries(tsdb_prometheus_unit_tests
        PRIVATE
            tsdb_test_common
            tsdb_prometheus
            GTest::gmock
    )
    


    if(HAVE_HTTPLIB)
        target_link_libraries(tsdb_prometheus_unit_tests PRIVATE httplib::httplib)
    endif()

    if(HAVE_RAPIDJSON)
        target_link_libraries(tsdb_prometheus_unit_tests PRIVATE ${RAPIDJSON_LIBRARIES})
    endif()
    
    tsdb_gtest_discover_tests(tsdb_prometheus_unit_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
            RESOURCE_LOCK HTTP_PORT
    )
endif()
    
    # Query metrics unit tests
    add_executable(tsdb_query_metrics_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/promql/query_metrics_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    
    target_include_directories(tsdb_query_metrics_unit_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(tsdb_query_metrics_unit_tests
        PRIVATE
            tsdb_test_common
            tsdb_prometheus
    )
    
    tsdb_gtest_discover_tests(tsdb_query_metrics_unit_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Storage adapter unit tests
    add_executable(tsdb_storage_adapter_unit_tests
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/promql/storage_adapter_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    
    target_include_directories(tsdb_storage_adapter_unit_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(tsdb_storage_adapter_unit_tests
        PRIVATE
            tsdb_test_common
            tsdb_prometheus
            GTest::gmock
    )

    # Performance Benchmark
    add_executable(tsdb_promql_benchmark
        ${CMAKE_SOURCE_DIR}/test/unit/prometheus/promql/range_benchmark_test.cpp
    )
    
    target_include_directories(tsdb_promql_benchmark
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(tsdb_promql_benchmark
        PRIVATE
            tsdb_prometheus
            benchmark::benchmark
    )

add_executable(tsdb_wal_backpressure_test
    storage/wal_backpressure_test.cpp
)
target_link_libraries(tsdb_wal_backpressure_test
    PRIVATE
    tsdb_test_common
    tsdb_storage_impl
)

add_executable(tsdb_pruning_test
    storage/parquet/pruning_test.cpp
)
target_link_libraries(tsdb_pruning_test
    PRIVATE
    tsdb_test_common
    tsdb_storage_impl
)

add_executable(tsdb_predicate_pushdown_test
    storage/parquet/predicate_pushdown_test.cpp
)
target_link_libraries(tsdb_predicate_pushdown_test
    PRIVATE
    tsdb_test_common
    tsdb_storage_impl
)

add_executable(tsdb_index_deletion_test
    storage/index_deletion_test.cpp
)
target_link_libraries(tsdb_index_deletion_test
    PRIVATE
    tsdb_test_common
    tsdb_storage_impl
)


    
    #     gtest_discover_tests(tsdb_storage_adapter_unit_tests
    #         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    #         PROPERTIES
    #             ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    #     )
    # endif()

add_executable(tsdb_integration_tests
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/storage_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# ============================================================================
# PHASE 1: MEMORY OPTIMIZATION UNIT TESTS
# ============================================================================

# Simple Memory Optimization Tests - ENABLED (working components)

# Enhanced Object Pool Tests - ENABLED (fixing interface issues)

# Memory Optimization Tests - DISABLED (compilation issues)

# Sequential Layout Optimizer Tests - ENABLED (working components)

# Access Pattern Optimizer Tests - ENABLED (working components)

# Adaptive Memory Integration Tests - ENABLED (fixing compilation issues)

# Adaptive Memory Integration NEW Tests - REDESIGNED VERSION

# Adaptive Memory Integration MINIMAL Tests - ISOLATION TEST

# Adaptive Memory Integration WORKING Tests - WORKING VERSION

# Adaptive Memory Integration SIMPLE Tests - SIMPLE VERSION

# Tiered Memory Integration Tests - ENABLED (fixing compilation issues)

# Core unit tests configuration
target_include_directories(tsdb_core_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_core_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_core_impl
)

# Simple Memory Optimization Tests configuration


# Storage unit tests configuration
target_include_directories(tsdb_storage_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_storage_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)



# Object pool unit tests configuration
target_include_directories(tsdb_object_pool_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_object_pool_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

target_link_libraries(tsdb_working_set_cache_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Adaptive compressor unit tests configuration


# Delta-of-delta encoder unit tests configuration
target_include_directories(tsdb_delta_of_delta_encoder_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_delta_of_delta_encoder_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Atomic metrics unit tests configuration
target_include_directories(tsdb_atomic_metrics_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_atomic_metrics_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Performance config unit tests configuration
target_include_directories(tsdb_performance_config_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_performance_config_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Sharded write buffer unit tests configuration


# Background processor unit tests configuration
target_include_directories(tsdb_background_processor_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_background_processor_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Lock-free queue unit tests configuration
target_include_directories(tsdb_lock_free_queue_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_lock_free_queue_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Cache hierarchy unit tests configuration
target_include_directories(tsdb_cache_hierarchy_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_cache_hierarchy_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Predictive cache unit tests configuration
target_include_directories(tsdb_predictive_cache_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_predictive_cache_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Atomic reference counted unit tests configuration
target_include_directories(tsdb_atomic_ref_counted_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_atomic_ref_counted_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# ============================================================================
# PHASE 1: MEMORY OPTIMIZATION UNIT TESTS CONFIGURATION
# ============================================================================

# Enhanced Object Pool Tests Configuration - ENABLED


# Memory Optimization Tests Configuration - DISABLED


# Sequential Layout Optimizer Tests Configuration - ENABLED


# Access Pattern Optimizer Tests Configuration - ENABLED


# Adaptive Memory Integration Tests Configuration - ENABLED


# Adaptive Memory Integration NEW Tests Configuration - REDESIGNED VERSION


# Adaptive Memory Integration MINIMAL Tests Configuration - ISOLATION TEST


# Adaptive Memory Integration WORKING Tests Configuration - WORKING VERSION


# Adaptive Memory Integration SIMPLE Tests Configuration - SIMPLE VERSION


# Tiered Memory Integration Tests Configuration - ENABLED


# Integration tests configuration
target_include_directories(tsdb_integration_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_integration_tests
    PRIVATE
        tsdb_test_common
        tsdb_lib
)

# Semantic vector smoke tests (gated by TSDB_SEMVEC)
if(TSDB_SEMVEC)
    add_executable(tsdb_semvec_smoke_tests
        ${CMAKE_SOURCE_DIR}/test/unit/semantic_vector/quantized_vector_index_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/semantic_vector/pruned_semantic_index_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/semantic_vector/sparse_temporal_graph_test.cpp
        ${CMAKE_SOURCE_DIR}/test/unit/semantic_vector/memory_management_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    target_include_directories(tsdb_semvec_smoke_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(tsdb_semvec_smoke_tests PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
    )
    gtest_discover_tests(tsdb_semvec_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )
endif()

# Test fixtures
#
# IMPORTANT:
# Multiple unit test executables were previously running the same POST_BUILD
# `copy_directory` into `${CMAKE_CURRENT_BINARY_DIR}/data` in parallel, which can
# intermittently fail under parallel builds.
#
# Fix: copy the fixtures ONCE via a stamped custom target, then add dependencies.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    set(TSDB_UNIT_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/data)
    set(TSDB_UNIT_DATA_STAMP ${TSDB_UNIT_DATA_DIR}/.tsdb_test_data_copied)

    add_custom_command(
        OUTPUT ${TSDB_UNIT_DATA_STAMP}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TSDB_UNIT_DATA_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${TSDB_UNIT_DATA_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${TSDB_UNIT_DATA_STAMP}
        COMMENT "Copying unit test fixtures to ${TSDB_UNIT_DATA_DIR}"
    )

    add_custom_target(tsdb_copy_unit_test_data DEPENDS ${TSDB_UNIT_DATA_STAMP})

    add_dependencies(tsdb_core_unit_tests tsdb_copy_unit_test_data)
    add_dependencies(tsdb_storage_unit_tests tsdb_copy_unit_test_data)
    add_dependencies(tsdb_integration_tests tsdb_copy_unit_test_data)
    add_dependencies(tsdb_object_pool_unit_tests tsdb_copy_unit_test_data)
    add_dependencies(tsdb_working_set_cache_unit_tests tsdb_copy_unit_test_data)
endif()
tsdb_gtest_discover_tests(tsdb_storage_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
)

tsdb_gtest_discover_tests(tsdb_integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
)



tsdb_gtest_discover_tests(tsdb_object_pool_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

tsdb_gtest_discover_tests(tsdb_working_set_cache_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)


tsdb_gtest_discover_tests(tsdb_delta_of_delta_encoder_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

tsdb_gtest_discover_tests(tsdb_atomic_metrics_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

tsdb_gtest_discover_tests(tsdb_performance_config_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)


tsdb_gtest_discover_tests(tsdb_background_processor_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_THREADS
)

tsdb_gtest_discover_tests(tsdb_lock_free_queue_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

tsdb_gtest_discover_tests(tsdb_cache_hierarchy_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

tsdb_gtest_discover_tests(tsdb_predictive_cache_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

tsdb_gtest_discover_tests(tsdb_atomic_ref_counted_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

# Semantic vector tests (gated by TSDB_SEMVEC option)
if(TSDB_SEMVEC)
    # Semantic vector quantized vector index smoke tests
    add_executable(tsdb_semvec_quantized_vector_index_smoke_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/quantized_vector_index_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_quantized_vector_index_smoke_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_quantized_vector_index_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Semantic vector pruned semantic index smoke tests
    add_executable(tsdb_semvec_pruned_semantic_index_smoke_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/pruned_semantic_index_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_pruned_semantic_index_smoke_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_pruned_semantic_index_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Semantic vector sparse temporal graph smoke tests
    add_executable(tsdb_semvec_sparse_temporal_graph_smoke_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/sparse_temporal_graph_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_sparse_temporal_graph_smoke_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_sparse_temporal_graph_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Semantic vector memory management smoke tests
    add_executable(tsdb_semvec_memory_management_smoke_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/memory_management_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_memory_management_smoke_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_memory_management_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Semantic vector compression smoke tests
    add_executable(tsdb_semvec_compression_smoke_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/compression_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_compression_smoke_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_compression_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Semantic vector advanced analytics smoke tests
    add_executable(tsdb_semvec_analytics_smoke_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/advanced_analytics_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_analytics_smoke_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_analytics_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Semantic vector query processing smoke tests
    add_executable(tsdb_semvec_query_processor_smoke_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/query_processor_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_query_processor_smoke_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_query_processor_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Semantic vector migration manager smoke tests
    add_executable(tsdb_semvec_migration_manager_smoke_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/migration_manager_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_migration_manager_smoke_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_migration_manager_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )

    # Semantic vector architecture validation tests
    add_executable(tsdb_semvec_architecture_validation_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/semantic_vector/architecture_validation_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )

    target_link_libraries(tsdb_semvec_architecture_validation_tests
        tsdb_storage_impl
        tsdb_core
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(tsdb_semvec_architecture_validation_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES
            ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
    )
endif() 