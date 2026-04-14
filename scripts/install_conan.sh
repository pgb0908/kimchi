#!/bin/bash
set -e

CONAN_VERSION="2.9.2"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONAN_DIR="$SCRIPT_DIR/../conan"
CONAN_BIN="$CONAN_DIR/bin/conan"

if [ -f "$CONAN_BIN" ]; then
    echo "Conan already installed: $($CONAN_BIN --version)"
    exit 0
fi

echo "Downloading Conan $CONAN_VERSION for Linux x86_64..."
mkdir -p "$CONAN_DIR"
TMP_TGZ="$(mktemp /tmp/conan-XXXXXX.tgz)"
curl -fL \
    "https://github.com/conan-io/conan/releases/download/$CONAN_VERSION/conan-${CONAN_VERSION}-linux-x86_64.tgz" \
    -o "$TMP_TGZ"
tar -xzf "$TMP_TGZ" -C "$CONAN_DIR"
rm -f "$TMP_TGZ"
chmod +x "$CONAN_BIN"

echo "Done: $($CONAN_BIN --version)"
