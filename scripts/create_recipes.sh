#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONAN="$SCRIPT_DIR/../conan/bin/conan"
PROFILE="$SCRIPT_DIR/../conan/profile"

create_recipe() {
    local label="$1"
    local path="$2"
    local build_type="$3"

    echo "  [${label}] build_type=${build_type}..."
    $CONAN create "$path" \
        --profile:host="$PROFILE" \
        --profile:build="$PROFILE" \
        -s build_type="${build_type}" \
        --build=missing
}

echo "=== Creating local Conan recipes (Release + Debug) ==="

for BUILD_TYPE in Release Debug; do
    echo "--- ${BUILD_TYPE} ---"
    create_recipe "1/4 folly"    "$SCRIPT_DIR/../recipes/folly"    "$BUILD_TYPE"
    create_recipe "2/4 fizz"     "$SCRIPT_DIR/../recipes/fizz"     "$BUILD_TYPE"
    create_recipe "3/4 wangle"   "$SCRIPT_DIR/../recipes/wangle"   "$BUILD_TYPE"
    create_recipe "4/4 proxygen" "$SCRIPT_DIR/../recipes/proxygen" "$BUILD_TYPE"
done

echo "=== Done. All recipes registered in local Conan cache. ==="
