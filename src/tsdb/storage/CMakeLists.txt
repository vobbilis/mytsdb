# Storage library
add_library(tsdb_storage_impl)
add_library(tsdb::storage_impl ALIAS tsdb_storage_impl)

# Add source files
target_sources(tsdb_storage_impl
    PRIVATE
        block_manager.cpp
        compression.cpp
        histogram_ops.cpp
        object_pool.cpp
        working_set_cache.cpp
        adaptive_compressor.cpp
        delta_of_delta_encoder.cpp
        atomic_metrics.cpp
        performance_config.cpp
        sharded_write_buffer.cpp
        background_processor.cpp
        lock_free_queue.cpp
        cache_hierarchy.cpp
        memory_mapped_cache.cpp
        predictive_cache.cpp
        atomic_ref_counted.cpp
        storage_impl.cpp
        storage_factory.cpp
        internal/block_impl.cpp
)

# Suppress -Wunused-function for compression.cpp due to persistent issues with tool edits
set_source_files_properties(compression.cpp PROPERTIES COMPILE_FLAGS "-Wno-unused-function")

# Set include directories
target_include_directories(tsdb_storage_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set compile features
target_compile_features(tsdb_storage_impl
    PUBLIC
        cxx_std_17
)

# Link dependencies
target_link_libraries(tsdb_storage_impl
    PUBLIC
        tsdb::core_impl
        Threads::Threads
)

# Install headers
install(FILES
    block_types.h
    block_manager.h
    compression.h
    histogram_ops.h
    object_pool.h
    working_set_cache.h
    adaptive_compressor.h
    delta_of_delta_encoder.h
    atomic_metrics.h
    performance_config.h
    sharded_write_buffer.h
    background_processor.h
    lock_free_queue.h
    cache_types.h
    cache_hierarchy.h
    memory_mapped_cache.h
    predictive_cache.h
    atomic_ref_counted.h
    DESTINATION include/tsdb/storage
)

# SIMD configuration
if(ENABLE_SIMD)
    if(COMPILER_SUPPORTS_AVX512)
        target_compile_options(tsdb_storage_impl PRIVATE
            -mavx512f
            -mavx512dq
            -mavx512bw
            -mavx512vl
        )
        target_compile_definitions(tsdb_storage_impl PRIVATE
            TSDB_USE_AVX512
        )
    endif()
endif()

# Install library
install(TARGETS tsdb_storage_impl
    EXPORT tsdb-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
) 