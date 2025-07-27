include(GoogleTest)
enable_testing()

# Common test settings
add_library(tsdb_test_common INTERFACE)
target_include_directories(tsdb_test_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR} # for test_config.h.in resolution by tests
    ${CMAKE_CURRENT_BINARY_DIR} # for generated test_config.h
    ${CMAKE_SOURCE_DIR}/include # to find tsdb headers from tests
    ${CMAKE_SOURCE_DIR}/src # Potentially too broad, let specific targets add what they need from src
)
target_link_libraries(tsdb_test_common INTERFACE
    GTest::GTest
    GTest::Main
    tsdb_lib       # Link against the main library
)

# Test data configuration
set(TSDB_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/test_config.h
)

# Unit tests
add_subdirectory(unit)

# Integration tests
add_subdirectory(integration)

# Performance benchmark tests
add_subdirectory(benchmark)

# Load testing
add_subdirectory(load)

# Stress testing
add_subdirectory(stress)

# Scalability testing
add_subdirectory(scalability)

# PromQL Parser Tests - Direct build without storage dependency
add_executable(promql_parser_tests
    ${CMAKE_SOURCE_DIR}/src/tsdb/prometheus/tests/promql_parser_test.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/prometheus/promql/lexer.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/prometheus/promql/ast.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/prometheus/promql/parser.cpp
    ${CMAKE_SOURCE_DIR}/src/tsdb/prometheus/model/types.cpp
)

target_include_directories(promql_parser_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/prometheus/promql
    ${CMAKE_SOURCE_DIR}/src/tsdb/prometheus/model
)

target_link_libraries(promql_parser_tests PRIVATE
    GTest::GTest
    GTest::Main
)

add_test(NAME PromQLParserTests COMMAND promql_parser_tests)
