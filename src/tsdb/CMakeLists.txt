# Add subdirectories
add_subdirectory(core)
add_subdirectory(storage)
add_subdirectory(histogram)
add_subdirectory(query)

# Only add otel if we have gRPC
if(HAVE_GRPC)
    add_subdirectory(otel)
endif()

# Check for dependencies required by Prometheus
find_package(RapidJSON QUIET)
if(RapidJSON_FOUND)
    set(HAVE_RAPIDJSON TRUE)
endif()

find_package(httplib QUIET)
if(httplib_FOUND)
    set(HAVE_HTTPLIB TRUE)
endif()

# Only add prometheus if we have RapidJSON
if(HAVE_RAPIDJSON)
    add_subdirectory(prometheus)
endif()

# Create main TSDB library (interface library since it has no sources, just aggregates dependencies)
add_library(tsdb_main INTERFACE)
add_library(tsdb::main ALIAS tsdb_main)

# Include directories (INTERFACE keyword required for INTERFACE library)
target_include_directories(tsdb_main INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/tsdb>
    $<INSTALL_INTERFACE:include>
)

# Link libraries (INTERFACE keyword required for INTERFACE library)
target_link_libraries(tsdb_main INTERFACE
    tsdb::core_impl
    tsdb::storage_impl
    tsdb::histogram_impl
)

# Only link otel if we have gRPC
if(HAVE_GRPC)
    target_link_libraries(tsdb_main INTERFACE tsdb_otel_impl)
endif()

# Link optional dependencies
if(HAVE_GRPC)
    target_link_libraries(tsdb_main INTERFACE gRPC::grpc++ protobuf::libprotobuf)
endif()

if(ENABLE_OTEL)
    target_link_libraries(tsdb_main INTERFACE
        tsdb::otel
        OpenSSL::SSL
        OpenSSL::Crypto
    )
endif()

# Note: main.cpp is not added to tsdb_main library since it contains main()
# It will be added to the executable target below

# Create server executable
add_executable(tsdb_server)
target_sources(tsdb_server PRIVATE
    main.cpp
)

# Link server executable with required libraries
# Note: We link tsdb::main for its dependencies, but main.cpp is only in the executable
target_link_libraries(tsdb_server PRIVATE
    tsdb::main
    tsdb::storage_impl
    tsdb::core_impl
    tsdb::histogram_impl
    Threads::Threads
)

# Link optional dependencies for server
if(HAVE_SPDLOG)
    target_link_libraries(tsdb_server PRIVATE spdlog::spdlog)
endif()

if(HAVE_TBB)
    target_link_libraries(tsdb_server PRIVATE TBB::tbb)
endif()

# Link gRPC dependencies if available
if(HAVE_GRPC)
    # Define HAVE_GRPC macro for the server
    target_compile_definitions(tsdb_server PRIVATE HAVE_GRPC=1)
    
    # Link OTEL implementation library
    target_link_libraries(tsdb_server PRIVATE 
        tsdb_otel_impl
        gRPC::grpc++ 
        protobuf::libprotobuf
    )
    
    # Also include OTEL proto generated headers
    target_include_directories(tsdb_server PRIVATE
        ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
    )
endif()

# Link Prometheus library if available
if(HAVE_RAPIDJSON)
    target_link_libraries(tsdb_server PRIVATE tsdb_prometheus)
endif()

# Set include directories for server
target_include_directories(tsdb_server PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/proto/gen>
)

# Install library
install(TARGETS tsdb_main
    EXPORT tsdb-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install server executable
install(TARGETS tsdb_server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
) 