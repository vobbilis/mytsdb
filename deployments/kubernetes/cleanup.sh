#!/bin/bash
set -e

echo "=== Cleaning up MyTSDB Kubernetes Deployment ==="
echo ""

# Delete namespace (this will delete all resources in it)
echo "Deleting namespace and all resources..."
kubectl delete namespace mytsdb --ignore-not-found=true

echo "Waiting for namespace deletion..."
kubectl wait --for=delete namespace/mytsdb --timeout=60s 2>/dev/null || true

# Delete Kind cluster
echo "Deleting Kind cluster..."
kind delete cluster --name mytsdb-cluster

echo ""
echo "✓ Cleanup complete!"
echo ""
