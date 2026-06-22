#!/usr/bin/env bash

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"

echo "step:HELLO"
printf '%s\n' 'WORLD' > "$SCRIPT_DIR/world.txt"
