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
        promql/ast.cpp
        promql/lexer.cpp
        promql/parser.cpp
        promql/engine.cpp
        promql/evaluator.cpp
        promql/functions.cpp
        promql/functions/rate.cpp
        promql/functions/extrapolation.cpp
        promql/functions/aggregation_advanced.cpp
        promql/functions/label_manipulation.cpp
        promql/functions/utility.cpp
        promql/functions/math_advanced.cpp
        promql/functions/remaining_functions.cpp
        promql/query_metrics.cpp
        storage/tsdb_adapter.cpp
        remote/converter.cpp
        remote/write_handler.cpp
        remote/read_handler.cpp
        auth/basic_auth.cpp
        auth/bearer_auth.cpp
        auth/header_auth.cpp
        auth/composite_auth.cpp
)

# Try to find dependencies
find_package(httplib QUIET)
find_package(RapidJSON QUIET)
find_package(Protobuf QUIET)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

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

find_package(Snappy QUIET)
set(HAVE_SNAPPY FALSE)
if(Snappy_FOUND)
    set(HAVE_SNAPPY TRUE)
    message(STATUS "Found Snappy for Prometheus Remote Storage")
endif()

# Include directories
target_include_directories(tsdb_prometheus
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
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
        OpenSSL::SSL
        OpenSSL::Crypto
)

if(HAVE_HTTPLIB)
    target_link_libraries(tsdb_prometheus PRIVATE httplib::httplib)
endif()

if(HAVE_RAPIDJSON)
    target_link_libraries(tsdb_prometheus PRIVATE ${RAPIDJSON_LIBRARIES})
endif()

if(HAVE_PROTOBUF)
    target_link_libraries(tsdb_prometheus PRIVATE protobuf::libprotobuf)
    if(TARGET tsdb_proto)
        target_link_libraries(tsdb_prometheus PRIVATE tsdb_proto)
        target_include_directories(tsdb_prometheus PRIVATE ${CMAKE_BINARY_DIR}/common)
    endif()
endif()

if(HAVE_SNAPPY)
    target_compile_definitions(tsdb_prometheus PRIVATE HAVE_SNAPPY)
    target_link_libraries(tsdb_prometheus PRIVATE Snappy::snappy)
endif()

# Compiler flags
target_compile_features(tsdb_prometheus PUBLIC cxx_std_17)
target_compile_options(tsdb_prometheus PRIVATE
    -Wall
    -Wextra
    # Remove -Werror to allow warnings
    -O2
)

 