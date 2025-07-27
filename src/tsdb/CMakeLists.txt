# Add subdirectories
add_subdirectory(core)
add_subdirectory(storage)
add_subdirectory(histogram)
add_subdirectory(query)

# Only add otel if we have gRPC
if(HAVE_GRPC)
    add_subdirectory(otel)
endif()

# Only add prometheus if we have RapidJSON
if(HAVE_RAPIDJSON)
    add_subdirectory(prometheus)
endif()

# Create main TSDB library
add_library(tsdb_main)
add_library(tsdb::main ALIAS tsdb_main)

# Include directories
target_include_directories(tsdb_main PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(tsdb_main PUBLIC
    tsdb::core_impl
    tsdb::storage_impl
    tsdb::histogram_impl
)

# Only link otel if we have gRPC
if(HAVE_GRPC)
    target_link_libraries(tsdb_main PUBLIC tsdb_otel_impl)
endif()

# Link optional dependencies
if(HAVE_GRPC)
    target_link_libraries(tsdb_main PUBLIC gRPC::grpc++ protobuf::libprotobuf)
endif()

if(ENABLE_OTEL)
    target_link_libraries(tsdb_main PUBLIC
        tsdb::otel
        OpenSSL::SSL
        OpenSSL::Crypto
    )
endif()

# Add sources
target_sources(tsdb_main PRIVATE
    main.cpp
)

# Install library
install(TARGETS tsdb_main
    EXPORT tsdb-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
) 