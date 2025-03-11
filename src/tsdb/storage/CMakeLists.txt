# Storage library
add_library(tsdb_storage_impl)
add_library(tsdb::storage_impl ALIAS tsdb_storage_impl)

# Add source files
target_sources(tsdb_storage_impl
    PRIVATE
        block_manager.cpp
        compression.cpp
        histogram_ops.cpp
        storage_impl.cpp
)

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