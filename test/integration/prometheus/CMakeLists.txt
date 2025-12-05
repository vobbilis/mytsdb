cmake_minimum_required(VERSION 3.15)

# Prometheus Remote Storage Integration Test
add_executable(tsdb_prometheus_remote_storage_integration_test
    prometheus_remote_storage_test.cpp
)

target_link_libraries(tsdb_prometheus_remote_storage_integration_test
    PRIVATE
        tsdb::lib
        tsdb::prometheus
        tsdb_proto
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(tsdb_prometheus_remote_storage_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/common
)

target_compile_features(tsdb_prometheus_remote_storage_integration_test PRIVATE cxx_std_17)

# gtest_discover_tests(tsdb_prometheus_remote_storage_integration_test
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
# )

message(STATUS "Prometheus Remote Storage integration test enabled")

