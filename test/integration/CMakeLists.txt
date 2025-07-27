# Integration Testing Configuration

# Main integration test executable
add_executable(tsdb_integration_test_suite
    core_storage_integration_test.cpp
    storage_histogram_integration_test.cpp
    config_integration_test.cpp
    otel_integration_test.cpp
    grpc_integration_test.cpp
    end_to_end_workflow_test.cpp
    multi_component_test.cpp
    error_handling_test.cpp
    recovery_test.cpp
    load_test.cpp
    stress_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

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

# Individual integration test executables for specific testing
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

# Test fixtures
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    add_custom_command(TARGET tsdb_integration_test_suite POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_core_storage_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_storage_histogram_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_config_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_otel_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_grpc_integration_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_end_to_end_workflow_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_multi_component_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_error_handling_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
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

gtest_discover_tests(tsdb_end_to_end_workflow_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 1200  # 20 minutes timeout for end-to-end workflow tests
)

gtest_discover_tests(tsdb_multi_component_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 900   # 15 minutes timeout for multi-component tests
)

gtest_discover_tests(tsdb_error_handling_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 600   # 10 minutes timeout for error handling tests
) 