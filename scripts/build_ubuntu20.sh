#!/bin/bash
# Ubuntu 20.04 Docker 빌드 — glibc 2.31 호환 바이너리 생성
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
IMAGE="kimchi-builder:ubuntu20"
OUTPUT="$PROJECT_DIR/dist/kimchi-ubuntu20"

echo "=== Building Docker image (Ubuntu 20.04) ==="
docker build -f "$PROJECT_DIR/Dockerfile.build" -t "$IMAGE" "$PROJECT_DIR"

echo "=== Extracting binary ==="
mkdir -p "$PROJECT_DIR/dist"
docker run --rm "$IMAGE" cat /usr/local/bin/kimchi > "$OUTPUT"
chmod +x "$OUTPUT"

echo ""
echo "=== Binary ready ==="
file "$OUTPUT"
echo "Path: $OUTPUT"
