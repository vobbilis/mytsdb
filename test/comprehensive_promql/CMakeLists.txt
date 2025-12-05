add_executable(comprehensive_promql_test_harness 
    test_harness.cpp
    selector_tests.cpp
    aggregation_tests.cpp
    function_tests.cpp
    binary_operator_tests.cpp
    subquery_tests.cpp
    complex_scenarios_tests.cpp
)

target_include_directories(comprehensive_promql_test_harness PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(comprehensive_promql_test_harness PRIVATE
    tsdb_lib
    tsdb_prometheus
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
)

# Copy test cases if we have them in a file
# add_custom_command(...)

# Register tests with ctest
include(GoogleTest)
gtest_discover_tests(comprehensive_promql_test_harness
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/test/data"
        TIMEOUT 120
)
