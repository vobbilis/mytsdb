# Phase 5: Comprehensive PromQL Testing & Scale Simulation

## Objective
To validate the PromQL engine against a massive, realistic dataset simulating a large-scale Kubernetes environment, ensuring 100% correctness across all possible query combinations and edge cases.

## Scope
1.  **Scale Simulation**: Generate 10M+ metric samples spanning 30 days, simulating a large K8s cluster.
2.  **Comprehensive Test Suite**: Implement ~1000 additional test cases covering every PromQL feature, function, and edge case.
3.  **Performance Verification**: Validate query performance and correctness at scale.

## 1. Data Generation Strategy (`DATA_GENERATION_SPEC.md`)
We will build a `SyntheticClusterGenerator` to simulate:
- **Infrastructure**: Nodes, Pods, Deployments, StatefulSets, DaemonSets, Services.
- **Labels**:
    - Standard K8s: `pod`, `namespace`, `node`, `service`, `job`, `instance`.
    - Metadata: `version` (SemVer), `environment` (dev/staging/prod), `team`, `region`, `zone`.
- **Metrics**:
    - **Infrastructure**: `node_cpu_seconds_total`, `node_memory_usage_bytes`, `container_cpu_usage_seconds_total`, `kube_pod_status_phase`.
    - **Application**: `http_requests_total`, `http_request_duration_seconds_bucket`, `jvm_memory_bytes_used`, `go_goroutines`.
    - **Business**: `orders_total`, `payment_processing_seconds`.
- **Patterns**:
    - Normal traffic (sinusoidal, linear growth).
    - Spikes and anomalies.
    - Counter resets (simulating restarts).
    - Churn (pods created/destroyed).

## 2. Test Coverage Strategy (`TEST_COVERAGE_STRATEGY.md`)
We will source test cases from:
1.  **Prometheus Compliance Tests**: Official Prometheus compliance test suite.
2.  **Community Scenarios**: Common and complex queries found in Grafana dashboards and alerting rules (e.g., `kube-mixin`).
3.  **Combinatorial Generation**: Systematically generating combinations of:
    - Selectors (exact, regex, negative).
    - Ranges (1m, 5m, 1h, 1d, 30d).
    - Aggregations (sum, avg, min, max, quantile).
    - Functions (rate, irate, delta, increase, predict_linear).
    - Binary Operations (arithmetic, comparison, logical, vector matching).
    - Subqueries and offsets.

## 3. Execution Framework
- **Location**: `test/comprehensive_promql/`
- **Harness**: A dedicated C++ test harness that:
    1.  Initializes the TSDB storage.
    2.  Ingests the generated synthetic data.
    3.  Runs the test suite (queries vs expected results).
    4.  Reports detailed pass/fail metrics and performance stats.

## Roadmap
1.  **Design Data Generator**: Define schema and generation logic.
2.  **Implement Data Generator**: Build the tool to populate TSDB.
3.  **Design Test Cases**: Catalog the 1000+ scenarios.
4.  **Implement Test Harness**: Build the runner.
5.  **Execute & Fix**: Run tests, identify bugs, fix engine/storage, repeat.
