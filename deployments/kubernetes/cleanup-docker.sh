#!/bin/bash
# Docker cleanup script

echo "=== Docker Disk Usage ==="
docker system df

echo ""
echo "=== Cleaning up Docker ==="
echo "Removing stopped containers..."
docker container prune -f

echo "Removing unused images..."
docker image prune -a -f

echo "Removing unused volumes..."
docker volume prune -f

echo "Removing build cache..."
docker builder prune -a -f

echo ""
echo "=== Docker Disk Usage After Cleanup ==="
docker system df
