#!/bin/bash
# 빌드 환경 베이스 이미지 생성 — 최초 1회 또는 vcpkg.json 변경 시 실행
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
IMAGE="kimchi-builder-base:ubuntu20"

echo "=== Building base image (system packages + vcpkg dependencies) ==="
echo "    이 작업은 처음 실행 시 30분 이상 소요될 수 있습니다."
docker build -f "$PROJECT_DIR/Dockerfile.builder" -t "$IMAGE" "$PROJECT_DIR"

echo ""
echo "=== Base image ready: $IMAGE ==="
docker image inspect "$IMAGE" --format "Size: {{.Size}} bytes, Created: {{.Created}}"
