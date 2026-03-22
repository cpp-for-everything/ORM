#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

echo "=========================================="
echo "ORM Docker Test (System Dependencies)"
echo "=========================================="
echo ""
echo "Building Docker image with system-level dependencies..."
echo "This may take 15-20 minutes on first build, then cached."
echo ""

cd "${PROJECT_ROOT}"

# Build and test the ORM library with system-level dependencies
docker compose -f docker/docker-compose.yml build test-all

echo ""
echo "Running tests..."
docker compose -f docker/docker-compose.yml run --rm test-all

echo ""
echo "Cleaning up..."
docker compose -f docker/docker-compose.yml down

echo ""
echo "=========================================="
echo "Tests completed!"
echo "=========================================="
