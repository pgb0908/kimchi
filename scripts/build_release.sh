#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

if [ -z "$VCPKG_ROOT" ]; then
    echo "Error: VCPKG_ROOT is not set. Run scripts/install_vcpkg.sh for setup instructions."
    exit 1
fi

echo "=== Configuring Release build ==="
cmake --preset kimchi-release -S "$PROJECT_DIR"

echo "=== Building ($(nproc) jobs) ==="
cmake --build --preset release

mkdir -p "$PROJECT_DIR/dist"
cp "$PROJECT_DIR/cmake-build-release/src/kimchi" "$PROJECT_DIR/dist/kimchi"

echo ""
echo "=== Release binary ready ==="
file "$PROJECT_DIR/dist/kimchi"
echo "Path: $PROJECT_DIR/dist/kimchi"
