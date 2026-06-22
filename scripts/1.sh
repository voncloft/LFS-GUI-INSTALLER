#!/usr/bin/env bash

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"

echo "step:HELLO"
cat > "$SCRIPT_DIR/world.txt" <<'EOF'
WORLD
EOF
