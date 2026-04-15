#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONAN="$SCRIPT_DIR/../conan/bin/conan"

echo "=== Exporting local Conan recipes ==="
$CONAN export "$SCRIPT_DIR/../recipes/folly"
$CONAN export "$SCRIPT_DIR/../recipes/fizz"
$CONAN export "$SCRIPT_DIR/../recipes/wangle"
$CONAN export "$SCRIPT_DIR/../recipes/mvfst"
$CONAN export "$SCRIPT_DIR/../recipes/proxygen"
echo "=== Done. Recipes exported to local Conan cache. ==="
