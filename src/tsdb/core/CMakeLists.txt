# Core library configuration
# Sources are added in parent CMakeLists.txt 

add_library(tsdb_core_impl)
add_library(tsdb::core_impl ALIAS tsdb_core_impl)

# Add source files
target_sources(tsdb_core_impl
    PRIVATE
        core.cpp
        internal/types.cpp
)

# Set include directories
target_include_directories(tsdb_core_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set compile features
target_compile_features(tsdb_core_impl
    PUBLIC
        cxx_std_17
)

# Link dependencies
target_link_libraries(tsdb_core_impl
    PUBLIC
        Threads::Threads
)

# Link optional dependencies
if(HAVE_SPDLOG)
    target_link_libraries(tsdb_core_impl PUBLIC spdlog::spdlog)
endif()

# Install headers
install(FILES
    types.h
    result.h
    interfaces.h
    DESTINATION include/tsdb/core
)

# Install library
install(TARGETS tsdb_core_impl
    EXPORT tsdb-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
) 