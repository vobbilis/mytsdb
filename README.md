# MyTSDB - High-Performance Time Series Database

**Version:** 1.0.0  
**Status:** Production-Ready Core, Active Development  
**Language:** C++17  
**License:** MIT

---

## 🎯 Overview

MyTSDB is a high-performance, Prometheus-compatible time series database written in C++17. It features advanced optimizations including object pooling, multi-tier caching, predictive prefetching, and optional semantic vector capabilities for advanced analytics.

### Key Features

- ✅ **Prometheus-Compatible** - Label-based time series identification
- ✅ **PromQL Engine** - 100% complete (All functions, aggregation pushdown, 1000+ tests)
- ✅ **Write-Ahead Log (WAL)** - Crash recovery and durability
- ✅ **Inverted Index** - Fast label-based queries
- ✅ **Multi-Tier Storage** - HOT/WARM/COLD tiers with automatic management
- ✅ **Object Pooling** - 99%+ object reuse for performance
- ✅ **Cache Hierarchy** - L1 (in-memory) + L3 (disk) caching
- ✅ **Compression** - Gorilla, Delta-of-Delta, XOR, RLE algorithms
- ✅ **Predictive Caching** - Pattern-based prefetching
- ✅ **Background Processing** - Async compaction and cleanup
- ✅ **Thread-Safe** - Lock-free data structures with Intel TBB
- ✅ **Aggregation Pushdown** - ~785x speedup for aggregations
- ✅ **OpenTelemetry Support** - Native OTEL metric ingestion bridge
- ✅ **gRPC API** - High-performance query service
- ✅ **Prometheus Remote Storage** - Remote Write/Read API (18 tests passing)
- ✅ **Authentication** - Basic, Bearer, Header, Composite (25 tests passing)
- ⚠️ **Semantic Vectors** - Optional advanced analytics (experimental)

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                       Client API                             │
│  (C++ Library / gRPC Service / OTEL Bridge)                 │
├─────────────────────────────────────────────────────────────┤
│  StorageImpl (Core Storage Engine)                          │
│  ├─ Write-Ahead Log (WAL) - Durability                     │
│  ├─ Index (Inverted + Forward) - Fast Queries              │
│  ├─ Cache Hierarchy (L1/L3) - Performance                  │
│  ├─ Object Pools - Memory Efficiency                       │
│  ├─ Compression - Storage Efficiency                       │
│  └─ Background Processor - Async Operations                │
│  └─ Aggregation Pushdown - Query Optimization              │
├─────────────────────────────────────────────────────────────┤
│  Block Manager (Multi-Tier Storage)                         │
│  ├─ HOT Tier: Recent data, no compression                  │
│  ├─ WARM Tier: Compressed, recent data                     │
│  └─ COLD Tier: Highly compressed, archival                 │
└─────────────────────────────────────────────────────────────┘
```

### Core Components

**Storage Layer:**
- `StorageImpl` - Main storage engine
- `WAL` - Write-ahead logging for crash recovery
- `Index` - Inverted index for label-based queries
- `BlockManager` - Multi-tier block storage

**Performance Layer:**
- `ObjectPool` - Memory-efficient object reuse (TimeSeriesPool, LabelsPool, SamplePool)
- `CacheHierarchy` - Multi-level caching (L1 active, L2 disabled)
- `PredictiveCache` - Pattern-based prefetching
- `BackgroundProcessor` - Async task execution
- `AggregationPushdown` - Storage-side aggregation execution

**Integration Layer:**
- `OTELBridge` - OpenTelemetry metric conversion and ingestion
- `QueryService` - gRPC-based query interface
- `RemoteWriteHandler` - Prometheus Remote Write endpoint (`/api/v1/write`)
- `RemoteReadHandler` - Prometheus Remote Read endpoint (`/api/v1/read`)

**Compression Layer:**
- `GorillaCompressor` - Facebook's Gorilla algorithm for float values
- `DeltaOfDeltaEncoder` - Timestamp compression
- `XORCompressor` - XOR-based delta compression
- `RLECompressor` - Run-length encoding

---

## 🚀 Getting Started

**👉 For detailed step-by-step instructions, see [GETTING_STARTED.md](GETTING_STARTED.md)**

### Quick Start

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/mytsdb.git
cd mytsdb

# 2. Install dependencies (optional - auto-detects OS)
make deps

# 3. Build everything
make

# 4. Run all tests
make test-all
```

That's it! The project uses a Makefile wrapper around CMake for convenience. All operations can be done via Makefile targets.

### Prerequisites

- **Required:** C++17 compiler, CMake 3.15+, Make
- **Optional (but recommended):** Intel TBB, spdlog, Google Test, gRPC (required for OTEL support)

See [GETTING_STARTED.md](GETTING_STARTED.md) for detailed installation instructions for your platform.

### Quick Start

```cpp
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/types.h"

using namespace tsdb;

int main() {
    // Configure storage
    core::StorageConfig config = core::StorageConfig::Default();
    config.data_dir = "./tsdb_data";
    
    // Create storage instance
    storage::StorageImpl storage(config);
    
    // Initialize
    auto init_result = storage.init(config);
    if (!init_result.ok()) {
        std::cerr << "Init failed: " << init_result.error() << std::endl;
        return 1;
    }
    
    // Create time series
    core::Labels labels;
    labels.add("__name__", "cpu_usage");
    labels.add("host", "server1");
    core::TimeSeries series(labels);
    series.add_sample(std::time(nullptr) * 1000, 0.75);
    
    // Write
    auto write_result = storage.write(series);
    if (!write_result.ok()) {
        std::cerr << "Write failed: " << write_result.error() << std::endl;
        return 1;
    }
    
    // Read
    auto read_result = storage.read(labels, 0, std::time(nullptr) * 1000);
    if (read_result.ok()) {
        std::cout << "Read " << read_result.value().size() << " samples" << std::endl;
    }
    
    // Close
    storage.close();
    return 0;
}
```

---

## 📊 Performance

### Legacy Baseline Performance (v1.0)

```
Write Performance:  ~10K ops/sec (single-threaded)
Read Performance:   ~50K ops/sec (cached)
Aggregations:       ~1ms (pushdown) vs ~785ms (raw) -> ~785x Speedup
Compression Ratio:  4-6x (typical)
Object Pool Reuse:  99%+ (measured)
Cache Hit Ratio:    80-90% (hot data)
```

### Memory Usage

```
Base:               ~100MB (empty)
Per Series:         ~1KB (with 100 samples)
Object Pools:       ~50MB (default configuration)
Cache:              ~1GB (configurable)
```

### Optimization Status

- ✅ Object pooling (99%+ reuse)
- ✅ Multi-tier caching
- ✅ Compression (4-6x)
- ✅ Lock-free data structures
- ✅ Aggregation Pushdown (STDDEV, QUANTILE, etc.)
- ✅ gRPC/OTEL Integration
- ✅ **Write Path Optimization** (Batching enabled)

**Performance Update (v1.1):**
- **Throughput:** 260,000 samples/sec (Single Node)
- **Latency:** <3ms (P90)
- **Improvement:** 45% increase in throughput, 2.6x reduction in latency.

---

## ☸️ Kubernetes Deployment

MyTSDB is designed for cloud-native deployment.

### Quick Deploy

```bash
# Deploy to current context
kubectl apply -f deployments/k8s/
```

### Architecture

- **StatefulSet**: Ensures stable network identity and persistent storage.
- **Headless Service**: Direct pod addressing for gRPC/OTEL.
- **PVC**: Persistent Volume Claims for data durability.

See [docs/deployment/KUBERNETES.md](docs/deployment/KUBERNETES.md) (TODO) for full guide.

---

## 🔌 Prometheus Remote Storage

MyTSDB implements the Prometheus Remote Storage API, allowing it to serve as a long-term storage backend for Prometheus and OTEL Collector.

### Quick Start

```bash
# Build and run the example server
make build

# Run without authentication
./build/examples/prometheus_remote_storage/prometheus_remote_storage_server 9090 none

# Run with Basic Auth
./build/examples/prometheus_remote_storage/prometheus_remote_storage_server 9090 basic

# Run with Bearer Token
./build/examples/prometheus_remote_storage/prometheus_remote_storage_server 9090 bearer
```

### Authentication

MyTSDB supports multiple authentication mechanisms:

- **No Auth** - Default, backward compatible
- **Basic Auth** - Username/password (HTTP Basic)
- **Bearer Token** - API tokens/JWT
- **Header-Based** - Multi-tenancy (X-Scope-OrgID)
- **Composite** - Multiple methods (AND/OR logic)

See **[AUTHENTICATION.md](AUTHENTICATION.md)** for complete guide with examples.

### Prometheus Configuration

Add to your `prometheus.yml`:

```yaml
remote_write:
  - url: "http://localhost:9090/api/v1/write"
    # Optional: Enable Snappy compression
    headers:
      Content-Encoding: snappy

remote_read:
  - url: "http://localhost:9090/api/v1/read"
    # Optional: Request Snappy compression
    headers:
      Accept-Encoding: snappy
```

### OTEL Collector Configuration

Add to your OTEL Collector config:

```yaml
exporters:
  prometheusremotewrite:
    endpoint: "http://localhost:9090/api/v1/write"
    # Optional: Add custom headers for multi-tenancy
    headers:
      X-Scope-OrgID: "tenant-1"

service:
  pipelines:
    metrics:
      receivers: [otlp]
      exporters: [prometheusremotewrite]
```

### Features

- ✅ **Prometheus Remote Write** - Ingest metrics from Prometheus
- ✅ **Prometheus Remote Read** - Query historical data
- ✅ **Snappy Compression** - Optional compression for reduced bandwidth
- ✅ **Label Filtering** - Efficient label-based queries
- ✅ **Multi-tenancy Ready** - Custom header support
- ✅ **18 Tests Passing** - Comprehensive unit and integration tests

### Example Server

See [`examples/prometheus_remote_storage/`](examples/prometheus_remote_storage/) for a complete working example.

---

## 📚 Documentation

### Getting Started

- **[GETTING_STARTED.md](GETTING_STARTED.md)** - Complete step-by-step setup guide ⭐ **START HERE**

### Core Documentation

- **[Architecture Overview](docs/architecture/ARCHITECTURE_OVERVIEW.md)** - System architecture
- **[Storage Architecture](docs/architecture/STORAGE_ARCHITECTURE.md)** - Storage system design
- **[Comprehensive Design Study](docs/planning/COMPREHENSIVE_DESIGN_STUDY.md)** - Complete codebase analysis
- **[API Documentation](docs/api/)** - API reference (TODO)

### Planning & Development

- **[Current Status](docs/planning/CURRENT_STATUS.md)** - Current project status
- **[Project State Analysis](docs/planning/PROJECT_STATE_ANALYSIS.md)** - Top-down project analysis
- **[Real Optimization Strategy](docs/planning/REAL_OPTIMIZATION_STRATEGY.md)** - Measurement-driven optimization
- **[Critical Lessons Learned](docs/planning/CRITICAL_LESSONS_LEARNED.md)** - Key learnings from development

### Testing

- **[StorageImpl Comprehensive Test Plan](docs/planning/STORAGEIMPL_COMPREHENSIVE_TEST_PLAN.md)** - Complete test strategy
- **[Integration Testing Plan](docs/planning/INTEGRATION_TESTING_PLAN.md)** - Integration test strategy
- **[Test README](test/README.md)** - Running tests
- **[OTEL/gRPC Tests](test/otel_grpc/README.md)** - OTEL integration tests

### Advanced Features

- **[Semantic Vector Architecture](docs/architecture/SEMANTIC_VECTOR_ARCHITECTURE.md)** - Optional semantic vectors
- **[PromQL Engine](docs/planning/PROMQL_ENGINE_AGENT_TASKS.md)** - Query engine tasks
- **[LSM Implementation Plan](docs/planning/LSM_IMPLEMENTATION_PLAN.md)** - Future LSM storage

---

## 🧪 Testing

### Running Tests

All tests are run via Makefile targets:

```bash
# Run all tests (600+ tests, ~7-8 minutes)
make test-all

# Unit tests only (357+ tests, ~2-3 minutes)
make test-unit

# Integration tests only (177+ tests, ~5-6 minutes)
make test-integration

# Specific test suites
make test-core-unit          # Core unit tests (38 tests)
make test-storage-unit       # Storage unit tests (60 tests)
make test-cache-unit         # Cache unit tests (28 tests)
make test-compression-unit   # Compression unit tests (19 tests)
make test-histogram-unit     # Histogram unit tests (22 tests)

# PromQL Comprehensive Tests (with data generation)
make test-promql-full        # Runs full PromQL suite (900+ tests)

# OTEL/gRPC Tests
make test-otel-grpc          # Runs all OTEL/gRPC tests

# Background test execution (for overnight runs)
make test-background-caffeinate  # macOS - prevents hibernation
make test-background-status      # Check status
make test-background-stop        # Stop tests
```

### Test Status

**Current Status:** 100% pass rate (600+ tests passing)

- ✅ **Unit Tests:** 350+ tests
- ✅ **Integration Tests:** 170+ tests
- ✅ **PromQL Tests:** 50+ comprehensive scenarios
- ✅ **OTEL/gRPC Tests:** Verified
- ✅ **Overall:** All passing

See [docs/planning/FAILING_TESTS_FIX_PLAN.md](docs/planning/FAILING_TESTS_FIX_PLAN.md) for detailed test status and results.

---

## 🔧 Configuration

### Default Configuration

```cpp
Config config = Config::Default();
```

### Production Configuration

```cpp
Config config = Config::Production();
config.mutable_storage().cache_size_bytes = 4LL * 1024 * 1024 * 1024; // 4GB
config.mutable_query().max_concurrent_queries = 500;
```

### High-Performance Configuration

```cpp
Config config = Config::HighPerformance();
config.enable_semantic_vector(); // Enable advanced features
```

### Memory-Efficient Configuration

```cpp
Config config = Config::MemoryEfficient();
config.mutable_storage().cache_size_bytes = 512 * 1024 * 1024; // 512MB
```

### Key Configuration Options

```cpp
StorageConfig storage;
storage.data_dir = "./data";
storage.block_size = 64 * 1024 * 1024;           // 64MB blocks
storage.cache_size_bytes = 1024 * 1024 * 1024;   // 1GB cache
storage.enable_compression = true;

// Object pool configuration
storage.object_pool_config.time_series_max_size = 10000;
storage.object_pool_config.labels_max_size = 20000;
storage.object_pool_config.samples_max_size = 100000;

// Background processing (DISABLED by default for testing)
storage.background_config.enable_background_processing = false;
storage.background_config.enable_auto_compaction = false;
storage.background_config.enable_auto_cleanup = false;
```

---

## 🐛 Known Issues

1. **L2 Cache Disabled** - Memory-mapped cache causes segfaults (under investigation)
2. **Background Processing Disabled** - Disabled by default for testing stability

---

## 🗺️ Roadmap

### Current Focus (v1.0)

- ✅ Core storage engine
- ✅ WAL and crash recovery
- ✅ Index and query support
- ✅ Compression and optimization
- ✅ Basic PromQL support (Phase 1)
- ✅ Performance optimization (Batching enabled)
- ⚠️ Fix L2 cache (in progress)

### Next Release (v1.1)

- 🔲 Enable background processing
- 🔲 Fix L2 cache segfaults
- 🔲 Performance profiling and optimization
- 🔲 Enhanced PromQL support
- 🔲 Distributed sharding (for multi-machine deployment)

### Future (v2.0+)

- 🔲 LSM-tree storage option
- 🔲 Full semantic vector integration
- 🔲 Advanced query optimization
- 🔲 Kubernetes operator
- 🔲 Cloud-native features

---

## 🤝 Contributing

Contributions are welcome! Please read our contributing guidelines (TODO) before submitting pull requests.

### Development Setup

```bash
# Install dependencies (auto-detects OS)
make deps

# Build with all features (tests enabled by default)
make

# Run all tests
make test-all

# Run specific test suite
make test-storage-unit

# Clean and rebuild
make clean-all && make rebuild
```

For more details, see [GETTING_STARTED.md](GETTING_STARTED.md).

---

## 📝 License

MIT License - see [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- **Prometheus** - Inspiration for label-based time series model
- **VictoriaMetrics** - Inspiration for multi-tier storage
- **Facebook Gorilla** - Compression algorithms
- **Intel TBB** - High-performance concurrency primitives
- **Google Test** - Testing framework

---

## 📧 Contact

- **Issues:** [GitHub Issues](https://github.com/yourusername/mytsdb/issues)
- **Discussions:** [GitHub Discussions](https://github.com/yourusername/mytsdb/discussions)
- **Email:** your.email@example.com

---

## 🔗 Related Projects

- [Prometheus](https://prometheus.io/) - Time series monitoring system
- [VictoriaMetrics](https://victoriametrics.com/) - Fast time series database
- [InfluxDB](https://www.influxdata.com/) - Time series platform
- [TimescaleDB](https://www.timescale.com/) - SQL time series database

---

**Built with ❤️ for high-performance time series workloads**
