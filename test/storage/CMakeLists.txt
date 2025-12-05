# CMakeLists.txt for test/storage - Orphaned storage tests
# These tests were not integrated into the main test suite

add_executable(tsdb_storage_orphan_tests
    async_wal_test.cpp
    sharded_wal_test.cpp
    sharded_index_test.cpp
    cache_hierarchy_repro_test.cpp
)

target_include_directories(tsdb_storage_orphan_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/..  # for test_config.h
)

target_link_libraries(tsdb_storage_orphan_tests PRIVATE
    tsdb_lib
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

# Set test data directory
set_target_properties(tsdb_storage_orphan_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test/storage
)

# Register tests with ctest
include(GoogleTest)
gtest_discover_tests(tsdb_storage_orphan_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test/data"
        TIMEOUT 60
)
