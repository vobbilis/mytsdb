# Integration Testing Configuration

# gtest_discover_tests runs test executables at *build time* to enumerate tests.
# Some of our integration test binaries have non-trivial startup cost and can
# exceed CMake's default discovery timeout, causing flaky build failures.
#
# Increase discovery timeout globally for this directory.
set(CTEST_DISCOVERY_TIMEOUT 600)

# Force an explicit discovery timeout for every gtest_discover_tests invocation.
function(tsdb_gtest_discover_tests target)
    gtest_discover_tests(${target} ${ARGN} DISCOVERY_TIMEOUT 600)
endfunction()

# Main integration test executable
add_executable(tsdb_integration_test_suite
    core_storage_integration_test.cpp
    storage_histogram_integration_test.cpp
    config_integration_test.cpp
    derived_metrics_integration_test.cpp
    negative_matchers_integration_test.cpp
    time_bounds_pruning_integration_test.cpp
    storageimpl_phases/phase1_object_pool_integration_test.cpp
    storageimpl_phases/debug_stats_test.cpp
    storageimpl_phases/object_pool_education_test.cpp
    storageimpl_phases/object_pool_size_analysis_test.cpp
    storageimpl_phases/phase2_cache_hierarchy_integration_test.cpp
    storageimpl_phases/debug_cache_test.cpp
    storageimpl_phases/phase3_compression_integration_test.cpp
    storageimpl_phases/phase2_block_management_integration_test.cpp
    storageimpl_phases/phase2_background_processing_integration_test.cpp
    storageimpl_phases/phase2_predictive_caching_integration_test.cpp
    storageimpl_phases/comprehensive_integration_test.cpp
    storageimpl_phases/write_path_refactoring_integration_test.cpp
    storage_persistence_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

if(HAVE_GRPC AND ENABLE_OTEL)
    target_sources(tsdb_integration_test_suite PRIVATE
        otel_integration_test.cpp
        grpc_integration_test.cpp
        multi_component_test.cpp
        end_to_end_workflow_test.cpp
        error_handling_test.cpp
        recovery_test.cpp
    )
endif()

# Add semantic vector integration tests when enabled
if(TSDB_SEMVEC)
    target_sources(tsdb_integration_test_suite PRIVATE
        semantic_vector_integration_test.cpp
        semantic_vector_performance_test.cpp
        semantic_vector_stress_test.cpp
    )
endif()

target_include_directories(tsdb_integration_test_suite
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

                target_link_libraries(tsdb_integration_test_suite
                    PRIVATE
                        tsdb_test_common
                        tsdb_lib
                        GTest::gtest
                        GTest::gtest_main
                        Threads::Threads
                )



# Storage Stability Test
add_executable(tsdb_storage_stability_test
    storage_stability_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_storage_stability_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_storage_stability_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        GTest::gtest
        GTest::gtest_main
)

add_executable(tsdb_core_storage_integration_test
    core_storage_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_core_storage_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

                target_link_libraries(tsdb_core_storage_integration_test
                    PRIVATE
                        tsdb_test_common
                        tsdb_storage_impl
                        tsdb_core_impl
                        GTest::gtest
                        GTest::gtest_main
                )

add_executable(tsdb_storage_histogram_integration_test
    storage_histogram_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_storage_histogram_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_storage_histogram_integration_test
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_histogram_impl
        tsdb_core_impl
        GTest::gtest
        GTest::gtest_main
)

add_executable(tsdb_config_integration_test
    config_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_config_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_config_integration_test
    PRIVATE
        tsdb_test_common
        tsdb_core_impl
        GTest::gtest
        GTest::gtest_main
)

if(HAVE_GRPC AND ENABLE_OTEL)
add_executable(tsdb_otel_integration_test
    otel_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_otel_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_otel_integration_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        GTest::gtest
        GTest::gtest_main
)

add_executable(tsdb_grpc_integration_test
    grpc_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_grpc_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_grpc_integration_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        GTest::gtest
        GTest::gtest_main
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
add_executable(tsdb_end_to_end_workflow_test
    end_to_end_workflow_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_end_to_end_workflow_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_end_to_end_workflow_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
add_executable(tsdb_multi_component_test
    multi_component_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_multi_component_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_multi_component_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
add_executable(tsdb_error_handling_test
    error_handling_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_error_handling_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_error_handling_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        GTest::gtest
        GTest::gtest_main
)
endif()

add_executable(tsdb_hybrid_query_test
    parquet/hybrid_query_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_hybrid_query_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_hybrid_query_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        GTest::gtest
        GTest::gtest_main
)

# Secondary Index Integration Test - tests that ParquetBlock uses secondary index
add_executable(tsdb_secondary_index_integration_test
    parquet/secondary_index_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_secondary_index_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_secondary_index_integration_test
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
        Arrow::arrow_shared
        Parquet::parquet_shared
        GTest::gtest
        GTest::gtest_main
)

tsdb_gtest_discover_tests(tsdb_secondary_index_integration_test 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        RESOURCE_LOCK TSDB_STORAGE
        TIMEOUT 300
)

# Bloom Filter + B+ Tree Integration Test - tests Phase 0 + Phase 1 working together
add_executable(tsdb_bloom_btree_integration_test
    parquet/bloom_btree_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_bloom_btree_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_bloom_btree_integration_test
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
        Arrow::arrow_shared
        Parquet::parquet_shared
        GTest::gtest
        GTest::gtest_main
)

tsdb_gtest_discover_tests(tsdb_bloom_btree_integration_test 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        RESOURCE_LOCK TSDB_STORAGE
        TIMEOUT 300
)

# Query instrumentation integration test
add_executable(tsdb_query_instrumentation_integration_test
    query_instrumentation_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/server/self_monitor.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_query_instrumentation_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_query_instrumentation_integration_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        tsdb_prometheus
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

# Self-monitor end-to-end test (standalone executable, not using GTest)
add_executable(tsdb_self_monitor_e2e_test
    self_monitor_e2e_test.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/server/self_monitor.cpp
)

target_include_directories(tsdb_self_monitor_e2e_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_self_monitor_e2e_test
    PRIVATE
        tsdb_lib
        tsdb_prometheus
        tsdb_storage_impl
        Threads::Threads
)

# Test fixtures
#
# IMPORTANT:
# Multiple integration executables were previously running the same POST_BUILD
# `copy_directory` into `${CMAKE_CURRENT_BINARY_DIR}/data` in parallel, which can
# intermittently fail under parallel builds.
#
# Fix: copy the fixtures ONCE via a stamped custom target, then add dependencies.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    set(TSDB_INTEGRATION_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/data)
    set(TSDB_INTEGRATION_DATA_STAMP ${TSDB_INTEGRATION_DATA_DIR}/.tsdb_test_data_copied)

    add_custom_command(
        OUTPUT ${TSDB_INTEGRATION_DATA_STAMP}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TSDB_INTEGRATION_DATA_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${TSDB_INTEGRATION_DATA_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${TSDB_INTEGRATION_DATA_STAMP}
        COMMENT "Copying integration test fixtures to ${TSDB_INTEGRATION_DATA_DIR}"
    )

    add_custom_target(tsdb_copy_integration_test_data DEPENDS ${TSDB_INTEGRATION_DATA_STAMP})

    add_dependencies(tsdb_integration_test_suite tsdb_copy_integration_test_data)
    add_dependencies(tsdb_core_storage_integration_test tsdb_copy_integration_test_data)
    add_dependencies(tsdb_storage_histogram_integration_test tsdb_copy_integration_test_data)
    add_dependencies(tsdb_config_integration_test tsdb_copy_integration_test_data)

    if(HAVE_GRPC AND ENABLE_OTEL)
        add_dependencies(tsdb_otel_integration_test tsdb_copy_integration_test_data)
        add_dependencies(tsdb_grpc_integration_test tsdb_copy_integration_test_data)
        add_dependencies(tsdb_end_to_end_workflow_test tsdb_copy_integration_test_data)
        add_dependencies(tsdb_multi_component_test tsdb_copy_integration_test_data)
        add_dependencies(tsdb_error_handling_test tsdb_copy_integration_test_data)
    endif()
endif()

# Register integration tests
tsdb_gtest_discover_tests(tsdb_integration_test_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
        TIMEOUT 1800  # 30 minutes timeout for integration tests
)

tsdb_gtest_discover_tests(tsdb_core_storage_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
        TIMEOUT 600   # 10 minutes timeout for core-storage integration tests
)

tsdb_gtest_discover_tests(tsdb_storage_histogram_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
        TIMEOUT 600   # 10 minutes timeout for storage-histogram integration tests
)

tsdb_gtest_discover_tests(tsdb_config_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_CONFIG
        TIMEOUT 300   # 5 minutes timeout for config integration tests
)

# Prometheus Remote Storage integration tests
add_subdirectory(prometheus)

if(HAVE_GRPC AND ENABLE_OTEL)
tsdb_gtest_discover_tests(tsdb_otel_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 900   # 15 minutes timeout for OTEL integration tests
)

tsdb_gtest_discover_tests(tsdb_grpc_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 900   # 15 minutes timeout for gRPC integration tests
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
tsdb_gtest_discover_tests(tsdb_end_to_end_workflow_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 1200  # 20 minutes timeout for end-to-end workflow tests
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
tsdb_gtest_discover_tests(tsdb_multi_component_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 900   # 15 minutes timeout for multi-component tests
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
tsdb_gtest_discover_tests(tsdb_error_handling_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 600   # 10 minutes timeout for error handling tests
)
endif()

tsdb_gtest_discover_tests(tsdb_query_instrumentation_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
        TIMEOUT 600   # 10 minutes timeout for query instrumentation tests
)

add_executable(tsdb_storage_persistence_test
    storage_persistence_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_storage_persistence_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_storage_persistence_test
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
        GTest::gtest
        GTest::gtest_main
)

tsdb_gtest_discover_tests(tsdb_storage_persistence_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
        TIMEOUT 600   # 10 minutes timeout for storage persistence tests
)

add_executable(tsdb_storage_demotion_test
    storage_demotion_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_storage_demotion_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_storage_demotion_test
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
        GTest::gtest
        GTest::gtest_main
)

tsdb_gtest_discover_tests(tsdb_storage_demotion_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        RESOURCE_LOCK TSDB_STORAGE
        TIMEOUT 600   # 10 minutes timeout for storage demotion tests
)

# Index Performance Integration Test (Roaring Bitmap validation)
add_executable(tsdb_index_performance_integration_test
    index_performance_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_index_performance_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_index_performance_integration_test
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

tsdb_gtest_discover_tests(tsdb_index_performance_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 300  # 5 minutes for performance tests
)