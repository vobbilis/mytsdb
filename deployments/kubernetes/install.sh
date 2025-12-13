#!/bin/bash
# Fresh install script for MyTSDB Kubernetes deployment

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  MyTSDB Kubernetes Fresh Install          ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════╝${NC}"
echo ""

# Parse arguments
SKIP_PREFLIGHT=false
FORCE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-preflight)
            SKIP_PREFLIGHT=true
            shift
            ;;
        --force)
            FORCE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --skip-preflight    Skip pre-flight checks"
            echo "  --force             Force reinstall (delete existing cluster)"
            echo "  -h, --help          Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Step 0: Pre-flight checks
if [ "$SKIP_PREFLIGHT" = false ]; then
    echo -e "${CYAN}[0/7]${NC} Running pre-flight checks..."
    echo ""
    
    if ! ./preflight.sh; then
        echo ""
        echo -e "${RED}Pre-flight checks failed!${NC}"
        echo "Fix the errors above or use --skip-preflight to continue anyway"
        exit 1
    fi
    echo ""
else
    echo -e "${YELLOW}⚠ Skipping pre-flight checks${NC}"
    echo ""
fi

# Check for existing cluster
if kind get clusters 2>/dev/null | grep -q "mytsdb-cluster"; then
    if [ "$FORCE" = true ]; then
        echo -e "${YELLOW}⚠ Existing cluster found, deleting...${NC}"
        kind delete cluster --name mytsdb-cluster
        echo ""
    else
        echo -e "${YELLOW}⚠ Cluster 'mytsdb-cluster' already exists${NC}"
        read -p "Delete and recreate? (y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            kind delete cluster --name mytsdb-cluster
        else
            echo "Aborting installation"
            exit 1
        fi
        echo ""
    fi
fi

# Step 1: Create Kind cluster
echo -e "${CYAN}[1/7]${NC} Creating Kind cluster..."
kind create cluster --config kind/cluster-config.yaml
echo -e "${GREEN}✓${NC} Kind cluster created"
echo ""

# Step 2: Build Docker images
echo -e "${CYAN}[2/7]${NC} Building Docker images..."
echo ""

./manage-images.sh build-all

echo -e "${GREEN}✓${NC} All images built"
echo ""

# Step 3: Load images into Kind
echo -e "${CYAN}[3/7]${NC} Loading images into Kind cluster..."

./manage-images.sh load-all

echo -e "${GREEN}✓${NC} Images loaded into cluster"
echo ""

# Verify images
echo "  Verifying images in cluster..."
./manage-images.sh verify
echo ""

# Step 4: Deploy MyTSDB
echo -e "${CYAN}[4/7]${NC} Deploying MyTSDB..."
kubectl apply -f mytsdb/00-namespace.yaml
kubectl apply -f mytsdb/02-deployment.yaml
kubectl apply -f mytsdb/03-service.yaml

echo "  Waiting for MyTSDB to be ready..."
kubectl wait --for=condition=ready pod -l app=mytsdb -n mytsdb --timeout=900s 2>&1 | grep -v "condition met" || true
echo -e "${GREEN}✓${NC} MyTSDB deployed and ready"
echo ""

# Step 5: Deploy OTEL Collector
echo -e "${CYAN}[5/7]${NC} Deploying OTEL Collector..."
kubectl apply -f otel-collector/00-configmap.yaml
kubectl apply -f otel-collector/01-deployment.yaml
kubectl apply -f otel-collector/02-service.yaml

echo "  Waiting for OTEL Collector to be ready..."
kubectl wait --for=condition=ready pod -l app=otel-collector -n mytsdb --timeout=60s 2>&1 | grep -v "condition met" || true
echo -e "${GREEN}✓${NC} OTEL Collector deployed and ready"
echo ""

# Step 6: Deploy Sample Application
echo -e "${CYAN}[6/7]${NC} Deploying Sample Application..."
kubectl apply -f sample-app/00-deployment.yaml

echo "  Waiting for Sample App to be ready..."
kubectl wait --for=condition=ready pod -l app=sample-app -n mytsdb --timeout=60s 2>&1 | grep -v "condition met" || true
echo -e "${GREEN}✓${NC} Sample Application deployed and ready"
echo ""

# Step 7: Deploy Grafana
echo -e "${CYAN}[7/7]${NC} Deploying Grafana..."
kubectl apply -f grafana/00-datasource-configmap.yaml
kubectl apply -f grafana/01-dashboard-configmap.yaml
kubectl apply -f grafana/04-comprehensive-dashboards.yaml
kubectl apply -f grafana/02-deployment.yaml
kubectl apply -f grafana/03-service.yaml

echo "  Waiting for Grafana to be ready..."
kubectl wait --for=condition=ready pod -l app=grafana -n mytsdb --timeout=120s 2>&1 | grep -v "condition met" || true
echo -e "${GREEN}✓${NC} Grafana deployed and ready"
echo ""

# Final summary
echo -e "${GREEN}╔════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Installation Complete! 🎉                 ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}Access Information:${NC}"
echo "  📊 Grafana:  ${CYAN}http://localhost:13000${NC}"
echo "               Username: ${YELLOW}admin${NC}"
echo "               Password: ${YELLOW}admin${NC}"
echo ""
echo -e "${BLUE}Useful Commands:${NC}"
echo "  View all pods:        ${CYAN}kubectl get pods -n mytsdb${NC}"
echo "  View MyTSDB logs:     ${CYAN}kubectl logs -f -l app=mytsdb -n mytsdb${NC}"
echo "  View OTEL logs:       ${CYAN}kubectl logs -f -l app=otel-collector -n mytsdb${NC}"
echo "  View sample app logs: ${CYAN}kubectl logs -f -l app=sample-app -n mytsdb${NC}"
echo ""
echo -e "${BLUE}Makefile Targets:${NC}"
echo "  Update MyTSDB:        ${CYAN}make update-mytsdb${NC}"
echo "  Restart services:     ${CYAN}make restart-<service>${NC}"
echo "  View logs:            ${CYAN}make logs-<service>${NC}"
echo "  Cleanup:              ${CYAN}make clean${NC}"
echo ""
echo -e "${YELLOW}Next Steps:${NC}"
echo "  1. Open Grafana at http://localhost:13000"
echo "  2. View the pre-configured dashboard"
echo "  3. Explore live metrics from the sample app"
echo ""
