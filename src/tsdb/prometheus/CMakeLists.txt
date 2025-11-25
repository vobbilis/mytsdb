cmake_minimum_required(VERSION 3.10)

# Prometheus integration library
add_library(tsdb_prometheus)
add_library(tsdb::prometheus ALIAS tsdb_prometheus)

# Add source files
target_sources(tsdb_prometheus
    PRIVATE
        api/labels.cpp
        api/query_handler.cpp
        model/types.cpp
        server/http_server.cpp
)

# Try to find dependencies
find_package(httplib QUIET)
find_package(RapidJSON QUIET)
find_package(Protobuf QUIET)
find_package(Threads REQUIRED)

set(HAVE_HTTPLIB FALSE)
set(HAVE_RAPIDJSON FALSE)
set(HAVE_PROTOBUF FALSE)

if(httplib_FOUND)
    set(HAVE_HTTPLIB TRUE)
    message(STATUS "Found httplib")
endif()

if(RapidJSON_FOUND)
    set(HAVE_RAPIDJSON TRUE CACHE BOOL "RapidJSON found" FORCE)
    message(STATUS "Found RapidJSON")
endif()

if(Protobuf_FOUND)
    set(HAVE_PROTOBUF TRUE)
    message(STATUS "Found Protobuf")
endif()

# Include directories
target_include_directories(tsdb_prometheus
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(HAVE_HTTPLIB)
    target_include_directories(tsdb_prometheus PRIVATE ${HTTPLIB_INCLUDE_DIRS})
endif()

if(HAVE_RAPIDJSON)
    target_include_directories(tsdb_prometheus PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
endif()

if(HAVE_PROTOBUF)
    target_include_directories(tsdb_prometheus PRIVATE ${PROTOBUF_INCLUDE_DIRS})
endif()

# Link libraries
target_link_libraries(tsdb_prometheus
    PUBLIC
        tsdb::lib
    PRIVATE
        Threads::Threads
)

if(HAVE_HTTPLIB)
    target_link_libraries(tsdb_prometheus PRIVATE httplib::httplib)
endif()

if(HAVE_RAPIDJSON)
    target_link_libraries(tsdb_prometheus PRIVATE ${RAPIDJSON_LIBRARIES})
endif()

if(HAVE_PROTOBUF)
    target_link_libraries(tsdb_prometheus PRIVATE protobuf::libprotobuf)
endif()

# Compiler flags
target_compile_features(tsdb_prometheus PUBLIC cxx_std_17)
target_compile_options(tsdb_prometheus PRIVATE
    -Wall
    -Wextra
    # Remove -Werror to allow warnings
    -O2
)

if(BUILD_TESTS AND HAVE_GTEST AND HAVE_RAPIDJSON)
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
            Threads::Threads
    )
    
    if(HAVE_HTTPLIB)
        target_link_libraries(prometheus_tests PRIVATE httplib::httplib)
    endif()
    
    if(HAVE_RAPIDJSON)
        target_link_libraries(prometheus_tests PRIVATE ${RAPIDJSON_LIBRARIES})
    endif()

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