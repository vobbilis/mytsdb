# Prometheus Remote Storage Example

This example demonstrates how to use MyTSDB as a Prometheus Remote Storage backend.

## Building

```bash
cd /path/to/mytsdb
make
```

The example server will be built as `build/examples/prometheus_remote_storage/prometheus_remote_storage_server`.

## Running

```bash
./build/examples/prometheus_remote_storage/prometheus_remote_storage_server [port]
```

Default port is 9090.

## Prometheus Configuration

Add the following to your `prometheus.yml`:

```yaml
remote_write:
  - url: "http://localhost:9090/api/v1/write"
    # Optional: Add Snappy compression
    # headers:
    #   Content-Encoding: snappy

remote_read:
  - url: "http://localhost:9090/api/v1/read"
    # Optional: Request Snappy compression
    # headers:
    #   Accept-Encoding: snappy
```

## OTEL Collector Configuration

Add the following to your OTEL Collector config:

```yaml
exporters:
  prometheusremotewrite:
    endpoint: "http://localhost:9090/api/v1/write"
    # Optional: Add custom headers
    # headers:
    #   X-Scope-OrgID: "tenant-1"

service:
  pipelines:
    metrics:
      exporters: [prometheusremotewrite]
```

## Testing

### Health Check

```bash
curl http://localhost:9090/health
```

### Manual Write (requires protobuf tools)

```bash
# Create a WriteRequest protobuf
# See common/proto/remote.proto for message format

# Send to server
curl -X POST http://localhost:9090/api/v1/write \
  -H "Content-Type: application/x-protobuf" \
  --data-binary @write_request.pb
```

### Manual Read (requires protobuf tools)

```bash
# Create a ReadRequest protobuf
# See common/proto/remote.proto for message format

# Send to server
curl -X POST http://localhost:9090/api/v1/read \
  -H "Content-Type: application/x-protobuf" \
  --data-binary @read_request.pb
```

## Features

- ✅ Prometheus Remote Write API
- ✅ Prometheus Remote Read API
- ✅ Snappy compression support (optional)
- ✅ Multi-threaded HTTP server
- ✅ Persistent storage with WAL
- ✅ Automatic compression

## Next Steps

- Add authentication (see `prometheus_auth_requirements.md`)
- Add multi-tenancy support
- Add metrics endpoint for monitoring
- Add configuration file support
