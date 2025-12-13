#!/bin/bash
# Complete rebuild and redeploy script for MyTSDB
set -e

echo "=== Step 1: Cleanup Docker ==="
docker system prune -f
docker builder prune -a -f

echo ""
echo "=== Step 2: Delete existing Kind cluster ==="
kind delete cluster --name mytsdb-cluster 2>/dev/null || echo "Cluster doesn't exist"

echo ""
echo "=== Step 3: Create new Kind cluster ==="
kind create cluster --config kind/cluster-config.yaml --name mytsdb-cluster

echo ""
echo "=== Step 4: Build MyTSDB image ==="
make build-mytsdb

echo ""
echo "=== Step 5: Load image into Kind ==="
make load-mytsdb

echo ""
echo "=== Step 6: Deploy all components ==="
make deploy-all

echo ""
echo "=== Done! ==="
echo "Check status with: kubectl get pods -n mytsdb"

