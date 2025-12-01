#!/bin/bash
set -e

echo "=== MyTSDB Kubernetes Deployment ==="
echo ""

# Colors
GREEN='\033[0.32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check prerequisites
echo "Checking prerequisites..."
command -v docker >/dev/null 2>&1 || { echo "Error: docker is required but not installed."; exit 1; }
command -v kind >/dev/null 2>&1 || { echo "Error: kind is required but not installed."; exit 1; }
command -v kubectl >/dev/null 2>&1 || { echo "Error: kubectl is required but not installed."; exit 1; }

echo -e "${GREEN}✓${NC} All prerequisites met"
echo ""

# Step 1: Create Kind cluster
echo "Step 1: Creating Kind cluster..."
if kind get clusters | grep -q "mytsdb-cluster"; then
    echo -e "${YELLOW}⚠${NC} Cluster 'mytsdb-cluster' already exists"
    read -p "Delete and recreate? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        kind delete cluster --name mytsdb-cluster
        kind create cluster --config kind/cluster-config.yaml
    fi
else
    kind create cluster --config kind/cluster-config.yaml
fi

echo -e "${GREEN}✓${NC} Kind cluster ready"
echo ""

# Step 2: Build Docker images
echo "Step 2: Building Docker images..."

echo "Building MyTSDB image..."
cd ../../../
docker build -t mytsdb:latest -f deployments/kubernetes/mytsdb/Dockerfile .
kind load docker-image mytsdb:latest --name mytsdb-cluster

echo "Building sample app image..."
cd deployments/kubernetes/sample-app
docker build -t sample-app:latest .
kind load docker-image sample-app:latest --name mytsdb-cluster

cd ../

echo -e "${GREEN}✓${NC} Docker images built and loaded"
echo ""

# Step 3: Deploy MyTSDB
echo "Step 3: Deploying MyTSDB..."
kubectl apply -f mytsdb/00-namespace.yaml
kubectl apply -f mytsdb/01-pvc.yaml
kubectl apply -f mytsdb/02-deployment.yaml
kubectl apply -f mytsdb/03-service.yaml

echo "Waiting for MyTSDB to be ready..."
kubectl wait --for=condition=ready pod -l app=mytsdb -n mytsdb --timeout=120s

echo -e "${GREEN}✓${NC} MyTSDB deployed"
echo ""

# Step 4: Deploy OTEL Collector
echo "Step 4: Deploying OTEL Collector..."
kubectl apply -f otel-collector/00-configmap.yaml
kubectl apply -f otel-collector/01-deployment.yaml
kubectl apply -f otel-collector/02-service.yaml

echo "Waiting for OTEL Collector to be ready..."
kubectl wait --for=condition=ready pod -l app=otel-collector -n mytsdb --timeout=60s

echo -e "${GREEN}✓${NC} OTEL Collector deployed"
echo ""

# Step 5: Deploy Sample Application
echo "Step 5: Deploying Sample Application..."
kubectl apply -f sample-app/00-deployment.yaml

echo "Waiting for Sample App to be ready..."
kubectl wait --for=condition=ready pod -l app=sample-app -n mytsdb --timeout=60s

echo -e "${GREEN}✓${NC} Sample Application deployed"
echo ""

# Step 6: Deploy Grafana
echo "Step 6: Deploying Grafana..."
kubectl apply -f grafana/00-datasource-configmap.yaml
kubectl apply -f grafana/01-dashboard-configmap.yaml
kubectl apply -f grafana/02-deployment.yaml
kubectl apply -f grafana/03-service.yaml

echo "Waiting for Grafana to be ready..."
kubectl wait --for=condition=ready pod -l app=grafana -n mytsdb --timeout=120s

echo -e "${GREEN}✓${NC} Grafana deployed"
echo ""

# Summary
echo "=== Deployment Complete! ==="
echo ""
echo "Access URLs:"
echo "  Grafana:  http://localhost:3000"
echo "            Username: admin"
echo "            Password: admin"
echo ""
echo "Useful commands:"
echo "  View all pods:        kubectl get pods -n mytsdb"
echo "  View MyTSDB logs:     kubectl logs -f -l app=mytsdb -n mytsdb"
echo "  View OTEL logs:       kubectl logs -f -l app=otel-collector -n mytsdb"
echo "  View sample app logs: kubectl logs -f -l app=sample-app -n mytsdb"
echo ""
echo "To clean up:"
echo "  ./cleanup.sh"
echo ""
