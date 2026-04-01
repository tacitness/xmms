# XMMS Resurrection — Agent Instructions

> **These instructions apply to every codex / automated agent working in this repository.**
> Read this file fully before touching any code.

---

## CRITICAL: Always Commit and Open a PR

**You MUST commit all changes and open a pull request before finishing ANY implement-mode task.**
The container is ephemeral — if you don't push and open a PR, all work is lost.

### Mandatory Git Workflow (implement mode)

```bash
# 0. Identify the issue number from your task
ISSUE_NUM=<from the task>
SCOPE=<short-description>           # e.g. skin, vis, eq, build

# 1. Create a branch following the naming convention
#    fix/NNN-short-desc   → bug fixes
#    feat/NNN-short-desc  → features / enhancements
#    gtk3/NNN-desc        → GTK3 migration work
#    refactor/NNN-desc    → refactors
git checkout -b fix/${ISSUE_NUM}-${SCOPE}   # or feat/ gtk3/ refactor/

# 2. Stage ALL changes (new files too)
git add -A

# 3. Commit with Conventional Commits format + closes reference
git commit -m "fix(${SCOPE}): <short imperative summary> (Closes #${ISSUE_NUM})"

# 4. Push
git push origin HEAD

# 5. Open a PR — assign to correct milestone
MILESTONE="v1.3.0 — GTK3 Parity Release"   # or "v1.4.0 — GTK4 + Enhancements"
gh pr create \
  --title "fix(${SCOPE}): <description> (Closes #${ISSUE_NUM})" \
  --body "## Summary
<what changed and why>

## Testing
<how you verified correctness>

Closes #${ISSUE_NUM}" \
  --base main \
  --label "ready-for-review"
```

**Do NOT skip steps 4 and 5.** If tests fail or can't run, still push and note the failure in the PR body.

### If the Build Cannot Run (Missing System Dependencies)

If GTK dev headers or audio libs are unavailable:

1. Run syntax + clang-format checks instead (see §Testing below).
2. Commit anyway: `git commit -m "fix(...): ... [build skipped: GTK3 dev headers required]"`
3. Open the PR — note in the body which packages are needed.

### Never Stop Without Pushing

```bash
git add -A
git commit -m "wip(${SCOPE}): partial impl [needs review] (Refs #${ISSUE_NUM})"
git push origin HEAD
# Then open the PR as a Draft
gh pr create --draft --title "WIP: ..." --body "Refs #${ISSUE_NUM}" --base main
```

---

## Environment Setup

### Quick Bootstrap (all distros)

```bash
# 1. Clone (if needed)
git clone git@github.com:tacitness/xmms.git && cd xmms

# 2. Run the dev-environment bootstrap script (detects distro automatically)
bash scripts/setup-dev.sh
# Use --minimal to skip Valgrind/check/cmocka (faster in CI containers)
bash scripts/setup-dev.sh --minimal
```

The script handles:
- C build toolchain (gcc, make, autoconf, automake, libtool, pkg-config)
- CMake ≥ 3.20, Meson ≥ 0.60, Ninja
- GTK3 + GTK4 dev headers
- Audio libs: ALSA, PulseAudio, PipeWire
- Codec libs: libmad/mpg123, libvorbis/libogg, libFLAC
- X11 dev headers
- clang, clang-tidy, clang-format
- Python3 + pre-commit, commitizen
- GitHub CLI (`gh`)

### Manual per-distro (subset)

**Fedora / RHEL:**
```bash
sudo dnf install -y gcc make cmake meson ninja-build pkg-config \
    gtk3-devel glib2-devel alsa-lib-devel pulseaudio-libs-devel \
    clang clang-tools-extra gh git
```

**Debian / Ubuntu:**
```bash
sudo apt-get install -y build-essential cmake meson ninja-build pkg-config \
    libgtk-3-dev libglib2.0-dev libasound2-dev libpulse-dev \
    clang clang-tidy clang-format gh git
```

**Arch:**
```bash
sudo pacman -Sy --noconfirm base-devel cmake meson ninja pkg-config \
    gtk3 glib2 alsa-lib libpulse clang github-cli git
```

**Alpine:**
```bash
apk add build-base cmake meson ninja pkgconf \
    gtk+3.0-dev glib-dev alsa-lib-dev pulseaudio-dev \
    clang clang-extra-tools github-cli git
```

---

## Building the Project

### Primary build system — CMake (use this)

```bash
# Configure (Debug, GTK3)
cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DXMMS_GTK_VERSION=3

# Build all targets
cmake --build build -j$(nproc)

# Install (optional, to /usr/local)
sudo cmake --install build

# Key CMake options:
#   -DXMMS_GTK_VERSION=4         Build against GTK4 instead of GTK3
#   -DXMMS_ENABLE_ALSA=OFF       Disable individual plugins
#   -DXMMS_ENABLE_MPG123=OFF     (see CMakeLists.txt for full list)
#   -DCMAKE_BUILD_TYPE=Release   Release mode
```

### Legacy / Autotools build (fallback only)

```bash
# Regenerate build system (needed after touching configure.in / Makefile.am)
bash scripts/regen-build.sh

# Configure + build
./configure --prefix=/usr --enable-alsa --disable-esd \
    CFLAGS="-Wall -Wextra -std=c11 -O2 -Wno-unused-parameter"
make -j$(nproc)
sudo make install
```

> **Prefer CMake.** Autotools support is retained for compatibility but new build
> features are added to CMakeLists.txt only.

---

## Testing

There is no automated test suite yet (tests/ not yet created — see issue #28).
Until it exists, verification is:

```bash
# 1. Format check (must pass, CI enforces)
bash scripts/format-check.sh
# Auto-fix:
bash scripts/format-check.sh --fix

# 2. clang-tidy (requires compile_commands.json — generated by cmake above)
bash scripts/run-clang-tidy.sh xmms/*.c libxmms/*.c

# 3. Full pre-commit suite (runs format + tidy + spell-check + commit-msg)
pre-commit run --all-files

# 4. Smoke test: does the binary launch?
./build/xmms/xmms &
sleep 3 && kill %1   # check exit code is 0 or normal signal

# 5. Build-only quick check (no system deps needed for compile errors)
cmake --build build 2>&1 | grep -E 'error:|warning:' | head -40
```

### Checking for regressions

```bash
# Compare warning count before/after your change
cmake --build build 2>&1 | grep -c 'warning:' || true
```

---

## Project Structure

```
xmms/                   ← Core player binary (main window, skin, EQ, PL)
  main.c                ← Entry point, mainwin_create_gtk(), signal setup
  dock.c / dock.h       ← Window snapping/docking system
  skin.c / skin.h       ← Winamp 2.x skin loading + rendering
  playlist.c            ← Playlist engine
  equalizer.c           ← Equalizer window
  vis.c                 ← Visualization dispatch

libxmms/                ← Shared library — used by plugins AND the binary
  xmmsctrl.c            ← Remote-control API (xmms_remote_*)
  snap.c / snap.h       ← Vtable bridge: vis plugins → dock system
  util.c                ← Misc utilities (g_snprintf wrappers, etc.)

Input/                  ← Input plugins (.so): mpg123, vorbis, wav, cdaudio…
Output/                 ← Output plugins (.so): alsa, oss, pulse, diskwriter…
Effect/                 ← Effect plugins (.so): echo, stereo, voice
General/                ← General plugins (.so): joystick, song_change, ir
Visualization/          ← Vis plugins (.so): blurscope, sanalyzer, openglspectrum
  common/vis_chrome.c   ← Shared vis window chrome (title bar, drag, snap)
  common/vis_chrome.h

cmake/                  ← CMake helpers (XmmsPlugin.cmake, xmms_config.h.cmake)
scripts/                ← Dev tooling (setup-dev.sh, format-check.sh, …)
docs/                   ← Migration docs (GTK_MIGRATION_ROADMAP.md)
po/                     ← i18n translations
data/                   ← Installed data (icons, .desktop file) — see #38
```

---

## Code Style — Non-Negotiable Rules

| Rule | Detail |
|------|--------|
| **Standard** | C11 (`-std=c11`) |
| **Indent** | 4 spaces — NO tabs |
| **Braces** | K&R (opening brace on same line) |
| **Line length** | 100 chars max |
| **Pointer** | `int *ptr` — type-side |
| **Naming** | `snake_case` functions/vars; `SCREAMING_SNAKE_CASE` macros |
| **API prefix** | `playlist_`, `skin_`, `eq_`, `dock_`, `vis_` — match module |
| **sprintf** | NEVER — use `g_snprintf()` or `snprintf(size)` |
| **gets()** | NEVER — use `fgets()` |
| **malloc** | Prefer `g_malloc`/`g_free` in GTK-facing code |
| **NULL guard** | `g_return_if_fail()` / `g_return_val_if_fail()` in GLib contexts |
| **i18n** | Wrap all user-visible strings in `_()` macro |
| **GTK2 APIs** | Never introduce deprecated GTK2-only APIs without compat guard |

### Include ordering (clang-tidy enforced)

```c
#include "own_header.h"     /* 1: own header first */

#include <glib.h>           /* 2: system / library headers */
#include <gtk/gtk.h>
#include <stdlib.h>

#include "xmms.h"           /* 3: project-local headers */
#include "skin.h"
```

### GTK migration comments

Every replaced GTK2 API must have a comment:
```c
/* GTK3: replaced gdk_pixmap_create_from_xpm() with GdkPixbuf */
```

---

## Commit Message Format

All commits must follow [Conventional Commits v1.0](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary in imperative mood>

[optional body — wrap at 72 chars]

Closes #NNN
```

**Types:** `feat` `fix` `refactor` `style` `docs` `test` `build` `ci` `chore` `perf` `gtk3` `gtk4`

**Scopes:** `core` `skin` `playlist` `eq` `input` `output` `vis` `prefs` `i18n` `build` `ci` `docs` `alsa` `pulse` `mp3` `ogg`

**Examples:**
```
fix(vis): snap visualization windows to dock system (Closes #34)
feat(vis): dynamic plugin list in Visualizations menu (Closes #36)
feat(build): add freedesktop icon set and .desktop entry (Closes #38)
build(rpm): rewrite xmms.spec for GTK3 + CMake (Closes #30)
gtk3(skin): replace GdkPixmap with cairo_t surface rendering (Refs #24)
```

---

## Issue + Milestone Workflow

### Milestones

| # | Title | Due | Scope |
|---|-------|-----|-------|
| 1 | `v1.3.0 — GTK3 Parity Release` | 2026-06-30 | GTK3 migration, bug fixes, packaging |
| 2 | `v1.4.0 — GTK4 + Enhancements` | 2026-12-31 | GTK4, GdkEventController, Wayland-first |

### Before Starting Any Issue

```bash
# Confirm the issue exists and is assigned to a milestone
gh issue view <N>

# If not assigned to a milestone, assign it before starting:
gh issue edit <N> --milestone "v1.3.0 — GTK3 Parity Release"

# Move to in-progress
gh issue edit <N> --add-label "in-progress" --remove-label "confirmed"
```

### After Opening a PR

```bash
# Link PR to milestone and issue
gh pr edit <PR_NUM> --add-label "ready-for-review"
# The "Closes #N" in the PR body auto-closes the issue on merge
```

---

## Open Issues (as of 2026-04-01)

### v1.3.0 — GTK3 Parity Release (milestone #1)

| # | Type | Title | Priority |
|---|------|-------|----------|
| 34 | bug | vis windows don't dock/snap to skin windows; not resizable | P2 |
| 36 | feat | dynamic vis plugin list in Visualizations menu | P2 |
| 38 | feat | SVG + PNG icon + freedesktop .desktop entry | P2 |
| 32 | build | APKBUILD for Alpine packaging | P2 |
| 31 | build | debian/ packaging dir for dpkg-buildpackage | P1 |
| 30 | build | Rewrite xmms.spec for GTK3 + CMake | P1 |
| 28 | test | Acquire Winamp skin test corpus + compatibility tests | P1 |
| 27 | bug | Window shaping not applied to mainwin and EQ window | P1 |
| 24 | chore | Resolve all TODO(#gtk3) stubs for 1.0 parity | P1 |

### v1.4.0 — GTK4 + Enhancements (milestone #2)

| # | Type | Title | Priority |
|---|------|-------|----------|
| 29 | feat | Built-in skin editor | P3 |

### Listing all open issues

```bash
gh issue list --state open --limit 50
gh issue list --milestone "v1.3.0 — GTK3 Parity Release"
```

---

## Branch Naming

```
fix/NNN-short-desc          ← bug fix for issue #NNN
feat/NNN-short-desc         ← feature/enhancement for issue #NNN
gtk3/NNN-desc               ← GTK3 migration work
gtk4/NNN-desc               ← GTK4 migration work
refactor/NNN-desc           ← refactor sub-task
release/vX.Y.Z              ← release prep (maintainer only)
```

---

## Key Architectural Notes

### Plugin DSO / Binary Boundary

- Plugins are compiled as `.so` files and loaded at runtime via `dlopen`.
- Plugins link `libxmms.so` — they do **NOT** link the `xmms` binary.
- To expose main-binary functionality to plugins, use the vtable bridge pattern in `libxmms/`:
  - Declare a zero-init global in `libxmms/foo.c` (e.g. `XmmsSnapInterface xmms_snap`)
  - Populate it from `xmms/main.c` at startup (e.g. `vis_snap_init()`)
  - Plugins read it through `libxmms/foo.h`
- **Never** have a plugin call a symbol that lives only in the `xmms` binary.

### Dock / Snap System

`xmms/dock.c` implements the window-docking that keeps mainwin / EQ / playlist snapped together.
Key functions:
```c
dock_add_window(GList *window_list, GtkWidget *win);
dock_move_press(GList *window_list, GtkWidget *win, GdkEventButton *ev, gboolean group);
dock_move_motion(GtkWidget *win, GdkEventMotion *ev);
dock_move_release(GtkWidget *win);
```
All vis windows must use `xmms_snap` vtable (in `libxmms/snap.h`) to participate in this system.

### GTK3 Drawing

All drawing goes through `cairo_t`. Never use `GdkPixmap` / `GdkGC`. Use `GdkPixbuf` for
image loading, blit to `cairo_t` via `gdk_cairo_set_source_pixbuf()`.

### Wayland Safety

Wrap all X11-specific calls:
```c
#ifdef GDK_WINDOWING_X11
  /* x11-only operation */
#endif
```
Never call `GDK_DISPLAY()` directly; use `gdk_display_get_default()`.

---

## Security Checklist

Before committing, verify:
- [ ] No `sprintf` — use `g_snprintf()` or `snprintf(buf, sizeof(buf), ...)`
- [ ] No `gets()` — use `fgets()`
- [ ] No hardcoded paths — use `PKGLIBDIR`, `PKGDATADIR` CMake variables
- [ ] Skin/plugin paths validated against traversal (`../` etc.)
- [ ] Plugin ABI version checked before calling function pointers
- [ ] No credentials / secrets in source

---

## Pre-commit Hooks

Install once after cloning:
```bash
pip3 install --user pre-commit commitizen
pre-commit install
pre-commit install --hook-type commit-msg
```

Hooks that run on `git commit`:
1. `clang-format` — auto-formats changed `.c`/`.h` files
2. `clang-tidy` — static analysis (skipped if no `build/compile_commands.json`)
3. `trailing-whitespace` / `end-of-file-fixer`
4. `check-merge-conflict`
5. `commitizen` — validates Conventional Commits format
6. `codespell` — spell-checks comments

Run manually at any time:
```bash
pre-commit run --all-files
```

---

## Useful One-liners

```bash
# List all TODO(#gtk3) stubs
grep -rn 'TODO(#gtk3)' xmms/ libxmms/ --include='*.c' --include='*.h'

# Count build warnings
cmake --build build 2>&1 | grep -c 'warning:'

# Check clang-format compliance (no fix)
bash scripts/format-check.sh

# Auto-fix formatting
bash scripts/format-check.sh --fix

# Run clang-tidy on a single file
clang-tidy -p build xmms/skin.c

# See current milestone progress
gh milestone list

# List all branches
git branch -a

# Check what files a PR touches
gh pr diff <PR_NUM> --name-only
```
