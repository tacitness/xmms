#!/usr/bin/env bash
# =============================================================================
# format-check.sh — Run clang-format in check-only mode
#
# Usage:
#   ./scripts/format-check.sh             # check all C/H files
#   ./scripts/format-check.sh --fix       # auto-fix
#   ./scripts/format-check.sh src/foo.c   # specific file(s)
# =============================================================================

set -euo pipefail

FIX=false
FILES=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix) FIX=true; shift ;;
        *)     FILES+=("$1"); shift ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"

if [[ ${#FILES[@]} -eq 0 ]]; then
    mapfile -t FILES < <(find "$REPO_ROOT" \
        -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) \
        ! -path '*/intl/*' \
        ! -path '*/build/*' \
        ! -path '*/.git/*' \
        | sort)
fi

if ! command -v clang-format &>/dev/null; then
    echo "Error: clang-format not found. Run scripts/setup-dev.sh" >&2
    exit 1
fi

FAIL=0
for file in "${FILES[@]}"; do
    if [[ "$FIX" == true ]]; then
        clang-format --style=file -i "$file"
    else
        diff_output=$(clang-format --style=file "$file" | diff "$file" - || true)
        if [[ -n "$diff_output" ]]; then
            echo "❌  Format violation: $file"
            echo "$diff_output"
            FAIL=1
        fi
    fi
done

if [[ "$FIX" == true ]]; then
    echo "✅  Auto-formatted ${#FILES[@]} file(s)."
elif [[ $FAIL -eq 0 ]]; then
    echo "✅  All ${#FILES[@]} file(s) are properly formatted."
else
    echo ""
    echo "Run with --fix to auto-correct: ./scripts/format-check.sh --fix"
    exit 1
fi
