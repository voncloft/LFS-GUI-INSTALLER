shopt -s expand_aliases

AUTOUNTAR_SCRIPT=

if [ -n "${TARGET_SCRIPTS:-}" ]; then
    CANDIDATE="$(cd -- "$TARGET_SCRIPTS/.." && pwd)/tools/autountar.sh"
    if [ -f "$CANDIDATE" ]; then
        AUTOUNTAR_SCRIPT="$CANDIDATE"
    fi
fi

if [ -z "${AUTOUNTAR_SCRIPT:-}" ] && [ -n "${PROJECT_ROOT:-}" ] && [ -f "$PROJECT_ROOT/tools/autountar.sh" ]; then
    AUTOUNTAR_SCRIPT="$PROJECT_ROOT/tools/autountar.sh"
fi

if [ -z "${AUTOUNTAR_SCRIPT:-}" ]; then
    PREPARE_DIR="${SCRIPT_DIR:-$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)}"
    AUTOUNTAR_SCRIPT="$(cd -- "$PREPARE_DIR/.." && pwd)/tools/autountar.sh"
fi

export AUTOUNTAR_SCRIPT
alias autountar='sh "$AUTOUNTAR_SCRIPT"'
