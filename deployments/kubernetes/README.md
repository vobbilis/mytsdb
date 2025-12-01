# MyTSDB Kubernetes Deployment

Complete Kubernetes deployment for MyTSDB with Grafana, OTEL Collector, and sample application.

## Architecture

```
┌─────────────────┐
│  Sample App     │ (Python app generating metrics)
│  (OTEL SDK)     │
└────────┬────────┘
         │ OTLP/gRPC
         ▼
┌─────────────────┐
│ OTEL Collector  │ (Receives and forwards metrics)
└────────┬────────┘
         │ Prometheus Remote Write
         ▼
┌─────────────────┐
│    MyTSDB       │ (Time series database)
└────────┬────────┘
         │ Prometheus API
         ▼
┌─────────────────┐
│    Grafana      │ (Visualization)
└─────────────────┘
```

## Components

1. **MyTSDB** - Time series database with Prometheus Remote Storage API
2. **OTEL Collector** - Receives OTLP metrics and forwards to MyTSDB
3. **Sample Application** - Python app generating HTTP metrics
4. **Grafana** - Visualization and dashboarding

## Prerequisites

- **Docker** - For building images
- **Kind** - Kubernetes in Docker
- **kubectl** - Kubernetes CLI

### Install Prerequisites (macOS)

```bash
# Install Docker Desktop
# Download from: https://www.docker.com/products/docker-desktop

# Install Kind
brew install kind

# Install kubectl
brew install kubectl

# Verify installations
docker --version
kind --version
kubectl version --client
```

### Install Prerequisites (Linux)

```bash
# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Install Kind
curl -Lo ./kind https://kind.sigs.k8s.io/dl/v0.20.0/kind-linux-amd64
chmod +x ./kind
sudo mv ./kind /usr/local/bin/kind

# Install kubectl
curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
chmod +x kubectl
sudo mv kubectl /usr/local/bin/

# Verify installations
docker --version
kind --version
kubectl version --client
```

## Quick Start

### Option 1: Automated Install (Recommended)

```bash
cd deployments/kubernetes

# Run pre-flight checks
./preflight.sh

# Fresh install
./install.sh

# Or use Makefile
make install
```

### Option 2: One-Command Install

```bash
cd deployments/kubernetes
make install
```

This will:
1. ✅ Run pre-flight checks
2. ✅ Create Kind cluster
3. ✅ Build Docker images
4. ✅ Deploy all components
5. ✅ Wait for everything to be ready

### 2. Access Grafana

Open your browser to: **http://localhost:3000**

- **Username**: `admin`
- **Password**: `admin`

### 3. View Metrics

1. In Grafana, go to **Dashboards**
2. Open **"MyTSDB Sample Application Metrics"**
3. You should see live metrics from the sample application!

## Makefile Targets

The Makefile provides 50+ targets for managing your deployment:

```bash
make help                # Show all targets
make status              # View pod status
make logs-mytsdb         # View MyTSDB logs
make update-mytsdb       # Update MyTSDB
make restart-all         # Restart all services
make clean               # Full cleanup
```

See [QUICK_REFERENCE.md](QUICK_REFERENCE.md) for complete command list.

## Manual Deployment Steps

If you prefer to deploy manually:

### 1. Create Kind Cluster

```bash
kind create cluster --config kind/cluster-config.yaml
```

### 2. Build and Load Images

```bash
# Build MyTSDB image
cd ../../..
docker build -t mytsdb:latest -f deployments/kubernetes/mytsdb/Dockerfile .
kind load docker-image mytsdb:latest --name mytsdb-cluster

# Build sample app image
cd deployments/kubernetes/sample-app
docker build -t sample-app:latest .
kind load docker-image sample-app:latest --name mytsdb-cluster
```

### 3. Deploy Components

```bash
cd ..

# Deploy MyTSDB
kubectl apply -f mytsdb/00-namespace.yaml
kubectl apply -f mytsdb/01-pvc.yaml
kubectl apply -f mytsdb/02-deployment.yaml
kubectl apply -f mytsdb/03-service.yaml

# Deploy OTEL Collector
kubectl apply -f otel-collector/00-configmap.yaml
kubectl apply -f otel-collector/01-deployment.yaml
kubectl apply -f otel-collector/02-service.yaml

# Deploy Sample App
kubectl apply -f sample-app/00-deployment.yaml

# Deploy Grafana
kubectl apply -f grafana/00-datasource-configmap.yaml
kubectl apply -f grafana/01-dashboard-configmap.yaml
kubectl apply -f grafana/02-deployment.yaml
kubectl apply -f grafana/03-service.yaml
```

## Verification

### Check Pod Status

```bash
kubectl get pods -n mytsdb
```

Expected output:
```
NAME                              READY   STATUS    RESTARTS   AGE
grafana-xxx                       1/1     Running   0          1m
mytsdb-xxx                        1/1     Running   0          2m
otel-collector-xxx                1/1     Running   0          2m
sample-app-xxx                    1/1     Running   0          1m
```

### View Logs

```bash
# MyTSDB logs
kubectl logs -f -l app=mytsdb -n mytsdb

# OTEL Collector logs
kubectl logs -f -l app=otel-collector -n mytsdb

# Sample app logs
kubectl logs -f -l app=sample-app -n mytsdb

# Grafana logs
kubectl logs -f -l app=grafana -n mytsdb
```

### Test MyTSDB Directly

```bash
# Port-forward MyTSDB
kubectl port-forward -n mytsdb svc/mytsdb 9090:9090

# In another terminal, check health
curl http://localhost:9090/health
```

### Query Metrics

```bash
# Port-forward MyTSDB
kubectl port-forward -n mytsdb svc/mytsdb 9090:9090

# Query using Prometheus API
curl -X POST http://localhost:9090/api/v1/read \
  -H "Content-Type: application/x-protobuf" \
  --data-binary @read_request.pb
```

## Grafana Dashboard

The deployment includes a pre-configured dashboard showing:

- **Request Rate** - HTTP requests per second
- **Request Duration** - p95 latency
- **Active Requests** - Current concurrent requests
- **Total Requests** - Cumulative request count

### Creating Custom Dashboards

1. Go to **Dashboards** → **New** → **New Dashboard**
2. Add a panel
3. Select **MyTSDB** as the datasource
4. Enter a PromQL query, e.g.:
   ```promql
   rate(http_requests_total[5m])
   ```
5. Customize visualization and save

## Troubleshooting

### Pods Not Starting

```bash
# Check pod status
kubectl get pods -n mytsdb

# Describe pod for events
kubectl describe pod <pod-name> -n mytsdb

# Check logs
kubectl logs <pod-name> -n mytsdb
```

### No Metrics in Grafana

1. **Check Sample App is Running**
   ```bash
   kubectl logs -f -l app=sample-app -n mytsdb
   ```
   You should see: "Generated: GET /api/users - 200 (0.123s)"

2. **Check OTEL Collector**
   ```bash
   kubectl logs -f -l app=otel-collector -n mytsdb
   ```
   Look for successful exports to MyTSDB

3. **Check MyTSDB**
   ```bash
   kubectl logs -f -l app=mytsdb -n mytsdb
   ```
   Look for incoming write requests

4. **Verify Datasource in Grafana**
   - Go to **Configuration** → **Data Sources**
   - Click on **MyTSDB**
   - Click **Save & Test**
   - Should show "Data source is working"

### Image Pull Errors

If you see `ImagePullBackOff`:

```bash
# Rebuild and reload images
docker build -t mytsdb:latest -f mytsdb/Dockerfile ../../..
kind load docker-image mytsdb:latest --name mytsdb-cluster

# Restart deployment
kubectl rollout restart deployment/mytsdb -n mytsdb
```

### Port Already in Use

If port 3000 is already in use:

```bash
# Find process using port
lsof -i :3000

# Kill process or change port in grafana/03-service.yaml
```

## Configuration

### Changing Authentication

Edit `mytsdb/02-deployment.yaml`:

```yaml
args: ["9090", "basic"]  # Change to: basic, bearer, header, composite
```

Then apply:
```bash
kubectl apply -f mytsdb/02-deployment.yaml
kubectl rollout restart deployment/mytsdb -n mytsdb
```

### Adjusting Resources

Edit resource limits in deployment files:

```yaml
resources:
  requests:
    memory: "512Mi"
    cpu: "500m"
  limits:
    memory: "2Gi"
    cpu: "2000m"
```

### Scaling

```bash
# Scale MyTSDB (note: requires shared storage for multiple replicas)
kubectl scale deployment mytsdb -n mytsdb --replicas=2

# Scale sample app (generates more metrics)
kubectl scale deployment sample-app -n mytsdb --replicas=3
```

## Cleanup

### Remove Everything

```bash
./cleanup.sh
```

This will:
1. Delete the namespace (and all resources)
2. Delete the Kind cluster

### Partial Cleanup

```bash
# Delete just the namespace
kubectl delete namespace mytsdb

# Keep cluster for redeployment
kind get clusters
```

## File Structure

```
deployments/kubernetes/
├── deploy.sh                    # Main deployment script
├── cleanup.sh                   # Cleanup script
├── kind/
│   └── cluster-config.yaml      # Kind cluster configuration
├── mytsdb/
│   ├── Dockerfile               # MyTSDB Docker image
│   ├── .dockerignore
│   ├── 00-namespace.yaml        # Namespace
│   ├── 01-pvc.yaml              # Persistent volume claim
│   ├── 02-deployment.yaml       # MyTSDB deployment
│   └── 03-service.yaml          # MyTSDB service
├── otel-collector/
│   ├── 00-configmap.yaml        # OTEL config
│   ├── 01-deployment.yaml       # OTEL deployment
│   └── 02-service.yaml          # OTEL service
├── sample-app/
│   ├── app.py                   # Python metrics generator
│   ├── Dockerfile               # Sample app image
│   └── 00-deployment.yaml       # Sample app deployment
├── grafana/
│   ├── 00-datasource-configmap.yaml   # Datasource config
│   ├── 01-dashboard-configmap.yaml    # Dashboard config
│   ├── 02-deployment.yaml             # Grafana deployment
│   └── 03-service.yaml                # Grafana service (NodePort)
└── README.md                    # This file
```

## Next Steps

1. **Explore Metrics** - View different metric types in Grafana
2. **Create Dashboards** - Build custom visualizations
3. **Add Authentication** - Enable auth in MyTSDB deployment
4. **Scale Up** - Test with multiple sample app replicas
5. **Production Setup** - Add Ingress, TLS, persistent storage

## Production Considerations

For production deployments:

1. **Persistent Storage** - Use proper StorageClass (not `standard`)
2. **High Availability** - Run multiple MyTSDB replicas with shared storage
3. **Authentication** - Enable auth in MyTSDB (see AUTHENTICATION.md)
4. **TLS** - Add TLS certificates for all services
5. **Ingress** - Use Ingress controller instead of NodePort
6. **Monitoring** - Add Prometheus to monitor MyTSDB itself
7. **Backup** - Implement backup strategy for PVC data
8. **Resource Limits** - Tune based on actual workload
9. **Network Policies** - Restrict pod-to-pod communication
10. **RBAC** - Implement proper role-based access control

## Support

For issues or questions:
- Check logs: `kubectl logs -f <pod-name> -n mytsdb`
- View events: `kubectl get events -n mytsdb`
- Describe resources: `kubectl describe <resource> -n mytsdb`

See main [README.md](../../README.md) for more information about MyTSDB.
