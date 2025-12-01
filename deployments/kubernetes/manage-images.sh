#!/bin/bash
# Robust Docker image management for Kind cluster

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

CLUSTER_NAME="${CLUSTER_NAME:-mytsdb-cluster}"

# Function to check if image exists locally
image_exists_locally() {
    local image=$1
    docker images --format "{{.Repository}}:{{.Tag}}" | grep -q "^${image}$"
}

# Function to check if image exists in Kind cluster
image_exists_in_kind() {
    local image=$1
    docker exec ${CLUSTER_NAME}-control-plane crictl images | grep -q "${image}" 2>/dev/null
}

# Function to build image with proper tagging
build_image() {
    local dockerfile=$1
    local context=$2
    local image_name=$3
    local build_args=$4
    
    echo -e "${BLUE}Building ${image_name}...${NC}"
    
    # Build with explicit tag
    if [ -n "$build_args" ]; then
        docker build $build_args -t "${image_name}" -f "${dockerfile}" "${context}"
    else
        docker build -t "${image_name}" -f "${dockerfile}" "${context}"
    fi
    
    # Verify build succeeded
    if ! image_exists_locally "${image_name}"; then
        echo -e "${RED}✗ Failed to build ${image_name}${NC}"
        return 1
    fi
    
    echo -e "${GREEN}✓ Built ${image_name}${NC}"
    
    # Show image details
    local image_id=$(docker images --format "{{.ID}}" "${image_name}" | head -1)
    local image_size=$(docker images --format "{{.Size}}" "${image_name}" | head -1)
    echo -e "  ${CYAN}Image ID: ${image_id}, Size: ${image_size}${NC}"
    
    return 0
}

# Function to load image into Kind with verification
load_image_to_kind() {
    local image=$1
    
    echo -e "${BLUE}Loading ${image} into Kind cluster...${NC}"
    
    # Check if cluster exists
    if ! kind get clusters 2>/dev/null | grep -q "^${CLUSTER_NAME}$"; then
        echo -e "${RED}✗ Cluster '${CLUSTER_NAME}' not found${NC}"
        echo -e "  ${YELLOW}Run 'make cluster-create' first${NC}"
        return 1
    fi
    
    # Check if image exists locally
    if ! image_exists_locally "${image}"; then
        echo -e "${RED}✗ Image ${image} not found locally${NC}"
        echo -e "  ${YELLOW}Build the image first${NC}"
        return 1
    fi
    
    # Load image into Kind
    kind load docker-image "${image}" --name "${CLUSTER_NAME}"
    
    # Verify image was loaded
    sleep 2  # Give Kind time to register the image
    
    if image_exists_in_kind "${image}"; then
        echo -e "${GREEN}✓ Loaded ${image} into Kind cluster${NC}"
        
        # Show image in cluster
        local cluster_images=$(docker exec ${CLUSTER_NAME}-control-plane crictl images | grep "${image}" || echo "")
        if [ -n "$cluster_images" ]; then
            echo -e "  ${CYAN}Cluster image:${NC}"
            echo "$cluster_images" | sed 's/^/    /'
        fi
        return 0
    else
        echo -e "${YELLOW}⚠ Image loaded but not immediately visible in cluster${NC}"
        echo -e "  ${YELLOW}This is normal - image will be available when pod starts${NC}"
        return 0
    fi
}

# Function to pull remote image
pull_remote_image() {
    local image=$1
    
    echo -e "${BLUE}Pulling ${image} from registry...${NC}"
    
    if docker pull "${image}"; then
        echo -e "${GREEN}✓ Pulled ${image}${NC}"
        return 0
    else
        echo -e "${RED}✗ Failed to pull ${image}${NC}"
        return 1
    fi
}

# Function to ensure image is available (build or pull)
ensure_image() {
    local image=$1
    local dockerfile=$2
    local context=$3
    local prefer_local=${4:-true}
    
    echo -e "${CYAN}=== Ensuring ${image} is available ===${NC}"
    
    # Check if image already exists locally
    if image_exists_locally "${image}"; then
        echo -e "${GREEN}✓ Image ${image} already exists locally${NC}"
        
        # Ask if user wants to rebuild
        if [ "$prefer_local" = "true" ]; then
            read -p "Rebuild? (y/N) " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                build_image "${dockerfile}" "${context}" "${image}"
            fi
        fi
    else
        # Image doesn't exist, build it
        if [ -n "$dockerfile" ] && [ -n "$context" ]; then
            echo -e "${YELLOW}Image not found locally, building...${NC}"
            build_image "${dockerfile}" "${context}" "${image}"
        else
            echo -e "${YELLOW}No build context provided, trying to pull...${NC}"
            pull_remote_image "${image}"
        fi
    fi
    
    # Load into Kind
    load_image_to_kind "${image}"
}

# Function to clean old images
clean_old_images() {
    local image_name=$1
    
    echo -e "${BLUE}Cleaning old ${image_name} images...${NC}"
    
    # Remove dangling images
    local dangling=$(docker images -f "dangling=true" -q)
    if [ -n "$dangling" ]; then
        docker rmi $dangling 2>/dev/null || true
        echo -e "${GREEN}✓ Removed dangling images${NC}"
    else
        echo -e "${CYAN}No dangling images to remove${NC}"
    fi
}

# Function to verify all images in cluster
verify_cluster_images() {
    echo -e "${BLUE}Verifying images in cluster...${NC}"
    
    local required_images=("mytsdb:latest" "sample-app:latest")
    local all_ok=true
    
    for image in "${required_images[@]}"; do
        if image_exists_in_kind "${image}"; then
            echo -e "${GREEN}✓${NC} ${image}"
        else
            echo -e "${YELLOW}⚠${NC} ${image} (will be pulled when pod starts)"
            all_ok=false
        fi
    done
    
    if [ "$all_ok" = true ]; then
        echo -e "${GREEN}✓ All required images verified${NC}"
    else
        echo -e "${YELLOW}⚠ Some images not visible (this is normal)${NC}"
    fi
}

# Function to force reload image
force_reload_image() {
    local image=$1
    
    echo -e "${YELLOW}Force reloading ${image}...${NC}"
    
    # Remove from Kind cluster
    docker exec ${CLUSTER_NAME}-control-plane crictl rmi "${image}" 2>/dev/null || true
    
    # Reload
    load_image_to_kind "${image}"
}

# Main command dispatcher
case "${1:-help}" in
    build-mytsdb)
        cd ../..
        build_image "deployments/kubernetes/mytsdb/Dockerfile" "." "mytsdb:latest" "${DOCKER_BUILD_ARGS}"
        ;;
    
    build-sample-app)
        cd sample-app
        build_image "Dockerfile" "." "sample-app:latest"
        ;;
    
    build-all)
        cd ../..
        build_image "deployments/kubernetes/mytsdb/Dockerfile" "." "mytsdb:latest" "${DOCKER_BUILD_ARGS}"
        cd deployments/kubernetes/sample-app
        build_image "Dockerfile" "." "sample-app:latest"
        ;;
    
    load-mytsdb)
        load_image_to_kind "mytsdb:latest"
        ;;
    
    load-sample-app)
        load_image_to_kind "sample-app:latest"
        ;;
    
    load-all)
        load_image_to_kind "mytsdb:latest"
        load_image_to_kind "sample-app:latest"
        ;;
    
    ensure-mytsdb)
        cd ../..
        ensure_image "mytsdb:latest" "deployments/kubernetes/mytsdb/Dockerfile" "." "${2:-true}"
        ;;
    
    ensure-sample-app)
        cd sample-app
        ensure_image "sample-app:latest" "Dockerfile" "." "${2:-true}"
        ;;
    
    ensure-all)
        cd ../..
        ensure_image "mytsdb:latest" "deployments/kubernetes/mytsdb/Dockerfile" "." "${2:-true}"
        cd deployments/kubernetes/sample-app
        ensure_image "sample-app:latest" "Dockerfile" "." "${2:-true}"
        ;;
    
    verify)
        verify_cluster_images
        ;;
    
    clean)
        clean_old_images "mytsdb"
        clean_old_images "sample-app"
        ;;
    
    force-reload-mytsdb)
        force_reload_image "mytsdb:latest"
        ;;
    
    force-reload-sample-app)
        force_reload_image "sample-app:latest"
        ;;
    
    help|*)
        echo "Usage: $0 <command>"
        echo ""
        echo "Commands:"
        echo "  build-mytsdb           Build MyTSDB image"
        echo "  build-sample-app       Build sample app image"
        echo "  build-all              Build all images"
        echo ""
        echo "  load-mytsdb            Load MyTSDB into Kind"
        echo "  load-sample-app        Load sample app into Kind"
        echo "  load-all               Load all images into Kind"
        echo ""
        echo "  ensure-mytsdb          Ensure MyTSDB is built and loaded"
        echo "  ensure-sample-app      Ensure sample app is built and loaded"
        echo "  ensure-all             Ensure all images are built and loaded"
        echo ""
        echo "  verify                 Verify images in cluster"
        echo "  clean                  Clean old/dangling images"
        echo ""
        echo "  force-reload-mytsdb    Force reload MyTSDB image"
        echo "  force-reload-sample-app Force reload sample app image"
        echo ""
        echo "Examples:"
        echo "  $0 build-all           # Build all images"
        echo "  $0 load-all            # Load all images into Kind"
        echo "  $0 ensure-all          # Build and load all (recommended)"
        echo "  $0 verify              # Verify images are in cluster"
        ;;
esac
