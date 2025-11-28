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
