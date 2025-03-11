# Unit test executable
add_executable(tsdb_unit_tests)

target_include_directories(tsdb_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
)

# Test fixtures
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    add_custom_command(TARGET tsdb_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
endif()

# Register tests
gtest_discover_tests(tsdb_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
) 