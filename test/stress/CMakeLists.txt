# Stress Testing Configuration

# Stress test executables
add_executable(tsdb_stress_tests
    resource_stress_test.cpp
    data_volume_stress_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_stress_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_stress_tests
    PRIVATE
        tsdb_test_common
        tsdb_lib
        gtest
        gtest_main
        Threads::Threads
)

# Individual stress test executables
add_executable(tsdb_resource_stress_test
    resource_stress_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_resource_stress_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_resource_stress_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        gtest
        gtest_main
        Threads::Threads
)

add_executable(tsdb_data_volume_stress_test
    data_volume_stress_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_data_volume_stress_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_data_volume_stress_test
    PRIVATE
        tsdb_test_common
        tsdb_lib
        gtest
        gtest_main
        Threads::Threads
)

# Test fixtures
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    add_custom_command(TARGET tsdb_stress_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_resource_stress_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_data_volume_stress_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
endif()

# Register stress tests
gtest_discover_tests(tsdb_stress_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 7200  # 2 hour timeout for stress tests
)

gtest_discover_tests(tsdb_resource_stress_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 3600  # 1 hour timeout for resource stress tests
)

gtest_discover_tests(tsdb_data_volume_stress_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 7200  # 2 hour timeout for data volume stress tests
) 