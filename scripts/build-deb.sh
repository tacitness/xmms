#!/usr/bin/env bash
# scripts/build-deb.sh — Build Debian .deb package and collect artifacts in dist/deb/
#
# Usage:
#   bash scripts/build-deb.sh            # build + move artifacts to dist/deb/
#   bash scripts/build-deb.sh --no-move  # build only, leave artifacts in parent dir
#
# dpkg-buildpackage always writes output to the *parent* directory of the source
# tree, so this script wraps the build and relocates the artifacts to dist/deb/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PARENT_DIR="$(cd "${SRC_DIR}/.." && pwd)"
OUT_DIR="${SRC_DIR}/dist/deb"
NO_MOVE=0

for arg in "$@"; do
    case "$arg" in
        --no-move) NO_MOVE=1 ;;
        -h|--help)
            echo "Usage: bash scripts/build-deb.sh [--no-move]"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

PKG_VERSION="$(grep -m1 'VERSION [0-9]' "${SRC_DIR}/CMakeLists.txt" | awk '{print $2}')"
echo "Building XMMS ${PKG_VERSION} Debian package..."

cd "${SRC_DIR}"
dpkg-buildpackage -b -us -uc -j"$(nproc)"

if [[ "${NO_MOVE}" -eq 0 ]]; then
    mkdir -p "${OUT_DIR}"
    echo "Moving package artifacts to ${OUT_DIR}/"
    # dpkg-buildpackage writes to the parent directory
    find "${PARENT_DIR}" -maxdepth 1 \
        \( -name "xmms_*.deb" \
           -o -name "xmms-*.deb" \
           -o -name "xmms_*.ddeb" \
           -o -name "xmms-*.ddeb" \
           -o -name "xmms_*.buildinfo" \
           -o -name "xmms_*.changes" \) \
        -exec mv -v {} "${OUT_DIR}/" \;
    echo ""
    echo "✓ Package artifacts:"
    ls -lh "${OUT_DIR}/"
fi
