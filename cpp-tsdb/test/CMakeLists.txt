enable_testing()

# Find GTest package
find_package(GTest REQUIRED)

# Add test executable
add_executable(tsdb_tests
    storage/storage_test.cpp
    promql/engine_test.cpp
    alert/manager_test.cpp
    # Add other test files here
)

# Link dependencies
target_link_libraries(tsdb_tests
    PRIVATE
    GTest::GTest
    GTest::Main
    tsdb_lib
    ${Boost_LIBRARIES}
    ${TBB_LIBRARIES}
)

# Include directories
target_include_directories(tsdb_tests
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS}
)

# Add tests to CTest
add_test(NAME tsdb_tests COMMAND tsdb_tests)

# Set test properties
set_tests_properties(tsdb_tests PROPERTIES
    ENVIRONMENT "TSDB_TEST_DIR=${CMAKE_BINARY_DIR}/test_data"
)

# Add test coverage if enabled
option(ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" FALSE)

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(tsdb_tests PRIVATE --coverage)
        target_link_libraries(tsdb_tests PRIVATE --coverage)
    endif()
endif() 