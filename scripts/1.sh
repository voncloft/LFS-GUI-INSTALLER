#!/usr/bin/env bash

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
OUTPUT_DIR="$SCRIPT_DIR"

if [ ! -w "$OUTPUT_DIR" ]; then
    OUTPUT_DIR="${INSTALL_RUN_DIR:-$PWD}"
fi

OUTPUT_FILE="$(mktemp "$OUTPUT_DIR/world.XXXXXX.txt")"

echo "step:HELLO"
printf '%s\n' 'WORLD' > "$OUTPUT_FILE"
