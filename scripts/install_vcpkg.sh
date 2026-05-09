#!/bin/bash
set -e

VCPKG_DIR="${VCPKG_ROOT:-$HOME/vcpkg}"

if [ -f "$VCPKG_DIR/vcpkg" ]; then
    echo "vcpkg already installed: $($VCPKG_DIR/vcpkg version | head -1)"
    exit 0
fi

echo "=== Cloning vcpkg to $VCPKG_DIR ==="
git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"

echo "=== Bootstrapping vcpkg ==="
"$VCPKG_DIR/bootstrap-vcpkg.sh" -disableMetrics

echo ""
echo "=== Done ==="
echo "vcpkg installed: $($VCPKG_DIR/vcpkg version | head -1)"
echo ""
echo "Add to your ~/.bashrc:"
echo "  export VCPKG_ROOT=$VCPKG_DIR"
