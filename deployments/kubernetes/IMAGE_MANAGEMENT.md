# Docker Image Management for Kind

## Problem

Loading locally-built Docker images into Kind clusters can be unreliable due to:
- Images not being properly tagged
- Images not being found after build
- Kind cluster not recognizing loaded images
- Stale/dangling images causing confusion

## Solution

The `manage-images.sh` script provides robust image management with:
- ✅ Build verification
- ✅ Image existence checks (local and in-cluster)
- ✅ Proper tagging
- ✅ Force reload capability
- ✅ Dangling image cleanup
- ✅ Detailed logging

## Usage

### Via Script

```bash
# Build images
./manage-images.sh build-all

# Load into Kind
./manage-images.sh load-all

# Ensure images are built and loaded (recommended)
./manage-images.sh ensure-all

# Verify images in cluster
./manage-images.sh verify

# Clean old images
./manage-images.sh clean

# Force reload if having issues
./manage-images.sh force-reload-mytsdb
```

### Via Makefile

```bash
# Build images
make build-all

# Load into Kind
make load-all

# Ensure images (build + load)
make ensure-images

# Verify images
make verify-images

# Clean old images
make clean-images

# Update with force reload
make update-mytsdb
```

## How It Works

### 1. Build with Verification

```bash
./manage-images.sh build-mytsdb
```

- Builds image with explicit tag
- Verifies image exists after build
- Shows image ID and size
- Fails fast if build unsuccessful

### 2. Load with Verification

```bash
./manage-images.sh load-mytsdb
```

- Checks cluster exists
- Checks image exists locally
- Loads into Kind
- Verifies image in cluster (with grace period)
- Shows cluster image details

### 3. Ensure (Recommended)

```bash
./manage-images.sh ensure-all
```

- Checks if image exists locally
- Asks if you want to rebuild (if exists)
- Builds if doesn't exist
- Loads into Kind
- Verifies everything

### 4. Force Reload

```bash
./manage-images.sh force-reload-mytsdb
```

- Removes image from Kind cluster
- Reloads fresh copy
- Useful when image seems cached incorrectly

## Troubleshooting

### Image not found after build

```bash
# Verify image exists
docker images | grep mytsdb

# If not found, rebuild
./manage-images.sh build-mytsdb
```

### Image not loading into Kind

```bash
# Check cluster exists
kind get clusters

# Force reload
./manage-images.sh force-reload-mytsdb

# Verify
./manage-images.sh verify
```

### Old/stale images

```bash
# Clean dangling images
./manage-images.sh clean

# Or via Docker
docker image prune -f
```

### Pod shows ImagePullBackOff

This usually means:
1. Image not loaded into Kind
2. Image name mismatch in deployment YAML

Fix:
```bash
# Verify image name matches
kubectl describe pod <pod-name> -n mytsdb | grep Image

# Reload image
./manage-images.sh force-reload-mytsdb

# Restart deployment
kubectl rollout restart deployment/mytsdb -n mytsdb
```

## Best Practices

### 1. Always Use ensure-images

```bash
make ensure-images
```

This is the most reliable way to ensure images are built and loaded.

### 2. Verify After Loading

```bash
make load-all
make verify-images
```

### 3. Clean Regularly

```bash
make clean-images
```

Removes dangling images that can cause confusion.

### 4. Use Force Reload for Updates

```bash
make update-mytsdb  # Uses force-reload internally
```

This ensures the latest image is used.

## Image Verification

The script checks images in two places:

### 1. Local Docker

```bash
docker images --format "{{.Repository}}:{{.Tag}}" | grep "mytsdb:latest"
```

### 2. Kind Cluster

```bash
docker exec mytsdb-cluster-control-plane crictl images | grep "mytsdb:latest"
```

## Common Workflows

### Fresh Development

```bash
# 1. Build and load
make ensure-images

# 2. Deploy
make deploy-all

# 3. Verify
make status
```

### After Code Changes

```bash
# Update MyTSDB
make update-mytsdb

# This does:
# - build-mytsdb
# - force-reload-mytsdb
# - restart deployment
```

### Debugging Image Issues

```bash
# 1. Check what's in Docker
docker images | grep -E "mytsdb|sample-app"

# 2. Check what's in Kind
./manage-images.sh verify

# 3. Force reload
./manage-images.sh force-reload-all

# 4. Restart pods
make restart-all
```

## Script Commands

```
build-mytsdb           Build MyTSDB image
build-sample-app       Build sample app image
build-all              Build all images

load-mytsdb            Load MyTSDB into Kind
load-sample-app        Load sample app into Kind
load-all               Load all images into Kind

ensure-mytsdb          Ensure MyTSDB is built and loaded
ensure-sample-app      Ensure sample app is built and loaded
ensure-all             Ensure all images are built and loaded

verify                 Verify images in cluster
clean                  Clean old/dangling images

force-reload-mytsdb    Force reload MyTSDB image
force-reload-sample-app Force reload sample app image
```

## Integration with Makefile

The Makefile uses `manage-images.sh` for all image operations:

- `make build-*` → `./manage-images.sh build-*`
- `make load-*` → `./manage-images.sh load-*`
- `make update-*` → Uses `force-reload-*` for reliability

This ensures consistent, reliable image management across all operations.
