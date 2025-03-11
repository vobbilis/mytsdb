# OpenTelemetry library configuration
add_library(tsdb_otel_impl SHARED)

target_sources(tsdb_otel_impl
    PRIVATE
        bridge_impl.cpp
        bridge_factory.cpp
        metrics_service.cpp
)

target_include_directories(tsdb_otel_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tsdb_otel_impl
    PUBLIC
        tsdb_core_impl
        tsdb_storage_impl
        gRPC::grpc++
        protobuf::libprotobuf
        spdlog::spdlog
        fmt::fmt
        ${CMAKE_INSTALL_PREFIX}/lib/libopentelemetry_common.dylib
        ${CMAKE_INSTALL_PREFIX}/lib/libopentelemetry_trace.dylib
        ${CMAKE_INSTALL_PREFIX}/lib/libopentelemetry_metrics.dylib
        ${CMAKE_INSTALL_PREFIX}/lib/libopentelemetry_proto.dylib
)

set_target_properties(tsdb_otel_impl PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Install library
install(TARGETS tsdb_otel_impl
    EXPORT tsdb-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
) 