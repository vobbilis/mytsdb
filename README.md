# Time Series Database with OpenTelemetry Support

A high-performance time series database designed for metrics storage and querying, with native support for OpenTelemetry metrics ingestion over gRPC.

## Features

- High-performance storage engine with configurable granularity
- Support for OpenTelemetry metrics ingestion over gRPC
- Efficient histogram storage with multiple implementations
- Multi-level storage architecture (hot, warm, cold)
- Automatic data compaction and retention management
- Thread-safe operations with MVCC
- SIMD-optimized operations for histogram processing

## Building

### Prerequisites

- CMake 3.15 or higher
- C++20 compliant compiler
- gRPC and Protocol Buffers
- OpenTelemetry C++ SDK
- fmt
- spdlog
- Abseil

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Run tests
ctest
```

## Usage

### Running the Server

```bash
# Create data directory
mkdir data

# Start the server
./tsdb
```

The server will listen on port 4317 for OpenTelemetry metrics.

### Configuration

The server can be configured through environment variables:

- `TSDB_DATA_DIR`: Data directory (default: "data")
- `TSDB_BLOCK_SIZE`: Block size in bytes (default: 64MB)
- `TSDB_CACHE_SIZE`: Cache size in bytes (default: 1GB)
- `TSDB_COMPACTION_INTERVAL`: Compaction interval in minutes (default: 5)

### OpenTelemetry Integration

The database accepts OpenTelemetry metrics over gRPC on port 4317. It supports:

- Gauge metrics
- Sum metrics
- Histogram metrics

Example using the OpenTelemetry SDK:

```cpp
// Configure OpenTelemetry exporter
opentelemetry::exporter::metrics::OtlpGrpcExporterOptions options;
options.endpoint = "localhost:4317";
auto exporter = opentelemetry::exporter::metrics::OtlpGrpcExporterFactory::Create(options);

// Create meter provider
auto provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
    std::move(exporter));

// Create meter and record metrics
auto meter = provider->GetMeter("example");
auto counter = meter->CreateCounter<int64_t>("example_counter");
counter->Add(1);
```

## Architecture

The database uses a multi-level storage architecture:

1. Hot Storage
   - In-memory storage for recent data
   - Lock-free write path
   - SIMD-optimized operations

2. Warm Storage
   - Memory-mapped files
   - Compressed blocks
   - Quick access to recent historical data

3. Cold Storage
   - Highly compressed historical data
   - Optimized for space efficiency
   - Automatic data retention management

### Histogram Support

Multiple histogram implementations are available:

1. Fixed-bucket Histogram
   - Pre-defined bucket boundaries
   - Constant memory usage
   - Fast updates and queries

2. Exponential Histogram
   - Dynamic bucket boundaries
   - Logarithmic scale
   - Memory-efficient for large ranges

3. DDSketch (coming soon)
   - Relative error guarantees
   - Mergeable sketches
   - Accurate quantile estimation

## Performance

Target performance metrics:

- Write throughput: > 1M samples/sec/core
- Query latency: P99 < 100ms for 1y queries
- Storage efficiency: < 1.5 bytes per sample
- Histogram operations: < 100ns per update

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details. 