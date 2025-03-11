enable_testing()

# Test executable
add_executable(prometheus_tests
    types_test.cpp
    http_server_test.cpp
    labels_test.cpp
)

# Link test dependencies
target_link_libraries(prometheus_tests
    tsdb_prometheus
    GTest::GTest
    GTest::Main
    httplib::httplib
    RapidJSON::RapidJSON
    Threads::Threads
)

# Register tests with result output
add_test(NAME prometheus_model_types 
         COMMAND prometheus_tests 
         --gtest_filter="TypesTest.*"
         --gtest_output=xml:${CMAKE_BINARY_DIR}/test_results/types_test.xml
         --gtest_output=json:${CMAKE_BINARY_DIR}/test_results/types_test.json)

add_test(NAME prometheus_http_server 
         COMMAND prometheus_tests 
         --gtest_filter="HttpServerTest.*"
         --gtest_output=xml:${CMAKE_BINARY_DIR}/test_results/http_server_test.xml
         --gtest_output=json:${CMAKE_BINARY_DIR}/test_results/http_server_test.json)

add_test(NAME prometheus_labels_api
         COMMAND prometheus_tests 
         --gtest_filter="LabelsTest.*"
         --gtest_output=xml:${CMAKE_BINARY_DIR}/test_results/labels_test.xml
         --gtest_output=json:${CMAKE_BINARY_DIR}/test_results/labels_test.json)

# Test configuration
set_tests_properties(prometheus_model_types PROPERTIES
    ENVIRONMENT "GTEST_COLOR=1"
    TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    FIXTURES_REQUIRED test_env
)

set_tests_properties(prometheus_http_server PROPERTIES
    ENVIRONMENT "GTEST_COLOR=1"
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    FIXTURES_REQUIRED test_env
)

set_tests_properties(prometheus_labels_api PROPERTIES
    ENVIRONMENT "GTEST_COLOR=1"
    TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    FIXTURES_REQUIRED test_env
)

# Create test environment
add_test(NAME setup_test_env
         COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test_results/logs
)
set_tests_properties(setup_test_env PROPERTIES
    FIXTURES_SETUP test_env
)

# Generate test summary
add_custom_command(TARGET prometheus_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Generating test summary..."
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/test_results/*.xml > ${CMAKE_BINARY_DIR}/test_results/all_tests.xml
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_summary.cmake
) 