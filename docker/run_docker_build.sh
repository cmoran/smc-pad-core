#!/usr/bin/env bash
# Helper script to build the SMC-Pad firmware using Docker.
# Execute this script or run the two docker commands manually:
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

IMAGE_NAME="smc-pad-builder"

echo "=== Building Docker Build Container Image: ${IMAGE_NAME} ==="
docker build -t "${IMAGE_NAME}" -f docker/Dockerfile .

echo "=== Compiling SMC-Pad Target Firmware inside Docker ==="
docker run --rm -v "${ROOT_DIR}:/work" "${IMAGE_NAME}" make firmware

echo "=== Build Complete: Check build/ for firmware output ==="
