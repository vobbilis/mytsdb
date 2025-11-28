# Data Generation Specification: Synthetic K8s Cluster

## Objective
Generate a realistic dataset representing a large Kubernetes cluster to stress-test the PromQL engine.

## Scale Targets
- **Duration**: 30 days
- **Total Samples**: > 10,000,000
- **Resolution**: 15s scrape interval (configurable)
- **Cardinality**: High (due to pod churn and high-cardinality labels)

## Simulation Model

### 1. Cluster Topology
- **Nodes**: 50 nodes (3 zones: `us-west-1a`, `us-west-1b`, `us-west-1c`)
- **Namespaces**: `kube-system`, `monitoring`, `default`, `payment-service`, `inventory-service`, `frontend`, `auth-service`.

### 2. Workloads (Deployments/StatefulSets)
For each service (`payment`, `inventory`, `frontend`, `auth`):
- **Replicas**: 5-50 pods per service (auto-scaling simulation).
- **Versions**: Rolling updates (e.g., `v1.0.0` -> `v1.0.1` -> `v1.1.0`).
- **Environments**: `dev`, `staging`, `prod`.

### 3. Metric Series Schema

#### A. Infrastructure Metrics (Node)
- `node_cpu_seconds_total{mode="idle|user|system|iowait", instance="node-X", zone="Z"}`
- `node_memory_MemTotal_bytes{instance="node-X", ...}`
- `node_memory_MemAvailable_bytes{instance="node-X", ...}`
- `node_filesystem_size_bytes{mountpoint="/", ...}`

#### B. Container Metrics (cAdvisor style)
- `container_cpu_usage_seconds_total{pod="pod-X", namespace="NS", container="main", image="img:tag"}`
- `container_memory_usage_bytes{...}`
- `container_network_receive_bytes_total{...}`

#### C. Application Metrics (Custom)
- `http_requests_total{method="GET|POST", path="/api/v1/...", status="200|400|500", service="SVC", version="SEMVER"}`
- `http_request_duration_seconds_bucket{le="0.1|0.5|1|5|+Inf", ...}` (Histogram)
- `jvm_memory_bytes_used{area="heap|nonheap", ...}`
- `kafka_topic_partition_current_offset{topic="orders", partition="0..N"}`

### 4. Data Patterns & Anomalies
1.  **Daily Seasonality**: Traffic peaks during day, drops at night (sinusoidal).
2.  **Linear Growth**: Database size growing over time.
3.  **Spikes**: Sudden 500 errors or CPU spikes lasting 5-15 mins.
4.  **Counter Resets**: Pod restarts causing `_total` counters to reset to 0.
5.  **Churn**: Pods being replaced (new pod names, new series) every few hours/days.
6.  **Missing Data**: Scrape failures (gaps in data).

## Implementation Plan
- Create a C++ tool `tools/data_gen/main.cpp`.
- **Ingestion Method**: gRPC/OTEL Write Path.
    - The generator will act as a client sending `ExportMetricsServiceRequest` via gRPC.
    - Connects to the TSDB server (default port 9090 or configurable).
    - Uses the existing `tsdb::otel::GrpcClient` or builds a custom client using generated protobuf stubs.
- Configuration via JSON/YAML to define cluster size, duration, and target gRPC endpoint.
