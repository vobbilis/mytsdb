# E2E Tests for gRPC Server
# These tests start the actual TSDB gRPC server and test it via real gRPC connections

if(HAVE_GRPC)
    add_executable(tsdb_grpc_server_e2e_test
        grpc_server_e2e_test.cpp
        ${CMAKE_SOURCE_DIR}/test/main.cpp
    )
    
    target_include_directories(tsdb_grpc_server_e2e_test
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
    )
    
    target_link_libraries(tsdb_grpc_server_e2e_test
        PRIVATE
            tsdb_test_common
            tsdb_lib
            GTest::gtest
            GTest::gtest_main
            gRPC::grpc++
            protobuf::libprotobuf
    )
    
    # Discover tests
    gtest_discover_tests(tsdb_grpc_server_e2e_test
        PROPERTIES
            TIMEOUT 60  # 1 minute timeout per test
    )
    
    # Add to test targets
    set(OTEL_GRPC_E2E_TARGETS
        tsdb_grpc_server_e2e_test
        PARENT_SCOPE
    )
endif()

