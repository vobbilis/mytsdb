# CMakeLists.txt for test/histogram - Orphaned histogram tests

add_executable(tsdb_histogram_orphan_tests
    ddsketch_test.cpp
    fixed_bucket_test.cpp
)

target_include_directories(tsdb_histogram_orphan_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/..  # for test_config.h
)

target_link_libraries(tsdb_histogram_orphan_tests PRIVATE
    tsdb_lib
    GTest::gtest
    GTest::gtest_main
)

set_target_properties(tsdb_histogram_orphan_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test/histogram
)

include(GoogleTest)
gtest_discover_tests(tsdb_histogram_orphan_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test/data"
        TIMEOUT 30
)
