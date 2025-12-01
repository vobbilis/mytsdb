cmake_minimum_required(VERSION 3.15)

if(HAVE_GTEST AND HAVE_PROTOBUF)
    # Prometheus Remote Storage Converter Tests
    add_executable(test_prometheus_remote_converter
        test_converter.cpp
    )
    
    target_link_libraries(test_prometheus_remote_converter
        PRIVATE
            tsdb::prometheus
            tsdb_proto
            GTest::gtest
            GTest::gtest_main
    )
    
    target_include_directories(test_prometheus_remote_converter
        PRIVATE
            ${CMAKE_BINARY_DIR}/common
    )
    
    target_compile_features(test_prometheus_remote_converter PRIVATE cxx_std_17)
    
    gtest_discover_tests(test_prometheus_remote_converter)
    
    message(STATUS "Prometheus Remote Storage converter tests enabled")
else()
    message(STATUS "Prometheus Remote Storage tests disabled (missing GTest or Protobuf)")
endif()
