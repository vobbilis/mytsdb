find_package(OpenTelemetry QUIET COMPONENTS api sdk otlp_grpc_metric_exporter)

if(OpenTelemetry_FOUND)
    add_executable(example_client client.cpp)

    target_link_libraries(example_client
        PRIVATE
            OpenTelemetry::api
            OpenTelemetry::sdk
            OpenTelemetry::otlp_grpc_metric_exporter
    )
else()
    message(STATUS "OpenTelemetry not found, skipping example_client")
endif()

# Quick start example from README
add_executable(quickstart quickstart.cpp)

target_link_libraries(quickstart
    PRIVATE
        tsdb_lib
)

# Prometheus Remote Storage example
add_subdirectory(prometheus_remote_storage) 