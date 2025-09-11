# ============================================================================
# SEMANTIC VECTOR STORAGE BUILD CONFIGURATION
# ============================================================================
# 
# DESIGN HINTS FROM TASK-6:
# - Support sequential implementation of components
# - Enable parallel compilation for faster builds
# - Include all necessary dependencies from TASK-1, TASK-2, TASK-4
# - Support different build configurations (Debug, Release, RelWithDebInfo)
# - Enable testing for each component
# 
# IMPLEMENTATION GUIDANCE:
# - Build components in dependency order (TASK-11 -> TASK-12 -> TASK-13 -> etc.)
# - Include design hints and performance targets in build configuration
# - Support incremental builds for development efficiency
# - Enable optimization flags for production builds
# 
# INTEGRATION NOTES:
# - Integrates with main TSDB build system
# - Uses unified types and interfaces from TASK-1 and TASK-4
# - Follows configuration patterns from TASK-2
# - Supports AdvancedStorage integration from TASK-5

# ============================================================================
# COMPONENT IMPLEMENTATION FILES (SEQUENTIAL ORDER)
# ============================================================================

# TASK-11: Quantized Vector Index Implementation
set(SEMANTIC_VECTOR_SOURCES
    quantized_vector_index.cpp
    # TODO: TASK-11 - Add quantized vector index implementation
)

# TASK-12: Pruned Semantic Index Implementation  
set(SEMANTIC_VECTOR_SOURCES ${SEMANTIC_VECTOR_SOURCES}
    pruned_semantic_index.cpp
    # TODO: TASK-12 - Add pruned semantic index implementation
)

# TASK-13: Sparse Temporal Graph Implementation
set(SEMANTIC_VECTOR_SOURCES ${SEMANTIC_VECTOR_SOURCES}
    sparse_temporal_graph.cpp
    # TODO: TASK-13 - Add sparse temporal graph implementation
)

# TASK-14: Memory Management Implementation
set(SEMANTIC_VECTOR_SOURCES ${SEMANTIC_VECTOR_SOURCES}
    tiered_memory_manager.cpp
    adaptive_memory_pool.cpp
    # TODO: TASK-14 - Add memory management implementation
)

# TASK-15: Compression Implementation
set(SEMANTIC_VECTOR_SOURCES ${SEMANTIC_VECTOR_SOURCES}
    delta_compressed_vectors.cpp
    dictionary_compressed_metadata.cpp
    # TODO: TASK-15 - Add compression implementation
)

# TASK-16: Analytics Implementation
set(SEMANTIC_VECTOR_SOURCES ${SEMANTIC_VECTOR_SOURCES}
    causal_inference.cpp
    temporal_reasoning.cpp
    # TODO: TASK-16 - Add analytics implementation
)

# TASK-17: Query Processing Implementation
set(SEMANTIC_VECTOR_SOURCES ${SEMANTIC_VECTOR_SOURCES}
    query_processor.cpp
    # TODO: TASK-17 - Add query processing implementation
)

# TASK-18: Migration Implementation
set(SEMANTIC_VECTOR_SOURCES ${SEMANTIC_VECTOR_SOURCES}
    migration_manager.cpp
    # TODO: TASK-18 - Add migration implementation
)

# ============================================================================
# DEPENDENCIES (FROM TASK-1, TASK-2, TASK-4)
# ============================================================================

# Core dependencies from TASK-1 (Unified Types)
find_package(tsdb_core REQUIRED)

# Configuration dependencies from TASK-2 (Unified Configuration)
find_package(tsdb_config REQUIRED)

# Interface dependencies from TASK-4 (Interface Architecture)
find_package(tsdb_storage REQUIRED)

# External dependencies for semantic vector features
find_package(OpenMP REQUIRED)  # For parallel processing
find_package(BLAS REQUIRED)    # For vector operations
find_package(LAPACK REQUIRED)  # For matrix operations

# Optional dependencies for advanced features
find_package(MLPACK QUIET)     # For HNSW and IVF indexing
find_package(FAISS QUIET)      # For vector similarity search
find_package(TORCH QUIET)      # For BERT embeddings

# ============================================================================
# COMPILATION FLAGS AND OPTIMIZATIONS
# ============================================================================

# Performance optimization flags
set(SEMANTIC_VECTOR_CXX_FLAGS
    -O3                    # High optimization
    -march=native         # Use native CPU instructions
    -mtune=native         # Optimize for native CPU
    -ffast-math           # Fast math operations
    -funroll-loops        # Unroll loops for better performance
    -ftree-vectorize      # Enable vectorization
)

# Debug flags for development
set(SEMANTIC_VECTOR_DEBUG_FLAGS
    -g                    # Debug symbols
    -O0                   # No optimization for debugging
    -DDEBUG               # Debug mode
    -fsanitize=address    # Address sanitizer
    -fsanitize=undefined  # Undefined behavior sanitizer
)

# Release flags for production
set(SEMANTIC_VECTOR_RELEASE_FLAGS
    -DNDEBUG              # No debug mode
    -DRELEASE             # Release mode
    -fomit-frame-pointer  # Omit frame pointer for better performance
)

# ============================================================================
# SEMANTIC VECTOR LIBRARY TARGET
# ============================================================================

# Create semantic vector library
add_library(tsdb_semantic_vector STATIC ${SEMANTIC_VECTOR_SOURCES})

# Set target properties
set_target_properties(tsdb_semantic_vector PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Set compilation flags based on build type
target_compile_options(tsdb_semantic_vector PRIVATE
    $<$<CONFIG:Debug>:${SEMANTIC_VECTOR_DEBUG_FLAGS}>
    $<$<CONFIG:Release>:${SEMANTIC_VECTOR_RELEASE_FLAGS} ${SEMANTIC_VECTOR_CXX_FLAGS}>
    $<$<CONFIG:RelWithDebInfo>:${SEMANTIC_VECTOR_RELEASE_FLAGS} ${SEMANTIC_VECTOR_CXX_FLAGS} -g>
)

# Link dependencies
target_link_libraries(tsdb_semantic_vector
    tsdb_core
    tsdb_config
    tsdb_storage
    OpenMP::OpenMP_CXX
    BLAS::BLAS
    LAPACK::LAPACK
)

# Link optional dependencies if found
if(MLPACK_FOUND)
    target_link_libraries(tsdb_semantic_vector mlpack)
    target_compile_definitions(tsdb_semantic_vector PRIVATE USE_MLPACK=1)
endif()

if(FAISS_FOUND)
    target_link_libraries(tsdb_semantic_vector faiss)
    target_compile_definitions(tsdb_semantic_vector PRIVATE USE_FAISS=1)
endif()

if(TORCH_FOUND)
    target_link_libraries(tsdb_semantic_vector torch)
    target_compile_definitions(tsdb_semantic_vector PRIVATE USE_TORCH=1)
endif()

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================

target_include_directories(tsdb_semantic_vector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# ============================================================================
# TESTING CONFIGURATION
# ============================================================================

# Enable testing for each component
enable_testing()

# Test files (to be added as components are implemented)
set(SEMANTIC_VECTOR_TESTS
    # TODO: TASK-11 - Add quantized_vector_index_test.cpp
    # TODO: TASK-12 - Add pruned_semantic_index_test.cpp
    # TODO: TASK-13 - Add sparse_temporal_graph_test.cpp
    # TODO: TASK-14 - Add memory_management_test.cpp
    # TODO: TASK-15 - Add compression_test.cpp
    # TODO: TASK-16 - Add analytics_test.cpp
    # TODO: TASK-17 - Add query_processor_test.cpp
    # TODO: TASK-18 - Add migration_test.cpp
)

# Create test executable
if(SEMANTIC_VECTOR_TESTS)
    add_executable(tsdb_semantic_vector_tests ${SEMANTIC_VECTOR_TESTS})
    target_link_libraries(tsdb_semantic_vector_tests
        tsdb_semantic_vector
        gtest
        gtest_main
    )
    
    # Add tests
    foreach(test_file ${SEMANTIC_VECTOR_TESTS})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_test(NAME ${test_name} COMMAND tsdb_semantic_vector_tests --gtest_filter=${test_name}*)
    endforeach()
endif()

# ============================================================================
# INSTALLATION CONFIGURATION
# ============================================================================

# Install library
install(TARGETS tsdb_semantic_vector
    EXPORT tsdb_semantic_vectorTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/tsdb/storage/semantic_vector/
    DESTINATION include/tsdb/storage/semantic_vector
    FILES_MATCHING PATTERN "*.h"
)

# ============================================================================
# PERFORMANCE TARGETS AND MONITORING
# ============================================================================

# Performance targets from design hints
add_compile_definitions(tsdb_semantic_vector PRIVATE
    VECTOR_SEARCH_LATENCY_TARGET_MS=1
    SEMANTIC_SEARCH_LATENCY_TARGET_MS=5
    CORRELATION_OPERATION_LATENCY_TARGET_MS=5
    MEMORY_REDUCTION_TARGET_PERCENT=80
    ALLOCATION_EFFICIENCY_TARGET_PERCENT=95
    CACHE_HIT_RATIO_TARGET_PERCENT=85
)

# ============================================================================
# BUILD CONFIGURATION NOTES
# ============================================================================

# Sequential Implementation Order:
# 1. TASK-11: Quantized Vector Index (quantized_vector_index.cpp)
# 2. TASK-12: Pruned Semantic Index (pruned_semantic_index.cpp)
# 3. TASK-13: Sparse Temporal Graph (sparse_temporal_graph.cpp)
# 4. TASK-14: Memory Management (tiered_memory_manager.cpp, adaptive_memory_pool.cpp)
# 5. TASK-15: Compression (delta_compressed_vectors.cpp, dictionary_compressed_metadata.cpp)
# 6. TASK-16: Analytics (causal_inference.cpp, temporal_reasoning.cpp)
# 7. TASK-17: Query Processing (query_processor.cpp)
# 8. TASK-18: Migration (migration_manager.cpp)

# Design Hints Compliance:
# - All components use unified types from TASK-1
# - All components follow configuration patterns from TASK-2
# - All components implement interfaces from TASK-4
# - All components integrate with AdvancedStorage from TASK-5
# - All components include design hints from TASK-6

# Performance Targets:
# - Vector search: <1ms per query
# - Semantic search: <5ms per query
# - Memory reduction: 80% with optimization
# - Allocation efficiency: 95%
# - Cache hit ratio: 85% 