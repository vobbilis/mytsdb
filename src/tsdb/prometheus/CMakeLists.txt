cmake_minimum_required(VERSION 3.10)

# Prometheus integration library
add_library(tsdb_prometheus)
add_library(tsdb::prometheus ALIAS tsdb_prometheus)

# Add source files
target_sources(tsdb_prometheus
    PRIVATE
        api/labels.cpp
        model/types.cpp
        server/http_server.cpp
)

# Dependencies
find_package(httplib REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(Protobuf REQUIRED)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Include directories
target_include_directories(tsdb_prometheus
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${HTTPLIB_INCLUDE_DIRS}
        ${RAPIDJSON_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(tsdb_prometheus
    PUBLIC
        tsdb::lib
    PRIVATE
        httplib::httplib
        ${RAPIDJSON_LIBRARIES}
        protobuf::libprotobuf
)

# Compiler flags
target_compile_features(tsdb_prometheus PUBLIC cxx_std_17)
target_compile_options(tsdb_prometheus PRIVATE
    -Wall
    -Wextra
    -Werror
    -O2
)

if(BUILD_TESTS)
    # Add test executable
    add_executable(prometheus_tests
        tests/types_test.cpp
        tests/http_server_test.cpp
        tests/labels_test.cpp
    )

    # Test include directories
    target_include_directories(prometheus_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${RAPIDJSON_INCLUDE_DIRS}
    )

    # Test dependencies
    target_link_libraries(prometheus_tests
        PRIVATE
            tsdb_prometheus
            GTest::GTest
            GTest::Main
            httplib::httplib
            ${RAPIDJSON_LIBRARIES}
            Threads::Threads
    )

    # Add tests
    add_test(NAME prometheus_model_types 
             COMMAND prometheus_tests --gtest_filter="TypesTest.*")
    add_test(NAME prometheus_http_server 
             COMMAND prometheus_tests --gtest_filter="HttpServerTest.*")
    add_test(NAME prometheus_labels_api
             COMMAND prometheus_tests --gtest_filter="LabelsTest.*")

    # Test configuration
    set_tests_properties(prometheus_model_types PROPERTIES
        ENVIRONMENT "GTEST_COLOR=1"
        TIMEOUT 30
    )
    set_tests_properties(prometheus_http_server PROPERTIES
        ENVIRONMENT "GTEST_COLOR=1"
        TIMEOUT 60
    )
    set_tests_properties(prometheus_labels_api PROPERTIES
        ENVIRONMENT "GTEST_COLOR=1"
        TIMEOUT 30
    )
endif() 