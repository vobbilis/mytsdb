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
)

add_executable(tsdb_histogram_unit_tests
    ${CMAKE_SOURCE_DIR}/test/histogram/ddsketch_test.cpp
    ${CMAKE_SOURCE_DIR}/test/histogram/fixed_bucket_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

add_executable(tsdb_integration_tests
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/storage_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

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

# Histogram unit tests configuration
target_include_directories(tsdb_histogram_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_histogram_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_histogram_impl
)

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

# Test fixtures
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    add_custom_command(TARGET tsdb_core_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_storage_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_integration_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_histogram_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
endif()

# Register tests
gtest_discover_tests(tsdb_core_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_storage_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_histogram_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
) 