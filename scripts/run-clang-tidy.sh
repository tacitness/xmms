#!/usr/bin/env bash
# scripts/run-clang-tidy.sh
# Pre-commit wrapper: runs clang-tidy only when compile_commands.json exists.
# Silently skips otherwise (CMake migration in progress).
set -euo pipefail

if [[ ! -f "build/compile_commands.json" ]]; then
    echo "clang-tidy: skipping — no build/compile_commands.json (run cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)"
    exit 0
fi

exec clang-tidy --config-file=.clang-tidy -p build "$@"
