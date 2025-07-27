# Add subdirectories first
add_subdirectory(tsdb)

# Create the main library target
add_library(tsdb_lib)
add_library(tsdb::lib ALIAS tsdb_lib)

# Set include directories
target_include_directories(tsdb_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add sources to main library
target_sources(tsdb_lib PRIVATE
    tsdb/query/query.cpp
)

# Add otel sources only if we have gRPC
if(HAVE_GRPC)
    target_sources(tsdb_lib PRIVATE
        tsdb/otel/bridge.cpp
    )
endif()

# Link with required dependencies
target_link_libraries(tsdb_lib
    PUBLIC
        tsdb::main
        tsdb::storage_impl
        tsdb::histogram_impl
        Threads::Threads
)

# Link optional dependencies
if(HAVE_SPDLOG)
    target_link_libraries(tsdb_lib PUBLIC spdlog::spdlog)
endif()

if(HAVE_GRPC)
    target_link_libraries(tsdb_lib PUBLIC gRPC::grpc++ protobuf::libprotobuf)
endif()

if(HAVE_OTEL)
    target_link_libraries(tsdb_lib PUBLIC ${OPENTELEMETRY_LIBRARIES})
endif()

# Note: These would need to be made optional too if they don't exist
# target_link_libraries(tsdb_lib PUBLIC fmt::fmt absl::flat_hash_map)

# Set library properties
set_target_properties(tsdb_lib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME tsdb
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
) 