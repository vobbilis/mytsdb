#!/bin/bash
# Pre-flight check for MyTSDB Kubernetes deployment

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== MyTSDB Kubernetes Pre-flight Check ===${NC}"
echo ""

ERRORS=0
WARNINGS=0

# Function to check command exists
check_command() {
    local cmd=$1
    local required=$2
    local install_hint=$3
    
    if command -v "$cmd" >/dev/null 2>&1; then
        local version=""
        if [ "$cmd" = "kubectl" ]; then
            version=$($cmd version --client --short 2>/dev/null || $cmd version --client 2>/dev/null | head -n1)
        else
            version=$($cmd --version 2>&1 | head -n1)
        fi
        echo -e "${GREEN}✓${NC} $cmd is installed: $version"
        return 0
    else
        if [ "$required" = "true" ]; then
            echo -e "${RED}✗${NC} $cmd is NOT installed (required)"
            echo -e "  ${YELLOW}Install:${NC} $install_hint"
            ((ERRORS++))
        else
            echo -e "${YELLOW}⚠${NC} $cmd is NOT installed (optional)"
            echo -e "  ${YELLOW}Install:${NC} $install_hint"
            ((WARNINGS++))
        fi
        return 1
    fi
}

# Function to check Docker is running
check_docker_running() {
    if docker info >/dev/null 2>&1; then
        echo -e "${GREEN}✓${NC} Docker daemon is running"
        return 0
    else
        echo -e "${RED}✗${NC} Docker daemon is NOT running"
        echo -e "  ${YELLOW}Fix:${NC} Start Docker Desktop or run 'sudo systemctl start docker'"
        ((ERRORS++))
        return 1
    fi
}

# Function to check port availability
check_port() {
    local port=$1
    local service=$2
    
    if lsof -Pi :$port -sTCP:LISTEN -t >/dev/null 2>&1; then
        echo -e "${YELLOW}⚠${NC} Port $port is already in use (needed for $service)"
        local pid=$(lsof -Pi :$port -sTCP:LISTEN -t)
        local process=$(ps -p $pid -o comm= 2>/dev/null || echo "unknown")
        echo -e "  ${YELLOW}Process:${NC} $process (PID: $pid)"
        echo -e "  ${YELLOW}Fix:${NC} Kill process with 'kill $pid' or use different port"
        ((WARNINGS++))
        return 1
    else
        echo -e "${GREEN}✓${NC} Port $port is available ($service)"
        return 0
    fi
}

# Function to check disk space
check_disk_space() {
    local required_gb=$1
    local available_gb=0
    
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS (uses -g for gigabytes)
        available_gb=$(df -g . | tail -1 | awk '{print $4}')
    else
        # Linux (uses -BG for gigabytes)
        available_gb=$(df -BG . | tail -1 | awk '{print $4}' | sed 's/G//')
    fi
    
    if [ "$available_gb" -ge "$required_gb" ]; then
        echo -e "${GREEN}✓${NC} Sufficient disk space: ${available_gb}GB available (${required_gb}GB required)"
        return 0
    else
        echo -e "${RED}✗${NC} Insufficient disk space: ${available_gb}GB available (${required_gb}GB required)"
        ((ERRORS++))
        return 1
    fi
}

# Function to check memory
check_memory() {
    local required_gb=$1
    
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        local total_mb=$(sysctl -n hw.memsize | awk '{print $1/1024/1024}')
        local total_gb=$((total_mb / 1024))
    else
        # Linux
        local total_gb=$(free -g | awk '/^Mem:/{print $2}')
    fi
    
    if [ "$total_gb" -ge "$required_gb" ]; then
        echo -e "${GREEN}✓${NC} Sufficient memory: ${total_gb}GB total (${required_gb}GB recommended)"
        return 0
    else
        echo -e "${YELLOW}⚠${NC} Low memory: ${total_gb}GB total (${required_gb}GB recommended)"
        echo -e "  ${YELLOW}Note:${NC} Deployment may be slow or fail"
        ((WARNINGS++))
        return 1
    fi
}

# Function to check Kind cluster
check_kind_cluster() {
    if kind get clusters 2>/dev/null | grep -q "mytsdb-cluster"; then
        echo -e "${YELLOW}⚠${NC} Kind cluster 'mytsdb-cluster' already exists"
        echo -e "  ${YELLOW}Note:${NC} Will be recreated during deployment"
        return 0
    else
        echo -e "${GREEN}✓${NC} No existing 'mytsdb-cluster' found (clean slate)"
        return 0
    fi
}

echo "Checking required tools..."
echo ""

# Check required commands
check_command "docker" "true" "https://docs.docker.com/get-docker/"
check_command "kind" "true" "brew install kind (macOS) or https://kind.sigs.k8s.io/docs/user/quick-start/#installation"
check_command "kubectl" "true" "brew install kubectl (macOS) or https://kubernetes.io/docs/tasks/tools/"

echo ""
echo "Checking optional tools..."
echo ""

check_command "make" "false" "brew install make (macOS) or apt-get install build-essential (Linux)"
check_command "curl" "false" "brew install curl (macOS) or apt-get install curl (Linux)"

echo ""
echo "Checking Docker..."
echo ""

check_docker_running

echo ""
echo "Checking system resources..."
echo ""

check_disk_space 10  # 10GB required
check_memory 4       # 4GB recommended

echo ""
echo "Checking port availability..."
echo ""

check_port 3000 "Grafana"
check_port 9090 "MyTSDB (optional direct access)"

echo ""
echo "Checking existing deployments..."
echo ""

check_kind_cluster

echo ""
echo -e "${BLUE}=== Summary ===${NC}"
echo ""

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    echo -e "${GREEN}✓ Ready to deploy MyTSDB${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Run: ./install.sh"
    echo "  2. Or: make install"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠ $WARNINGS warning(s) found${NC}"
    echo -e "${GREEN}✓ No critical errors${NC}"
    echo ""
    echo "You can proceed with deployment, but review warnings above."
    echo ""
    echo "Next steps:"
    echo "  1. Run: ./install.sh"
    echo "  2. Or: make install"
    exit 0
else
    echo -e "${RED}✗ $ERRORS error(s) found${NC}"
    if [ $WARNINGS -gt 0 ]; then
        echo -e "${YELLOW}⚠ $WARNINGS warning(s) found${NC}"
    fi
    echo ""
    echo "Please fix the errors above before proceeding."
    exit 1
fi
