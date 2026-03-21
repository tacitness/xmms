# GNUmakefile — human-facing make wrapper that delegates to the CMake build.
#
# This file takes precedence over the autotools-generated Makefile on GNU make
# (GNU make reads GNUmakefile before Makefile), giving developers a single
# consistent `make` entrypoint while CMake is the authoritative build system.
#
# The legacy autotools Makefile / configure remain on disk for any packaging
# tooling that invokes them directly (rpmbuild, dpkg-buildpackage), but are
# NOT what `make` uses during day-to-day development.
#
# Usage:
#   make                  — configure (auto, first run) + build
#   make install          — build then install
#   make clean            — clean build artefacts (keeps cmake config)
#   make distclean        — wipe the entire build directory
#   make check            — run CTest suite
#   make reconfigure      — re-run cmake configuration
#
# Pass extra cmake config flags:
#   make CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Debug -DENABLE_GTK4=ON"
# Use a different build directory:
#   make BUILD_DIR=_build

BUILD_DIR  ?= build
CMAKE      ?= cmake
NPROC      := $(shell nproc 2>/dev/null || echo 4)
CMAKE_ARGS ?=

.PHONY: all install clean distclean check test reconfigure help

# ── Primary target ────────────────────────────────────────────────────────────
all: $(BUILD_DIR)/build.ninja
	$(CMAKE) --build $(BUILD_DIR) -j$(NPROC)

# ── Install ───────────────────────────────────────────────────────────────────
install: $(BUILD_DIR)/build.ninja
	$(CMAKE) --build $(BUILD_DIR) -j$(NPROC)
	$(CMAKE) --install $(BUILD_DIR)

# ── Clean (keep cmake config) ─────────────────────────────────────────────────
clean: $(BUILD_DIR)/build.ninja
	$(CMAKE) --build $(BUILD_DIR) --target clean

# ── Full wipe ─────────────────────────────────────────────────────────────────
distclean:
	rm -rf $(BUILD_DIR)

# ── Test suite ────────────────────────────────────────────────────────────────
check test: $(BUILD_DIR)/build.ninja
	cd $(BUILD_DIR) && ctest --output-on-failure -j$(NPROC)

# ── Re-run cmake configuration ────────────────────────────────────────────────
reconfigure:
	$(CMAKE) -B $(BUILD_DIR) $(CMAKE_ARGS)

# ── Auto-configure on first use ───────────────────────────────────────────────
$(BUILD_DIR)/build.ninja:
	$(CMAKE) -B $(BUILD_DIR) $(CMAKE_ARGS)

# ── Help ──────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "  make               — configure (if needed) + build"
	@echo "  make install       — build + install"
	@echo "  make clean         — clean build artefacts (keeps cmake config)"
	@echo "  make distclean     — remove entire build directory"
	@echo "  make check/test    — run CTest suite"
	@echo "  make reconfigure   — re-run cmake configuration"
	@echo ""
	@echo "  CMAKE_ARGS  — extra cmake flags, e.g. -DCMAKE_BUILD_TYPE=Debug"
	@echo "  BUILD_DIR   — build directory (default: build)"
	@echo ""
