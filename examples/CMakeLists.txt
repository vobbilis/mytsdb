add_executable(example_client client.cpp)

target_link_libraries(example_client
    PRIVATE
        OpenTelemetry::api
        OpenTelemetry::sdk
        OpenTelemetry::otlp_grpc_metric_exporter
) 