# GPT52 — kind + Kubernetes deployment plan (MyTSDB + OTEL Collector + sample-app + Grafana)

## Goals
- Deploy **MyTSDB** into a **local kind cluster** with:
  - **PromQL HTTP API** exposed to Grafana
  - **OTLP/gRPC ingestion** via an **OTEL Collector** (for a sample app + integration validation)
  - **Apache Arrow Flight ingestion** used to seed a **large, realistic K8s dataset** (7 days) on every pod start
- Ensure **clean start** on every MyTSDB pod start (fresh data dir).
- Ensure MyTSDB is marked **Ready only after Arrow seeding is complete**, so Grafana can reliably query real data.
- Provision Grafana with a **preloaded dashboard JSON** that exercises a wide PromQL surface area over realistic K8s labels:
  - by **cluster / region / zone**
  - by **namespace / service / deployment / pod / container / node**
  - CPU, memory, network, filesystem, kube-state, HTTP latency histograms

## What we already have in-repo (artifacts to update)
Location: `deployments/kubernetes/`

- **MyTSDB**: `mytsdb/00-namespace.yaml`, `01-pvc.yaml`, `02-deployment.yaml`, `03-service.yaml`, Dockerfile.
- **OTEL Collector**: `otel-collector/00-configmap.yaml`, `01-deployment.yaml`, `02-service.yaml`.
- **Sample app**: `sample-app/*` (Python OTEL SDK → OTLP/gRPC to collector).
- **Grafana**: datasources + dashboards provisioning via configmaps (`grafana/*.yaml`, several dashboard JSON files).
- **Kind + automation**: `kind/cluster-config.yaml`, `install.sh`, `deploy.sh`, `manage-images.sh`, `Makefile`.

### Key gaps vs current project state
- **Arrow Flight is not wired into K8s**:
  - `mytsdb/02-deployment.yaml` does **not** pass `--arrow-port` nor expose port `8815`.
  - `mytsdb/03-service.yaml` does **not** expose the Arrow port.
- **Dockerfile does not build/copy the Arrow seeding tool**:
  - It currently builds `synthetic_cluster_generator` (OTEL path), but the desired seed is the Arrow path tool:
    - `tools/k8s_data_generator` (supports **7 days** and **12 K8s label dimensions**).
- **Readiness semantics do not include “seed finished”**:
  - `/health` exists and should stay for liveness.
  - We need a separate readiness gate: **Ready == seed done** (plus server alive).
- Existing Grafana dashboards appear partially tailored to older metric naming / derived metrics. We’ll add a new “K8s Seeded Cluster” dashboard that matches the generator’s metrics/labels.

## Target runtime wiring (ports + services)
MyTSDB must expose:
- **HTTP PromQL**: `9090` (Service name `mytsdb`)
- **Arrow Flight**: `8815` (Service name `mytsdb`)
- **OTLP/gRPC (optional)**: `4317` (either direct to MyTSDB *or* via OTEL Collector; today we use collector → remote write)

OTEL Collector:
- OTLP gRPC `4317`, OTLP HTTP `4318`
- exports to MyTSDB **remote write**: `http://mytsdb:9090/api/v1/write`

Grafana:
- connects to MyTSDB Prometheus API: `http://mytsdb:9090`

## “Clean start + seed + readiness” design
We need three properties simultaneously:
1. The MyTSDB pod starts from a **clean data directory**.
2. After the server starts, it is **seeded with 7 days of K8s metrics via Arrow Flight**.
3. K8s considers the pod **Ready only after seeding finishes successfully**.

### Recommended approach: seed sidecar + readiness file gate
In `mytsdb/02-deployment.yaml`:
- Use a shared `emptyDir` volume:
  - `data`: mounted at `/data` for the server (clean each pod start)
  - `ready`: mounted at `/ready` shared between containers
- Add **initContainer** `wipe-data`:
  - `rm -rf /data/* /ready/*`
- Main container `mytsdb`:
  - args: `--http-port 9090 --arrow-port 8815 --data-dir /data --address 0.0.0.0:4317`
  - livenessProbe: HTTP GET `/health` on 9090
  - readinessProbe: `exec` check `test -f /ready/seed_done`
- Add **seed sidecar** container `k8s-seed`:
  - waits for Arrow port to be reachable (simple TCP loop)
  - runs `k8s_data_generator` against `localhost:8815` with `--days 7` and a chosen scale preset
  - on success: `touch /ready/seed_done`
  - then sleeps forever (so it doesn’t restart-loop)

This yields:
- service endpoints won’t route traffic to MyTSDB until seed is complete
- `install.sh` / deployment scripts that `kubectl wait --for=condition=ready` will naturally wait for seeding

### Scale choice (practicality)
`tools/k8s_data_generator` can generate extremely large datasets if run at “full 9k pods / 100 metrics / 15s scrape for 7d”.

We’ll parameterize scale via env vars on the seed container (defaults that complete quickly on a laptop):
- `SEED_PRESET`: `small|medium|large` (default `small`)
- `SEED_DAYS`: default `7`

If needed, we can extend `k8s_data_generator` to accept additional knobs (pods/nodes/services) to fit local resources.

## Docker image updates (MyTSDB + tools)
Update `deployments/kubernetes/mytsdb/Dockerfile`:
- Build **with Arrow Flight enabled** and include:
  - `tsdb_server`
  - `tools/k8s_data_generator`
  - (optional) `tools/k8s_combined_benchmark` for in-cluster perf runs later
- Runtime image must include Arrow/Flight shared libs.

Practical build strategy (choose one):
- **Option A (preferred)**: use a builder base image that already includes Arrow/Parquet dev libs (e.g. an Arrow dev image), then copy the runtime libs + binaries.
- **Option B**: install Arrow/Flight from OS packages (may be older), acceptable for local kind.
- **Option C**: build Arrow from source (slow; not recommended for fast iteration).

## Grafana provisioning (datasource + dashboards)
Current datasource configmap (`grafana/00-datasource-configmap.yaml`) is close: it points to `http://mytsdb:9090`.

We will add a new dashboard JSON dedicated to the Arrow-seeded dataset:
- Provision via ConfigMap `grafana-dashboards-k8s-seeded`
- Mount into `/var/lib/grafana/dashboards/k8s-seeded`
- Ensure a dashboards provider points at that folder.

### Dashboard content (high-level)
Create a “K8s Seeded Cluster (7d)” dashboard with:
- **Variables**:
  - `region`, `zone`, `cluster`, `namespace`, `service`, `deployment`, `pod`, `container`, `node`
  - implemented via Grafana `label_values()` queries over the seeded label set
- **Panels** (examples):
  - **CPU by pod**: `sum(rate(container_cpu_usage_seconds_total[5m])) by (pod, namespace)`
  - **Memory by pod**: `sum(container_memory_working_set_bytes) by (pod, namespace)`
  - **Node CPU busy%**: `1 - avg(rate(node_cpu_seconds_total{mode="idle"}[5m])) by (node)`
  - **Node memory used%**: `1 - (node_memory_MemAvailable_bytes / node_memory_MemTotal_bytes)`
  - **HTTP RPS by service**: `sum(rate(http_requests_total[5m])) by (service, namespace)`
  - **HTTP latency p99**: `histogram_quantile(0.99, sum(rate(http_request_duration_seconds_bucket[5m])) by (le, service))`
  - **Kube pod status**: `count(kube_pod_status_phase) by (namespace, phase)`
  - **Restarts**: `sum(increase(kube_pod_container_status_restarts_total[1h])) by (namespace)`

Time-range defaults:
- dashboard default: `now-7d to now` (or `now-24h` for performance), with quick-select presets.

## OTEL Collector + sample-app integration test
Keep the current flow:
- sample-app → OTLP/gRPC → collector (`otel-collector:4317`)
- collector → remote write → mytsdb (`/api/v1/write`)

Validation checklist:
- Confirm `http_requests_total` appears in Grafana (sample-app stream).
- Confirm seeded K8s metrics appear in Grafana (Arrow stream).
- Confirm PromQL read path works for both instant + range queries.

## Rollout steps (operator workflow)
1. `cd deployments/kubernetes && make install` (or `./install.sh`)
2. MyTSDB pod starts:
   - initContainer wipes data
   - server starts (HTTP+Arrow)
   - seed sidecar runs `k8s_data_generator --days 7 ...`
   - once done: readiness file created → pod becomes Ready
3. OTEL Collector + sample-app deploy
4. Grafana deploys and loads dashboards automatically

## What we will update (file-level)
- `deployments/kubernetes/mytsdb/02-deployment.yaml`
  - add Arrow port, seed sidecar, initContainer wipe, readiness file gate
- `deployments/kubernetes/mytsdb/03-service.yaml`
  - add Arrow port `8815`
- `deployments/kubernetes/mytsdb/Dockerfile`
  - build/copy `k8s_data_generator` (and ensure Arrow libs in runtime)
- `deployments/kubernetes/grafana/*`
  - add new dashboard JSON + provisioning
- `deployments/kubernetes/install.sh` / `Makefile`
  - ensure “wait for MyTSDB ready” naturally includes “seed done” (because readiness gate changes)

## Open questions (to confirm before implementation)
1. **Seed scale**: ✅ Use **~20M samples** total. We’ll choose the generator knobs to hit this target.
2. **Persistence**: ✅ **Always clean** on every pod start (ephemeral `emptyDir` + initContainer wipe).
3. **OTEL ingestion**: ✅ Validate OTEL ingestion still works via **OTLP/gRPC** (OTEL exporter → collector → MyTSDB OTLP endpoint).

## Decisions (applied)
### Seeding target: ~20M samples via Arrow Flight
We will seed using `tools/k8s_data_generator` (Arrow Flight) and tune parameters to land near 20M samples.

Target formula (approx):
\[
samples \approx total\_pods \times containers\_per\_pod \times metrics\_per\_container \times \frac{days \times 24 \times 3600}{scrape\_interval\_sec}
\]

Recommended “20M-ish” seed defaults (local-kind friendly):
- **days**: 1
- **scrape interval**: 15s
- **pods**: 20 (achieved via namespaces/services/pods-per-service)
- **containers per pod**: 2
- **metrics per container**: 90

This yields roughly:
- \(20 \times 2 \times 90 \times 5760 \approx 20.7M\) samples

Implementation detail: we will extend `tools/k8s_data_generator` CLI to accept these knobs explicitly (today it only exposes `--days` + presets).

### Always-clean MyTSDB pod start
We will switch MyTSDB storage to an **ephemeral** `emptyDir` volume and wipe it at pod start:
- initContainer: `rm -rf /data/* /ready/*`
- MyTSDB runs with `--data-dir /data`

### OTEL ingestion validation (OTLP/gRPC)
MyTSDB already hosts an OTLP gRPC MetricsService on the configured `--address` (default `0.0.0.0:4317`).
We will update the OTEL Collector to export **OTLP** to MyTSDB:
- exporter: `otlp` to `mytsdb.mytsdb.svc.cluster.local:4317` (insecure)
- keep `logging` exporter for visibility

We can optionally keep the existing `prometheusremotewrite` exporter as a second pipeline if we want to validate both ingestion routes, but the primary goal here is OTLP/gRPC.


