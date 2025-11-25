add_executable(tsdb_otel_grpc_full_path_test test_grpc_full_path.cpp)

target_compile_definitions(tsdb_otel_grpc_full_path_test PRIVATE
    TSDB_SERVER_PATH="${CMAKE_BINARY_DIR}/src/tsdb/tsdb_server"
)

target_include_directories(tsdb_otel_grpc_full_path_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(tsdb_otel_grpc_full_path_test PRIVATE
    tsdb_test_common
    tsdb_lib
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
)

gtest_discover_tests(tsdb_otel_grpc_full_path_test)

add_executable(tsdb_otel_benchmark_verify_test test_benchmark_verification_path.cpp)

target_compile_definitions(tsdb_otel_benchmark_verify_test PRIVATE
    TSDB_SERVER_PATH="${CMAKE_BINARY_DIR}/src/tsdb/tsdb_server"
)

target_include_directories(tsdb_otel_benchmark_verify_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(tsdb_otel_benchmark_verify_test PRIVATE
    tsdb_test_common
    tsdb_lib
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
)

gtest_discover_tests(tsdb_otel_benchmark_verify_test)

add_executable(tsdb_otel_verification_debug_test test_verification_debug.cpp)

target_compile_definitions(tsdb_otel_verification_debug_test PRIVATE
    TSDB_SERVER_PATH="${CMAKE_BINARY_DIR}/src/tsdb/tsdb_server"
)

target_include_directories(tsdb_otel_verification_debug_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(tsdb_otel_verification_debug_test PRIVATE
    tsdb_test_common
    tsdb_lib
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
)

gtest_discover_tests(tsdb_otel_verification_debug_test)
