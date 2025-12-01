# Write Path Optimization Tasks

- [x] **Analyze Write Path Bottlenecks**
    - [x] Instrument `WritePerformanceInstrumentation` with granular metrics.
    - [x] Run benchmarks to identify bottleneck (OTEL Conversion).
    - [x] Create `BOTTLENECK_ANALYSIS.md`.

- [x] **Optimize OTEL Conversion**
    - [x] Create `OTEL_OPTIMIZATION_PLAN.md`.
    - [x] Implement batching in `src/tsdb/otel/bridge.cpp`.
    - [x] Verify performance improvement (45% throughput increase).

- [x] **Verify Benchmark Logic**
    - [x] Fix pod calculation in `tools/benchmark_tool.cpp`.
    - [x] Verify 100k series load generation.
    - [x] Update `walkthrough.md` and `BOTTLENECK_ANALYSIS.md` with final results.

- [x] **Documentation Update**
    - [x] Create `docs/performance`, `docs/design`, `docs/deployment` directories.
    - [x] Copy artifacts to project docs.
    - [x] Update `README.md` with performance stats and K8s info.
    - [x] Update `GETTING_STARTED.md` with K8s deployment guide.
