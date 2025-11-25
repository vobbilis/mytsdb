# Test: Protobuf attribute preservation
add_executable(tsdb_otel_attributes_test test_attributes_preservation.cpp)

target_include_directories(tsdb_otel_attributes_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(tsdb_otel_attributes_test PRIVATE
    tsdb_test_common
    GTest::gtest
    GTest::gtest_main
    protobuf::libprotobuf
)

gtest_discover_tests(tsdb_otel_attributes_test)

# Test: Bridge conversion
add_executable(tsdb_otel_bridge_conversion_test test_bridge_conversion.cpp)

target_include_directories(tsdb_otel_bridge_conversion_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(tsdb_otel_bridge_conversion_test PRIVATE
    tsdb_test_common
    tsdb_lib
    GTest::gtest
    GTest::gtest_main
    protobuf::libprotobuf
)

gtest_discover_tests(tsdb_otel_bridge_conversion_test)

# Test: Simulate exact gRPC path
add_executable(tsdb_otel_grpc_path_test test_grpc_path.cpp)

target_include_directories(tsdb_otel_grpc_path_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(tsdb_otel_grpc_path_test PRIVATE
    tsdb_test_common
    tsdb_lib
    GTest::gtest
    GTest::gtest_main
    protobuf::libprotobuf
)

gtest_discover_tests(tsdb_otel_grpc_path_test)

# Test: QueryService unit test
add_executable(tsdb_otel_query_service_test test_query_service.cpp)

target_include_directories(tsdb_otel_query_service_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(tsdb_otel_query_service_test PRIVATE
    tsdb_test_common
    tsdb_lib
    tsdb_otel_impl
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
)

gtest_discover_tests(tsdb_otel_query_service_test)
