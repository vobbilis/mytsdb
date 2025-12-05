# Integration Testing Configuration

# Main integration test executable
add_executable(tsdb_integration_test_suite
    core_storage_integration_test.cpp
    storage_histogram_integration_test.cpp
    config_integration_test.cpp
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
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    add_custom_command(TARGET tsdb_integration_test_suite POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_core_storage_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_storage_histogram_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_config_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    if(HAVE_GRPC AND ENABLE_OTEL)
    add_custom_command(TARGET tsdb_otel_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_grpc_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    endif()
    if(HAVE_GRPC AND ENABLE_OTEL)
    add_custom_command(TARGET tsdb_end_to_end_workflow_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    endif()
    if(HAVE_GRPC AND ENABLE_OTEL)
    add_custom_command(TARGET tsdb_multi_component_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    endif()
    if(HAVE_GRPC AND ENABLE_OTEL)
    add_custom_command(TARGET tsdb_error_handling_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    endif()
endif()

# Register integration tests
gtest_discover_tests(tsdb_integration_test_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 1800  # 30 minutes timeout for integration tests
)

gtest_discover_tests(tsdb_core_storage_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 600   # 10 minutes timeout for core-storage integration tests
)

gtest_discover_tests(tsdb_storage_histogram_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 600   # 10 minutes timeout for storage-histogram integration tests
)

gtest_discover_tests(tsdb_config_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 300   # 5 minutes timeout for config integration tests
)

# Prometheus Remote Storage integration tests
add_subdirectory(prometheus)

if(HAVE_GRPC AND ENABLE_OTEL)
gtest_discover_tests(tsdb_otel_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 900   # 15 minutes timeout for OTEL integration tests
)

gtest_discover_tests(tsdb_grpc_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 900   # 15 minutes timeout for gRPC integration tests
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
gtest_discover_tests(tsdb_end_to_end_workflow_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 1200  # 20 minutes timeout for end-to-end workflow tests
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
gtest_discover_tests(tsdb_multi_component_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 900   # 15 minutes timeout for multi-component tests
)
endif()

if(HAVE_GRPC AND ENABLE_OTEL)
gtest_discover_tests(tsdb_error_handling_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 600   # 10 minutes timeout for error handling tests
)
endif()

gtest_discover_tests(tsdb_query_instrumentation_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
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

gtest_discover_tests(tsdb_storage_persistence_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
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

gtest_discover_tests(tsdb_storage_demotion_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 600   # 10 minutes timeout for storage demotion tests
)