# Prometheus Integration

This directory contains the implementation of Prometheus compatibility layer for the TSDB.

## Directory Structure

```
prometheus/
├── api/              # HTTP API implementation
│   ├── query.h       # Query API handlers
│   ├── read.h        # Remote Read API handlers
│   └── labels.h      # Label API handlers
├── model/            # Data model conversion
│   ├── metric.h      # Metric type conversion
│   ├── label.h       # Label handling
│   └── sample.h      # Sample value conversion
├── promql/           # PromQL implementation
│   ├── parser.h      # Query parser
│   ├── engine.h      # Query engine
│   └── functions.h   # PromQL functions
└── storage/          # Storage adapter
    ├── adapter.h     # Storage interface
    └── index.h       # Label index
```

## Components

### API Layer
- Implements Prometheus HTTP API endpoints
- Handles request parsing and validation
- Manages response formatting
- Implements error handling

### Data Model
- Converts between TSDB and Prometheus data models
- Handles label name and value restrictions
- Manages metric type conversion
- Ensures timestamp compatibility

### PromQL Engine
- Parses PromQL queries
- Executes query evaluation
- Implements PromQL functions
- Manages query optimization

### Storage Adapter
- Provides Prometheus-compatible storage interface
- Implements label indexing
- Manages series lookup
- Handles time range selection

## Building

```bash
mkdir build && cd build
cmake ..
make prometheus
```

## Testing

```bash
# Run Prometheus integration tests
ctest -R "prometheus_.*"

# Run specific test categories
ctest -R "prometheus_api_.*"
ctest -R "prometheus_model_.*"
ctest -R "prometheus_promql_.*"
```

## Configuration

The Prometheus compatibility layer can be configured through the main TSDB configuration file:

```yaml
prometheus:
  listen_address: "0.0.0.0:9090"
  read_timeout: "30s"
  write_timeout: "30s"
  max_connections: 512
  query_timeout: "2m"
  max_samples: 50000000
  max_series: 500000
  cache_size: "1GB"
```

## Usage

1. Start the TSDB with Prometheus compatibility enabled:
   ```bash
   ./tsdb --enable-prometheus
   ```

2. Configure Grafana data source:
   ```yaml
   datasources:
     - name: TSDB
       type: prometheus
       url: http://localhost:9090
   ```

3. Use standard Prometheus queries in Grafana:
   ```
   rate(http_requests_total[5m])
   ```

## Performance Considerations

- Label indexing is crucial for query performance
- Cache frequently accessed time series
- Use appropriate query timeouts
- Monitor memory usage during queries
- Consider query complexity limits

## Security

- Enable TLS for production use
- Configure authentication
- Set appropriate CORS headers
- Implement rate limiting
- Use secure headers

## Monitoring

Monitor the following metrics:
- Query execution time
- Query errors
- Active connections
- Cache hit rate
- Memory usage
- Series cardinality

## Troubleshooting

Common issues and solutions:
1. High query latency
   - Check query complexity
   - Verify index performance
   - Monitor cache hit rates

2. Memory pressure
   - Adjust cache sizes
   - Monitor series cardinality
   - Check query limits

3. Connection issues
   - Verify network settings
   - Check TLS configuration
   - Monitor connection limits 