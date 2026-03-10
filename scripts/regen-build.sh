#!/usr/bin/env bash
# scripts/regen-build.sh
# Regenerate autotools build system for XMMS (GTK3 migration).
#
# Usage: ./scripts/regen-build.sh [--configure] [--build]
#
# Background:
#   autoreconf -fi fails because:
#     - configure.in uses obsolete name (not configure.ac)
#     - AM_PATH_ESD / AM_PATH_LIBMIKMOD macros not installed on modern systems
#   Instead, we invoke the autotools chain explicitly.
#
# To build after running this:
#   ./configure --prefix=/usr --enable-alsa --disable-esd
#   make -j$(nproc)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

DO_CONFIGURE=0
DO_BUILD=0
for arg in "$@"; do
    case "$arg" in
        --configure) DO_CONFIGURE=1 ;;
        --build)     DO_BUILD=1; DO_CONFIGURE=1 ;;
        *)           echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

echo "=== Step 1: libtoolize (root) ==="
libtoolize --force --copy

echo "=== Step 2: aclocal (root) ==="
aclocal -I m4

echo "=== Step 3: autoheader (root) ==="
autoheader

echo "=== Step 4: autoconf (root) ==="
autoconf

echo "=== Step 5: automake (root) ==="
automake --add-missing --copy

echo "=== Step 6: libtoolize (libxmms) ==="
pushd libxmms
libtoolize --force --copy
aclocal -I ../m4
autoheader
autoconf
automake --add-missing --copy
popd

echo ""
echo "=== Build system regenerated successfully ==="

if [[ "$DO_CONFIGURE" -eq 1 ]]; then
    echo ""
    echo "=== Running ./configure ==="
    ./configure \
        --prefix=/usr \
        --enable-alsa \
        --disable-esd \
        CFLAGS="-Wall -Wextra -std=c11 -O2 -Wno-unused-parameter"
fi

if [[ "$DO_BUILD" -eq 1 ]]; then
    echo ""
    echo "=== Running make ==="
    make -j"$(nproc)"
fi

echo ""
echo "Done. If configure was not run, now execute:"
echo "  ./configure --prefix=/usr --enable-alsa --disable-esd"
echo "  make -j\$(nproc)"
