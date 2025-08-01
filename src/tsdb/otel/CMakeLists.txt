# OpenTelemetry library configuration
add_library(tsdb_otel_impl SHARED)

target_sources(tsdb_otel_impl
    PRIVATE
        bridge_impl.cpp
        bridge_factory.cpp
        metrics_service.cpp
        ../proto/gen/opentelemetry/proto/metrics/v1/metrics.pb.cc
        ../proto/gen/opentelemetry/proto/metrics/v1/metrics.grpc.pb.cc
        ../proto/gen/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.cc
        ../proto/gen/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc
        ../proto/gen/opentelemetry/proto/common/v1/common.pb.cc
        ../proto/gen/opentelemetry/proto/resource/v1/resource.pb.cc
)

target_include_directories(tsdb_otel_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../proto/gen
)

target_link_libraries(tsdb_otel_impl
    PUBLIC
        tsdb_core_impl
        tsdb_storage_impl
)

# Link optional dependencies
if(HAVE_GRPC)
    target_link_libraries(tsdb_otel_impl PUBLIC gRPC::grpc++ protobuf::libprotobuf)
endif()

if(HAVE_SPDLOG)
    target_link_libraries(tsdb_otel_impl PUBLIC spdlog::spdlog)
endif()

# Note: These hardcoded paths would need to be made optional too
# target_link_libraries(tsdb_otel_impl PUBLIC
#     fmt::fmt
#     ${CMAKE_INSTALL_PREFIX}/lib/libopentelemetry_common.dylib
#     ${CMAKE_INSTALL_PREFIX}/lib/libopentelemetry_trace.dylib
#     ${CMAKE_INSTALL_PREFIX}/lib/libopentelemetry_metrics.dylib
#     ${CMAKE_INSTALL_PREFIX}/lib/libopentelemetry_proto.dylib
# )

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