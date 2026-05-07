#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
CMAKE="$PROJECT_DIR/cmake_local/bin/cmake"
CONAN="$PROJECT_DIR/conan/bin/conan"

if [ ! -f "$CMAKE" ]; then
    echo "CMake not found. Running install_cmake.sh..."
    bash "$SCRIPT_DIR/install_cmake.sh"
fi

# Release Conan 패키지 캐시 확인.
# cmake-conan은 build_type=Release 패키지를 Debug와 별도로 관리한다.
# 캐시에 없으면 첫 configure 시 소스에서 전체 재컴파일된다 (일회성 비용).
if "$CONAN" list "proxygen/*:*" 2>/dev/null | grep -q "build_type=Release"; then
    echo "=== Conan Release packages found in cache — configure will be fast ==="
else
    echo "=== [!] Conan Release packages not cached ==="
    echo "    First Release build compiles folly/fizz/wangle/proxygen from source."
    echo "    This takes ~30-60 minutes. Subsequent builds reuse the cache."
    echo ""
fi

echo "=== Configuring Release build ==="
"$CMAKE" -B "$PROJECT_DIR/cmake-build-release" \
         -S "$PROJECT_DIR" \
         -DCMAKE_BUILD_TYPE=Release

echo "=== Building ($(nproc) jobs) ==="
"$CMAKE" --build "$PROJECT_DIR/cmake-build-release" \
         --target kimchi \
         -j"$(nproc)"

mkdir -p "$PROJECT_DIR/dist"
cp "$PROJECT_DIR/cmake-build-release/src/kimchi" "$PROJECT_DIR/dist/kimchi"

echo ""
echo "=== Release binary ready ==="
file "$PROJECT_DIR/dist/kimchi"
echo "Path: $PROJECT_DIR/dist/kimchi"
echo ""
echo "Next Release build will reuse cached Conan packages and be significantly faster."
