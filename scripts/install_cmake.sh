#!/bin/bash
set -e

CMAKE_VERSION="3.31.7"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CMAKE_DIR="$SCRIPT_DIR/../cmake_local"
CMAKE_BIN="$CMAKE_DIR/bin/cmake"

if [ -f "$CMAKE_BIN" ]; then
    echo "CMake already installed: $($CMAKE_BIN --version | head -1)"
    exit 0
fi

ARCH="$(uname -m)"
case "$ARCH" in
    x86_64)  ARCH_SUFFIX="x86_64" ;;
    aarch64) ARCH_SUFFIX="aarch64" ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

echo "Downloading CMake $CMAKE_VERSION for Linux $ARCH_SUFFIX..."
mkdir -p "$CMAKE_DIR"
TMP_TGZ="$(mktemp /tmp/cmake-XXXXXX.tar.gz)"
curl -fL \
    "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${ARCH_SUFFIX}.tar.gz" \
    -o "$TMP_TGZ"
tar -xzf "$TMP_TGZ" --strip-components=1 -C "$CMAKE_DIR"
rm -f "$TMP_TGZ"

echo "Done: $($CMAKE_BIN --version | head -1)"
echo ""
echo "To use this CMake, add to PATH:"
echo "  export PATH=\"$CMAKE_DIR/bin:\$PATH\""
