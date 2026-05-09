#!/bin/bash
set -e

VCPKG_DIR="${VCPKG_ROOT:-$HOME/vcpkg}"

if [ ! -d "$VCPKG_DIR/ports" ]; then
    echo "Error: vcpkg not found at $VCPKG_DIR"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VCPKG_JSON="$SCRIPT_DIR/../vcpkg.json"

deps=$(python3 -c "
import json
with open('$VCPKG_JSON') as f:
    d = json.load(f)
for dep in d['dependencies']:
    print(dep if isinstance(dep, str) else dep['name'])
")

echo "=== vcpkg dependency versions (baseline: $(git -C "$VCPKG_DIR" rev-parse --short HEAD)) ==="
for pkg in $deps; do
    port="$VCPKG_DIR/ports/$pkg/vcpkg.json"
    if [ -f "$port" ]; then
        version=$(python3 -c "
import json
d = json.load(open('$port'))
print(d.get('version') or d.get('version-string') or d.get('version-semver') or '?')
")
        printf "  %-20s %s\n" "$pkg" "$version"
    else
        printf "  %-20s (port not found)\n" "$pkg"
    fi
done
