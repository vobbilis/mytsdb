# MyTSDB Kubernetes Networking Guide

## Overview

This document explains how MyTSDB networking is configured in the Kubernetes deployment, covering both internal cluster communication and external access.

## Network Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Host Machine (localhost)                                     │
│                                                               │
│  Port 3000 ──────────────────────────────────────────────┐  │
│  Port 9090 (optional) ────────────────────────────────┐  │  │
└───────────────────────────────────────────────────────│──│──┘
                                                         │  │
                    ┌────────────────────────────────────│──│──┐
                    │ Kind Cluster                       │  │  │
                    │                                    ▼  ▼  │
                    │  ┌──────────────────────────────────────┤
                    │  │ NodePort 30300 → Grafana:3000        │
                    │  │ NodePort 30090 → MyTSDB:9090         │
                    │  └──────────────────────────────────────┤
                    │                                          │
                    │  ┌─────────────────────────────────┐    │
                    │  │ Services (ClusterIP)            │    │
                    │  │                                 │    │
                    │  │  mytsdb.mytsdb.svc:9090        │    │
                    │  │  otel-collector.mytsdb.svc:4317│    │
                    │  │  grafana.mytsdb.svc:3000       │    │
                    │  └─────────────────────────────────┘    │
                    │           │                              │
                    │           ▼                              │
                    │  ┌─────────────────────────────────┐    │
                    │  │ Pods                            │    │
                    │  │                                 │    │
                    │  │  mytsdb:9090                   │    │
                    │  │  otel-collector:4317,4318      │    │
                    │  │  sample-app                    │    │
                    │  │  grafana:3000                  │    │
                    │  └─────────────────────────────────┘    │
                    └──────────────────────────────────────────┘
```

## 1. Internal Cluster Networking (Service-to-Service)

### MyTSDB Service (ClusterIP)

**File:** `mytsdb/03-service.yaml`

```yaml
apiVersion: v1
kind: Service
metadata:
  name: mytsdb
  namespace: mytsdb
spec:
  type: ClusterIP  # Internal only
  ports:
  - name: http
    port: 9090
    targetPort: 9090
    protocol: TCP
  selector:
    app: mytsdb
```

**DNS Name:** `mytsdb.mytsdb.svc.cluster.local:9090`
**Short Name:** `mytsdb:9090` (within same namespace)

### How Services Connect to MyTSDB

**OTEL Collector → MyTSDB:**
```yaml
# otel-collector/00-configmap.yaml
exporters:
  prometheusremotewrite:
    endpoint: "http://mytsdb.mytsdb.svc.cluster.local:9090/api/v1/write"
```

**Grafana → MyTSDB:**
```yaml
# grafana/00-datasource-configmap.yaml
datasources:
- name: MyTSDB
  url: http://mytsdb.mytsdb.svc.cluster.local:9090
```

### Verification

```bash
# Test from within cluster
kubectl run curl-test --image=curlimages/curl:latest --rm -i --restart=Never -n mytsdb -- \
  curl -s http://mytsdb.mytsdb.svc.cluster.local:9090/health

# Or use Makefile
make test-mytsdb-health
```

## 2. External Access (From Host Machine)

### Option A: Port Forwarding (Recommended for Development)

**Grafana (Already Configured):**
```bash
# Access via NodePort (automatic)
http://localhost:3000

# Or manual port-forward
kubectl port-forward -n mytsdb svc/grafana 3000:3000
```

**MyTSDB (Manual Port Forward):**
```bash
# Forward MyTSDB to localhost
kubectl port-forward -n mytsdb svc/mytsdb 9090:9090

# Then access
curl http://localhost:9090/health

# Or use Makefile
make forward-mytsdb
```

### Option B: NodePort Service (Optional, for Direct Access)

If you want direct access to MyTSDB from your host machine without port-forwarding:

**Create:** `mytsdb/04-service-nodeport.yaml` (optional)

```yaml
apiVersion: v1
kind: Service
metadata:
  name: mytsdb-external
  namespace: mytsdb
  labels:
    app: mytsdb
    component: database
spec:
  type: NodePort
  ports:
  - name: http
    port: 9090
    targetPort: 9090
    nodePort: 30090  # Maps to host port 9090 via Kind config
    protocol: TCP
  selector:
    app: mytsdb
```

**Deploy:**
```bash
kubectl apply -f mytsdb/04-service-nodeport.yaml
```

**Access:**
```
http://localhost:9090/health
http://localhost:9090/api/v1/write
http://localhost:9090/api/v1/read
```

### Kind Cluster Port Mapping

**File:** `kind/cluster-config.yaml`

```yaml
nodes:
- role: control-plane
  extraPortMappings:
  # Grafana
  - containerPort: 30300  # NodePort in cluster
    hostPort: 3000        # Port on host machine
    protocol: TCP
  # MyTSDB (optional)
  - containerPort: 30090  # NodePort in cluster
    hostPort: 9090        # Port on host machine
    protocol: TCP
```

**How it works:**
1. Kind maps `localhost:3000` → `cluster:30300`
2. NodePort service maps `cluster:30300` → `pod:3000`
3. Result: `localhost:3000` → `grafana pod:3000`

## 3. Container Port Configuration

### MyTSDB Deployment

**File:** `mytsdb/02-deployment.yaml`

```yaml
containers:
- name: mytsdb
  image: mytsdb:latest
  args: ["9090", "none"]  # Port 9090, no auth
  ports:
  - name: http
    containerPort: 9090  # Container listens on 9090
    protocol: TCP
```

### Docker Image

**File:** `mytsdb/Dockerfile`

```dockerfile
# Expose port in image
EXPOSE 9090

# Server listens on port 9090
CMD ["9090", "none"]
```

## 4. Complete Port Reference

| Service | Internal (ClusterIP) | External (NodePort) | Host Access | Purpose |
|---------|---------------------|---------------------|-------------|---------|
| MyTSDB | mytsdb:9090 | 30090 | localhost:9090* | Prometheus Remote Storage |
| OTEL Collector | otel-collector:4317 | - | - | OTLP gRPC |
| OTEL Collector | otel-collector:4318 | - | - | OTLP HTTP |
| Grafana | grafana:3000 | 30300 | localhost:3000 | Visualization |
| Sample App | - | - | - | Metrics generator |

\* Requires NodePort service or port-forward

## 5. Testing Connectivity

### Internal (Service-to-Service)

```bash
# OTEL → MyTSDB
make logs-otel | grep "successfully sent"

# Grafana → MyTSDB
# Open Grafana → Configuration → Data Sources → MyTSDB → Test

# Manual test from within cluster
kubectl run curl-test --image=curlimages/curl:latest --rm -i --restart=Never -n mytsdb -- \
  curl -s http://mytsdb:9090/health
```

### External (Host → Cluster)

```bash
# Grafana (via NodePort)
curl http://localhost:3000/api/health

# MyTSDB (via port-forward)
kubectl port-forward -n mytsdb svc/mytsdb 9090:9090 &
curl http://localhost:9090/health

# Or use Makefile
make forward-mytsdb
# In another terminal:
curl http://localhost:9090/health
```

## 6. Troubleshooting

### Service Not Reachable Internally

```bash
# Check service exists
kubectl get svc -n mytsdb

# Check endpoints
kubectl get endpoints mytsdb -n mytsdb

# Should show pod IP:
# NAME     ENDPOINTS         AGE
# mytsdb   10.244.0.5:9090   5m

# Check pod is running
kubectl get pods -l app=mytsdb -n mytsdb

# Test DNS resolution
kubectl run curl-test --image=curlimages/curl:latest --rm -i --restart=Never -n mytsdb -- \
  nslookup mytsdb.mytsdb.svc.cluster.local
```

### External Access Not Working

```bash
# Check NodePort service
kubectl get svc mytsdb-external -n mytsdb

# Check Kind port mappings
docker ps | grep mytsdb-cluster

# Check if port is listening on host
lsof -i :9090

# Recreate cluster with port mappings
kind delete cluster --name mytsdb-cluster
kind create cluster --config kind/cluster-config.yaml
```

### Connection Refused

```bash
# Check pod logs
kubectl logs -l app=mytsdb -n mytsdb

# Check if server is listening
kubectl exec -it -n mytsdb $(kubectl get pod -l app=mytsdb -n mytsdb -o jsonpath='{.items[0].metadata.name}') -- \
  netstat -tlnp | grep 9090

# Check health endpoint
kubectl exec -it -n mytsdb $(kubectl get pod -l app=mytsdb -n mytsdb -o jsonpath='{.items[0].metadata.name}') -- \
  curl localhost:9090/health
```

## 7. Security Considerations

### Internal Communication

- ✅ All services use ClusterIP (not exposed externally)
- ✅ Communication within cluster is encrypted by K8s network policies (optional)
- ✅ Services can only be accessed from within the cluster

### External Access

- ⚠️ NodePort exposes service to host machine
- ⚠️ No TLS by default (use port-forward for secure access)
- ✅ Only accessible from localhost (Kind limitation)
- 🔒 For production: Use Ingress with TLS

## 8. Production Recommendations

### Use Ingress Instead of NodePort

```yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: mytsdb-ingress
  namespace: mytsdb
  annotations:
    cert-manager.io/cluster-issuer: "letsencrypt-prod"
spec:
  tls:
  - hosts:
    - mytsdb.example.com
    secretName: mytsdb-tls
  rules:
  - host: mytsdb.example.com
    http:
      paths:
      - path: /
        pathType: Prefix
        backend:
          service:
            name: mytsdb
            port:
              number: 9090
```

### Enable Network Policies

```yaml
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: mytsdb-policy
  namespace: mytsdb
spec:
  podSelector:
    matchLabels:
      app: mytsdb
  policyTypes:
  - Ingress
  ingress:
  - from:
    - podSelector:
        matchLabels:
          app: otel-collector
    - podSelector:
        matchLabels:
          app: grafana
    ports:
    - protocol: TCP
      port: 9090
```

## 9. Quick Reference

### Access URLs

**From Host Machine:**
- Grafana: `http://localhost:3000`
- MyTSDB: `kubectl port-forward -n mytsdb svc/mytsdb 9090:9090` then `http://localhost:9090`

**From Within Cluster:**
- MyTSDB: `http://mytsdb.mytsdb.svc.cluster.local:9090`
- OTEL: `http://otel-collector.mytsdb.svc.cluster.local:4317`
- Grafana: `http://grafana.mytsdb.svc.cluster.local:3000`

### Makefile Commands

```bash
make forward-mytsdb      # Port-forward MyTSDB
make forward-grafana     # Port-forward Grafana
make forward-otel        # Port-forward OTEL
make test-mytsdb-health  # Test MyTSDB health
make test-connectivity   # Test all connectivity
```

## Summary

✅ **Internal Networking:** Fully configured with ClusterIP services
✅ **External Access:** Available via port-forward or optional NodePort
✅ **Port Mappings:** Configured in Kind cluster
✅ **Image Tagging:** `mytsdb:latest` with `imagePullPolicy: Never`
✅ **Health Checks:** Liveness and readiness probes configured
✅ **DNS:** Kubernetes DNS resolution working

All networking is properly configured for both internal cluster communication and external access!
