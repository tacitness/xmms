#!/usr/bin/env bash
# scripts/test-packages.sh — Build and smoke-test RPM + APK in Docker containers
#
# Usage:
#   bash scripts/test-packages.sh [rpm|apk|both]
#
# Artifacts land in:
#   dist/rpm/*.rpm
#   dist/apk/*.apk

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TARGET="${1:-both}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
ok()   { echo -e "${GREEN}✓${NC} $*"; }
info() { echo -e "${YELLOW}→${NC} $*"; }
fail() { echo -e "${RED}✗${NC} $*" >&2; exit 1; }

cd "${SRC_DIR}"

build_rpm() {
    info "Building Fedora RPM container image..."
    docker build \
        -f scripts/Dockerfile.fedora-rpm \
        -t xmms-rpmbuild:1.3.0 \
        . 2>&1 | tee /tmp/xmms-rpm-build.log | \
        grep -E 'Step|RUN|Successfully|error|Error|warning|Wrote:' || true

    mkdir -p dist/rpm
    info "Extracting RPMs from container..."
    CID=$(docker create xmms-rpmbuild:1.3.0)
    docker cp "${CID}:/out/." dist/rpm/ || true
    docker rm "${CID}" >/dev/null

    echo ""
    ok "RPM artifacts:"
    ls -lh dist/rpm/*.rpm 2>/dev/null || fail "No RPMs found in dist/rpm/"
}

build_apk() {
    info "Building Alpine APK container image..."
    docker build \
        -f scripts/Dockerfile.alpine-apk \
        -t xmms-apkbuild:1.3.0 \
        . 2>&1 | tee /tmp/xmms-apk-build.log | \
        grep -E 'Step|RUN|Successfully|error|Error|warning|\\.apk' || true

    mkdir -p dist/apk
    info "Extracting APKs from container..."
    CID=$(docker create xmms-apkbuild:1.3.0)
    docker cp "${CID}:/out/." dist/apk/ || true
    docker rm "${CID}" >/dev/null

    echo ""
    ok "APK artifacts:"
    ls -lh dist/apk/*.apk 2>/dev/null || fail "No APKs found in dist/apk/"
}

smoke_test_rpm() {
    local rpm
    rpm=$(ls dist/rpm/xmms-[0-9]*.rpm 2>/dev/null | head -1)
    [[ -z "${rpm}" ]] && return
    info "Smoke-testing RPM in Fedora container: ${rpm}"
    docker run --rm \
        -v "$(pwd)/dist/rpm:/pkgs:ro" \
        fedora:41 \
        bash -c "
            dnf install -y gtk3 glib2 alsa-lib pulseaudio-libs \
                libvorbis libogg mpg123 flac libX11 libXext libXxf86vm >/dev/null 2>&1
            rpm -ivh /pkgs/$(basename "${rpm}")
            echo '--- xmms binary ---'
            file /usr/bin/xmms
            strings /usr/bin/xmms | grep -iE 'resurrection|tacitsoft|1\.3\.' | head -5
            echo '--- version check ---'
            rpm -q xmms
        " 2>&1 | grep -v '^$' | tail -20
    ok "RPM smoke test passed"
}

smoke_test_apk() {
    local apk
    apk=$(ls dist/apk/xmms-[0-9]*.apk 2>/dev/null | head -1)
    [[ -z "${apk}" ]] && return
    info "Smoke-testing APK in Alpine container: ${apk}"
    docker run --rm \
        -v "$(pwd)/dist/apk:/pkgs:ro" \
        alpine:3.21 \
        sh -c "
            apk add --no-cache gtk+3.0 glib alsa-lib pulseaudio libvorbis libogg mpg123 flac \
                libx11 libxext libxxf86vm >/dev/null 2>&1
            # Install with --allow-untrusted since key isn't in this container
            apk add --allow-untrusted /pkgs/$(basename "${apk}")
            echo '--- xmms binary ---'
            file /usr/bin/xmms
            strings /usr/bin/xmms | grep -iE 'resurrection|tacitsoft|1\.3\.' | head -5
            echo '--- version check ---'
            apk info xmms
        " 2>&1 | grep -v '^$' | tail -20
    ok "APK smoke test passed"
}

echo ""
echo "════════════════════════════════════════════════════════"
echo " XMMS Resurrection — Package Build & Smoke Test"
echo "════════════════════════════════════════════════════════"
echo ""

case "${TARGET}" in
    rpm)
        build_rpm
        smoke_test_rpm
        ;;
    apk)
        build_apk
        smoke_test_apk
        ;;
    both)
        build_rpm
        smoke_test_rpm
        echo ""
        build_apk
        smoke_test_apk
        ;;
    *)
        echo "Usage: $0 [rpm|apk|both]"
        exit 1
        ;;
esac

echo ""
echo "════════════════════════════════════════════════════════"
ok "All done! Artifacts:"
[[ -d dist/rpm ]] && ls -lh dist/rpm/*.rpm 2>/dev/null || true
[[ -d dist/apk ]] && ls -lh dist/apk/*.apk 2>/dev/null || true
echo "════════════════════════════════════════════════════════"
