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
mkdir -p "$PROJECT_DIR/dist/lib"
docker run --rm --entrypoint cat "$IMAGE" /usr/local/bin/kimchi > "$OUTPUT"
chmod +x "$OUTPUT"

echo "=== Extracting libstdc++ ==="
LIBSTDCXX=$(docker run --rm --entrypoint sh "$IMAGE" -c "ls /usr/lib/x86_64-linux-gnu/libstdc++.so.6.*")
for lib in $LIBSTDCXX; do
    libname=$(basename "$lib")
    docker run --rm --entrypoint cat "$IMAGE" "$lib" > "$PROJECT_DIR/dist/lib/$libname"
done
# 심볼릭 링크 생성 (libstdc++.so.6 → libstdc++.so.6.x.y)
VERSIONED=$(ls "$PROJECT_DIR/dist/lib/libstdc++.so.6."* 2>/dev/null | sort -V | tail -1)
if [ -n "$VERSIONED" ]; then
    ln -sf "$(basename "$VERSIONED")" "$PROJECT_DIR/dist/lib/libstdc++.so.6"
fi

echo ""
echo "=== Binary ready ==="
file "$OUTPUT"
echo "Path: $OUTPUT"
echo "Libs: $PROJECT_DIR/dist/lib/"
