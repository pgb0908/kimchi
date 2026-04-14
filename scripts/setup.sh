#!/bin/bash
set -e

CONAN_VERSION="2.9.2"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONAN_DIR="$SCRIPT_DIR/../conan"
CONAN_BIN="$CONAN_DIR/conan"

if [ -f "$CONAN_BIN" ]; then
    echo "Conan already installed: $($CONAN_BIN --version)"
    exit 0
fi

echo "Downloading Conan $CONAN_VERSION for Linux x86_64..."
mkdir -p "$CONAN_DIR"
curl -fL \
    "https://github.com/conan-io/conan/releases/download/$CONAN_VERSION/conan-linux-64" \
    -o "$CONAN_BIN"
chmod +x "$CONAN_BIN"

echo "Done: $($CONAN_BIN --version)"
