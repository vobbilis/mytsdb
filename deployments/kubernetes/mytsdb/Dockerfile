# Multi-stage build for MyTSDB Prometheus Remote Storage Server
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libsnappy-dev \
    libtbb-dev \
    libspdlog-dev \
    rapidjson-dev \
    libarrow-dev \
    libarrow-flight-dev \
    libparquet-dev \
    curl \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install httplib (header-only)
RUN curl -L -o /usr/include/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.14.3/httplib.h

# Set working directory
WORKDIR /build

# Copy source code
COPY . .

# Build the project
# Build the project
# Regenerate protobuf files to ensure compatibility with system libprotobuf
# Regenerate protobuf files to ensure compatibility with system libprotobuf
RUN which grpc_cpp_plugin || echo "grpc_cpp_plugin not found in PATH" && \
    find /usr -name grpc_cpp_plugin && \
    mkdir -p src/tsdb/proto/gen && \
    PLUGIN_PATH=$(which grpc_cpp_plugin) && \
    echo "Using gRPC plugin at: $PLUGIN_PATH" && \
    protoc --cpp_out=src/tsdb/proto/gen --grpc_out=src/tsdb/proto/gen --plugin=protoc-gen-grpc=$PLUGIN_PATH --experimental_allow_proto3_optional -I src/tsdb/proto src/tsdb/proto/tsdb.proto && \
    protoc --cpp_out=src/tsdb/proto/gen --grpc_out=src/tsdb/proto/gen --plugin=protoc-gen-grpc=$PLUGIN_PATH --experimental_allow_proto3_optional -I otel-proto otel-proto/opentelemetry/proto/common/v1/common.proto otel-proto/opentelemetry/proto/resource/v1/resource.proto otel-proto/opentelemetry/proto/metrics/v1/metrics.proto otel-proto/opentelemetry/proto/collector/metrics/v1/metrics_service.proto

# Build the project
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=ON -DHAVE_GRPC=ON -DENABLE_OTEL=ON -DENABLE_PARQUET=ON && \
    make tsdb_server synthetic_cluster_generator k8s_data_generator -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libprotobuf23 \
    libgrpc++-dev \
    libsnappy1v5 \
    libtbb12 \
    libspdlog1 \
    libarrow-dev \
    libarrow-flight-dev \
    libparquet-dev \
    && rm -rf /var/lib/apt/lists/*

# Create app user
RUN useradd -m -u 1000 mytsdb

# Create data directory
RUN mkdir -p /data && chown mytsdb:mytsdb /data

# Copy binary from builder
COPY --from=builder /build/build/src/tsdb/tsdb_server /usr/local/bin/
COPY --from=builder /build/build/tools/data_gen/synthetic_cluster_generator /usr/local/bin/
COPY --from=builder /build/build/tools/k8s_data_generator /usr/local/bin/
COPY --from=builder /build/build/src/tsdb/core/libtsdb_core_impl.so /usr/local/lib/
COPY --from=builder /build/build/src/tsdb/histogram/libtsdb_histogram_impl.so /usr/local/lib/
COPY --from=builder /build/build/src/tsdb/storage/libtsdb_storage_impl.so /usr/local/lib/
COPY --from=builder /build/build/src/libtsdb_lib.so /usr/local/lib/
COPY --from=builder /build/build/src/tsdb/prometheus/libtsdb_prometheus.so /usr/local/lib/
COPY --from=builder /build/build/src/tsdb/otel/libtsdb_otel_impl.so /usr/local/lib/

# Update shared library cache
RUN ldconfig

# Set user
USER mytsdb

# Set working directory
WORKDIR /data

# Expose ports (HTTP and gRPC)
EXPOSE 9090 4317 8815

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:9090/health || exit 1

# Run server
ENTRYPOINT ["/usr/local/bin/tsdb_server"]
CMD ["--address", "0.0.0.0:4317", "--http-port", "9090", "--arrow-port", "8815", "--data-dir", "/data"]
