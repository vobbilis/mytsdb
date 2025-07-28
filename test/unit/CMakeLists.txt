# Unit test executables
add_executable(tsdb_core_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/core/result_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/core/error_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/core/config_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Storage unit tests
add_executable(tsdb_storage_unit_tests
    ${CMAKE_SOURCE_DIR}/test/main.cpp
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/storage_test.cpp
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/block_management_test.cpp
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/compression_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/storage/object_pool_test.cpp
    ${CMAKE_SOURCE_DIR}/test/unit/storage/working_set_cache_test.cpp
)

add_executable(tsdb_histogram_unit_tests
    ${CMAKE_SOURCE_DIR}/test/histogram/ddsketch_test.cpp
    ${CMAKE_SOURCE_DIR}/test/histogram/fixed_bucket_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Object pool unit tests
add_executable(tsdb_object_pool_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/object_pool_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Working set cache unit tests
add_executable(tsdb_working_set_cache_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/working_set_cache_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Adaptive compressor unit tests
add_executable(tsdb_adaptive_compressor_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/adaptive_compressor_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Delta-of-delta encoder unit tests
add_executable(tsdb_delta_of_delta_encoder_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/delta_of_delta_encoder_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Atomic metrics unit tests
add_executable(tsdb_atomic_metrics_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/atomic_metrics_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Performance config unit tests
add_executable(tsdb_performance_config_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/performance_config_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Sharded write buffer unit tests
add_executable(tsdb_sharded_write_buffer_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/sharded_write_buffer_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Background processor unit tests
add_executable(tsdb_background_processor_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/background_processor_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Lock-free queue unit tests
add_executable(tsdb_lock_free_queue_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/lock_free_queue_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Cache hierarchy unit tests
add_executable(tsdb_cache_hierarchy_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/cache_hierarchy_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Predictive cache unit tests
add_executable(tsdb_predictive_cache_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/predictive_cache_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Atomic reference counted unit tests
add_executable(tsdb_atomic_ref_counted_unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/storage/atomic_ref_counted_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

add_executable(tsdb_integration_tests
    ${CMAKE_SOURCE_DIR}/test/tsdb/storage/storage_test.cpp
    ${CMAKE_SOURCE_DIR}/test/main.cpp
)

# Core unit tests configuration
target_include_directories(tsdb_core_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_core_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_core_impl
)

# Storage unit tests configuration
target_include_directories(tsdb_storage_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_storage_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Histogram unit tests configuration
target_include_directories(tsdb_histogram_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_histogram_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_histogram_impl
)

# Object pool unit tests configuration
target_include_directories(tsdb_object_pool_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_object_pool_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

target_link_libraries(tsdb_working_set_cache_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Adaptive compressor unit tests configuration
target_include_directories(tsdb_adaptive_compressor_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_adaptive_compressor_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Delta-of-delta encoder unit tests configuration
target_include_directories(tsdb_delta_of_delta_encoder_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_delta_of_delta_encoder_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Atomic metrics unit tests configuration
target_include_directories(tsdb_atomic_metrics_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_atomic_metrics_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Performance config unit tests configuration
target_include_directories(tsdb_performance_config_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_performance_config_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Sharded write buffer unit tests configuration
target_include_directories(tsdb_sharded_write_buffer_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_sharded_write_buffer_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Background processor unit tests configuration
target_include_directories(tsdb_background_processor_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_background_processor_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Lock-free queue unit tests configuration
target_include_directories(tsdb_lock_free_queue_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_lock_free_queue_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Cache hierarchy unit tests configuration
target_include_directories(tsdb_cache_hierarchy_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_cache_hierarchy_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Predictive cache unit tests configuration
target_include_directories(tsdb_predictive_cache_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_predictive_cache_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Atomic reference counted unit tests configuration
target_include_directories(tsdb_atomic_ref_counted_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_atomic_ref_counted_unit_tests
    PRIVATE
        tsdb_test_common
        tsdb_storage_impl
        tsdb_core_impl
)

# Integration tests configuration
target_include_directories(tsdb_integration_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tsdb_integration_tests
    PRIVATE
        tsdb_test_common
        tsdb_lib
)

# Test fixtures
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../data)
    add_custom_command(TARGET tsdb_core_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_storage_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_integration_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_histogram_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_object_pool_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_working_set_cache_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_adaptive_compressor_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_delta_of_delta_encoder_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_atomic_metrics_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_performance_config_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_sharded_write_buffer_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_background_processor_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_lock_free_queue_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_cache_hierarchy_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_predictive_cache_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
    add_custom_command(TARGET tsdb_atomic_ref_counted_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_CURRENT_BINARY_DIR}/data
    )
endif()

# Register tests
gtest_discover_tests(tsdb_core_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_storage_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_histogram_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_object_pool_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_working_set_cache_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_adaptive_compressor_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_delta_of_delta_encoder_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_atomic_metrics_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_performance_config_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_sharded_write_buffer_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_background_processor_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_lock_free_queue_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_cache_hierarchy_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_predictive_cache_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
)

gtest_discover_tests(tsdb_atomic_ref_counted_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        ENVIRONMENT "TSDB_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
) 