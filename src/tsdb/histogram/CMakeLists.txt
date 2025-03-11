# Histogram library
add_library(tsdb_histogram_impl)
add_library(tsdb::histogram_impl ALIAS tsdb_histogram_impl)

# Add source files
target_sources(tsdb_histogram_impl
    PRIVATE
        histogram.cpp
)

# Set include directories
target_include_directories(tsdb_histogram_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set compile features
target_compile_features(tsdb_histogram_impl
    PUBLIC
        cxx_std_17
)

# Link dependencies
target_link_libraries(tsdb_histogram_impl
    PUBLIC
        tsdb::core_impl
        Threads::Threads
)

# Install headers
install(FILES
    histogram.h
    DESTINATION include/tsdb/histogram
)

# Install library
install(TARGETS tsdb_histogram_impl
    EXPORT tsdb-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
) 