#!/usr/bin/env bash
# =============================================================================
# setup-dev.sh — XMMS Resurrection Development Environment Bootstrap
#
# Installs all tools needed for building, linting, pre-commit, and packaging.
# Detects distro and uses appropriate package manager.
#
# Usage:
#   ./scripts/setup-dev.sh
#   ./scripts/setup-dev.sh --minimal   # skip heavy codecs / packaging tools
# =============================================================================

set -euo pipefail

MINIMAL=false
[[ "${1:-}" == "--minimal" ]] && MINIMAL=true

# ── Detect OS ─────────────────────────────────────────────────────────────────
detect_os() {
    if [[ -f /etc/os-release ]]; then
        # shellcheck source=/dev/null
        source /etc/os-release
        echo "${ID_LIKE:-$ID}"
    elif command -v uname &>/dev/null; then
        uname -s | tr '[:upper:]' '[:lower:]'
    else
        echo "unknown"
    fi
}

OS_FAMILY="$(detect_os)"
echo "==> Detected OS family: $OS_FAMILY"

# ── Install packages ──────────────────────────────────────────────────────────

install_fedora() {
    echo "==> Installing packages via dnf (Fedora/RHEL)..."
    sudo dnf groupinstall -y "Development Tools"
    sudo dnf install -y \
        gcc gcc-c++ make autoconf automake libtool pkg-config \
        cmake meson ninja-build \
        gtk3-devel gtk4-devel glib2-devel \
        alsa-lib-devel pulseaudio-libs-devel pipewire-devel \
        libmad-devel mpg123-devel libvorbis-devel libogg-devel flac-devel \
        libX11-devel libXext-devel libXxf86vm-devel \
        clang clang-tools-extra \
        python3 python3-pip git gh \
        rpm-build rpmdevtools rpmlint

    if [[ "$MINIMAL" == false ]]; then
        sudo dnf install -y \
            valgrind libasan libubsan \
            check-devel cmocka-devel \
            doxygen graphviz
    fi
}

install_debian() {
    echo "==> Installing packages via apt (Debian/Ubuntu)..."
    sudo apt-get update -qq
    sudo apt-get install -y \
        build-essential gcc g++ make autoconf automake libtool pkg-config \
        cmake meson ninja-build \
        libgtk-3-dev libgtk-4-dev libglib2.0-dev \
        libasound2-dev libpulse-dev libpipewire-0.3-dev \
        libmad0-dev libmpg123-dev libvorbis-dev libogg-dev libflac-dev \
        libx11-dev libxext-dev libxxf86vm-dev \
        clang clang-tidy clang-format \
        python3 python3-pip git

    # Install GitHub CLI
    if ! command -v gh &>/dev/null; then
        curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
            | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
        echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
            | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
        sudo apt-get update && sudo apt-get install -y gh
    fi

    if [[ "$MINIMAL" == false ]]; then
        sudo apt-get install -y \
            valgrind libasan6 \
            check libcmocka-dev \
            doxygen graphviz \
            devscripts debhelper dpkg-dev
    fi
}

install_arch() {
    echo "==> Installing packages via pacman (Arch Linux)..."
    sudo pacman -Syu --noconfirm \
        base-devel gcc make autoconf automake libtool pkgconf \
        cmake meson ninja \
        gtk3 gtk4 glib2 \
        alsa-lib libpulse pipewire \
        libmad mpg123 libvorbis libogg flac \
        libx11 libxext libxxf86vm \
        clang \
        python python-pip git github-cli

    if [[ "$MINIMAL" == false ]]; then
        sudo pacman -S --noconfirm valgrind check cmocka doxygen graphviz
    fi
}

install_alpine() {
    echo "==> Installing packages via apk (Alpine Linux)..."
    sudo apk add --no-cache \
        build-base gcc g++ make autoconf automake libtool pkgconf \
        cmake meson ninja \
        gtk+3.0-dev gtk4.0-dev glib2-dev \
        alsa-lib-dev pulseaudio-dev pipewire-dev \
        libmad-dev mpg123-dev libvorbis-dev libogg-dev flac-dev \
        libx11-dev libxext-dev \
        clang clang-extra-tools \
        python3 py3-pip git alpine-sdk atools

    # gh CLI from community repo
    sudo apk add --no-cache --repository=https://dl-cdn.alpinelinux.org/alpine/edge/community \
        github-cli || echo "  (gh CLI not available in this Alpine version — install manually)"
}

case "$OS_FAMILY" in
    *fedora*|*rhel*|*centos*)  install_fedora  ;;
    *debian*|*ubuntu*)         install_debian  ;;
    *arch*)                    install_arch    ;;
    *alpine*)                  install_alpine  ;;
    *)
        echo "⚠️  Unknown OS family: $OS_FAMILY"
        echo "Please install GTK3/4 dev headers, clang-format, clang-tidy, cmake, meson, and python3-pip manually."
        ;;
esac

# ── Python tools (pre-commit, commitizen, detect-secrets) ─────────────────────
echo "==> Installing Python dev tools..."
pip3 install --user --quiet \
    pre-commit \
    commitizen \
    detect-secrets

# ── include-what-you-use ──────────────────────────────────────────────────────
if ! command -v iwyu &>/dev/null && ! command -v include-what-you-use &>/dev/null; then
    echo "  (IWYU not found — install manually for full include-hygiene checks)"
    echo "  Ubuntu: apt install iwyu"
    echo "  Fedora: dnf install include-what-you-use"
fi

# ── Set up pre-commit ─────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || echo "$SCRIPT_DIR/..")"

echo "==> Installing pre-commit hooks in $REPO_ROOT..."
(cd "$REPO_ROOT" && pre-commit install && pre-commit install --hook-type commit-msg)

# ── Create detect-secrets baseline if missing ────────────────────────────────
if [[ ! -f "$REPO_ROOT/.secrets.baseline" ]]; then
    echo "==> Creating detect-secrets baseline..."
    (cd "$REPO_ROOT" && detect-secrets scan > .secrets.baseline 2>/dev/null || true)
fi

# ── Generate compile_commands.json for clang-tidy ────────────────────────────
echo "==> Generating compile_commands.json (for clang-tidy)..."
(
    cd "$REPO_ROOT"
    if [[ -f CMakeLists.txt ]]; then
        cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              -DCMAKE_BUILD_TYPE=Debug -G Ninja 2>/dev/null || true
    elif [[ -f meson.build ]]; then
        meson setup build 2>/dev/null || true
    else
        echo "  (No CMake/Meson found yet — compile_commands.json needed for clang-tidy)"
        echo "  Run: autoreconf -fi && ./configure && bear -- make  (requires 'bear')"
    fi
)

echo ""
echo "✅  Development environment setup complete!"
echo ""
echo "Next steps:"
echo "  1.  git clone <repo> && cd xmms"
echo "  2.  pre-commit run --all-files    # validate everything"
echo "  3.  See README for build instructions"
