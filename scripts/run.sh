#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BINARY="$SCRIPT_DIR/kimchi-ubuntu20"
CONFIG_DIR="${KIMCHI_CONFIG_DIR:-$SCRIPT_DIR/config}"

exec "$BINARY" \
    -config_dir="$CONFIG_DIR" \
    -logtostderr \
    "$@"
