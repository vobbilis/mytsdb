# Scalability Testing Configuration

# Scalability test executables
add_executable(tsdb_scalability_tests
    horizontal_scalability_test.cpp
    vertical_scalability_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_scalability_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

    target_link_libraries(tsdb_scalability_tests
        PRIVATE
            tsdb_test_common
            tsdb_lib
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
    )

# Individual scalability test executables
add_executable(tsdb_horizontal_scalability_test
    horizontal_scalability_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_horizontal_scalability_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

    target_link_libraries(tsdb_horizontal_scalability_test
        PRIVATE
            tsdb_test_common
            tsdb_lib
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
    )

add_executable(tsdb_vertical_scalability_test
    vertical_scalability_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

target_include_directories(tsdb_vertical_scalability_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

    target_link_libraries(tsdb_vertical_scalability_test
        PRIVATE
            tsdb_test_common
            tsdb_lib
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
    )

# Test fixtures
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    add_custom_command(TARGET tsdb_scalability_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_horizontal_scalability_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_vertical_scalability_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
endif()

# Register scalability tests
gtest_discover_tests(tsdb_scalability_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 3600  # 1 hour timeout for scalability tests
)

gtest_discover_tests(tsdb_horizontal_scalability_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 3600  # 1 hour timeout for horizontal scalability tests
)

gtest_discover_tests(tsdb_vertical_scalability_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        TIMEOUT 3600  # 1 hour timeout for vertical scalability tests
) 