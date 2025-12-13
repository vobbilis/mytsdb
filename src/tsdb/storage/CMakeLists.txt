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
        delta_of_delta_encoder.cpp
        atomic_metrics.cpp
        performance_config.cpp
        background_processor.cpp
        lock_free_queue.cpp
        cache_hierarchy.cpp
        memory_mapped_cache.cpp
        predictive_cache.cpp
        atomic_ref_counted.cpp
        storage_impl.cpp
        series.cpp
        storage_factory.cpp
        wal.cpp
        sharded_wal.cpp
        async_wal_shard.cpp
        index.cpp
        sharded_index.cpp
        internal/block_impl.cpp
        parquet/schema_mapper.cpp
        parquet/fingerprint.cpp
        parquet/writer.cpp
        parquet/reader.cpp
        parquet/reader.cpp
        parquet/parquet_block.cpp
        parquet/secondary_index.cpp
        parquet/bloom_filter_manager.cpp
        parquet_catalog.cpp
        rule_manager.cpp
        filtering_storage.cpp
        derived_metrics.cpp
)

# Optional: compile semantic vector sources for early feedback when enabled
if(TSDB_SEMVEC)
    message(STATUS "TSDB_SEMVEC enabled: compiling semantic vector sources for early feedback")
    target_sources(tsdb_storage_impl PRIVATE
        semantic_vector_storage_impl.cpp
        semantic_vector/quantized_vector_index.cpp
        semantic_vector/pruned_semantic_index.cpp
        semantic_vector/sparse_temporal_graph.cpp
        semantic_vector/tiered_memory_manager_stub.cpp
        semantic_vector/adaptive_memory_pool_stub.cpp
        semantic_vector/delta_compressed_vectors.cpp
        semantic_vector/dictionary_compressed_metadata.cpp
        semantic_vector/causal_inference.cpp
        semantic_vector/temporal_reasoning.cpp
        semantic_vector/query_processor.cpp
        semantic_vector/migration_manager.cpp
    )
endif()

# Suppress -Wunused-function for compression.cpp due to persistent issues with tool edits
set_source_files_properties(compression.cpp PROPERTIES COMPILE_FLAGS "-Wno-unused-function")

# Set include directories
target_include_directories(tsdb_storage_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
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
        tsdb::prometheus
        Threads::Threads
)

# Link TBB if available
if(HAVE_TBB)
    target_link_libraries(tsdb_storage_impl PUBLIC TBB::tbb)
endif()

# Abseil (absl) - required by absl::flat_hash_map and friends in the storage layer.
# On some distros, failing to link these yields undefined references at final link.
find_package(absl QUIET)
if(absl_FOUND)
    target_link_libraries(tsdb_storage_impl PUBLIC
        absl::hash
        absl::raw_hash_set
    )
else()
    # Fallback for environments without absl CMake config: link the needed libs directly if present.
    find_library(ABSL_HASH_LIB absl_hash)
    find_library(ABSL_RAW_HASH_SET_LIB absl_raw_hash_set)
    if(ABSL_HASH_LIB AND ABSL_RAW_HASH_SET_LIB)
        target_link_libraries(tsdb_storage_impl PUBLIC
            ${ABSL_HASH_LIB}
            ${ABSL_RAW_HASH_SET_LIB}
        )
    else()
        message(WARNING "Abseil not found via CMake and required libraries not found; build may fail at link time")
    endif()
endif()

# Link Arrow/Parquet if available
if(HAVE_ARROW)
    target_link_libraries(tsdb_storage_impl PUBLIC Arrow::arrow_shared)
endif()
if(HAVE_PARQUET)
    target_link_libraries(tsdb_storage_impl PUBLIC Parquet::parquet_shared)
endif()

# Find and link CRoaring for optimized bitmap operations
find_library(ROARING_LIBRARY roaring HINTS /opt/homebrew/lib)
find_path(ROARING_INCLUDE_DIR roaring/roaring.h HINTS /opt/homebrew/include)
if(ROARING_LIBRARY AND ROARING_INCLUDE_DIR)
    message(STATUS "Found CRoaring: ${ROARING_LIBRARY}")
    target_include_directories(tsdb_storage_impl PUBLIC ${ROARING_INCLUDE_DIR})
    target_link_libraries(tsdb_storage_impl PUBLIC ${ROARING_LIBRARY})
    target_compile_definitions(tsdb_storage_impl PUBLIC HAVE_ROARING=1)
else()
    message(WARNING "CRoaring not found - using std::vector for posting lists (slower)")
endif()

# Install headers
install(FILES
    block_types.h
    block_manager.h
    compression.h
    histogram_ops.h
    object_pool.h
    working_set_cache.h
    delta_of_delta_encoder.h
    atomic_metrics.h
    performance_config.h
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