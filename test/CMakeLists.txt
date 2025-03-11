include(GoogleTest)
enable_testing()

# Common test settings
add_library(tsdb_test_common INTERFACE)
target_include_directories(tsdb_test_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(tsdb_test_common INTERFACE
    GTest::GTest
    GTest::Main
    tsdb_lib
)

# Test data configuration
set(TSDB_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/test_config.h
)

# Unit tests - temporarily disabled
# add_subdirectory(unit)

# Integration tests - temporarily disabled
# add_subdirectory(integration) 