# GitHub Copilot Instructions — XMMS Resurrection Project

## Agent Dispatch — tsctl

This repo is registered in the Tacitsoft infrastructure control CLI (`tsctl`) as key **`xmms`**.
Use `tsctl` to dispatch Codex (or other runner) jobs for GitHub issues.

### repos.yaml location

```
/data/src/tacitsoft/infrastructure/tsctl/repos.yaml
```

Always pass `--repos-file` to every tsctl command:

```bash
REPOS=/data/src/tacitsoft/infrastructure/tsctl/repos.yaml
```

### Common tsctl commands

```bash
# List registered repos
tsctl repos list --repos-file $REPOS

# Run preflight checks (always do this before dispatching)
tsctl agent preflight xmms --repos-file $REPOS

# Plan dispatch order for a set of issues (DAG-aware wave schedule)
tsctl agent plan xmms --repos-file $REPOS --issues 27,24,36,28

# Dispatch a single issue to Codex
tsctl agent dispatch xmms --repos-file $REPOS --runner codex --issue 27 --mode implement

# Dispatch multiple issues as a dependency-aware batch
tsctl agent dispatch xmms --repos-file $REPOS --runner codex --issues 24,27,28 --wait

# Dry-run (prints k8s Job manifest, no job created)
tsctl agent dispatch xmms --repos-file $REPOS --runner codex --issue 27 --dry-run

# Watch job logs
tsctl agent logs xmms-27 --repos-file $REPOS

# List active jobs
tsctl agent jobs --repos-file $REPOS
```

### Dispatch order for remaining v1.3.0 issues

Wave schedule (from `tsctl agent plan xmms --issues 27,24,36,28`):

| Wave | Issues | Notes |
|------|--------|-------|
| 1 (parallel) | #27, #24, #28 | No inter-dependencies |
| 2 (after #24) | #36 | Vis menu depends on GtkItemFactory → GMenuModel work in #24 |

Dispatch wave 1:
```bash
tsctl agent dispatch xmms --repos-file $REPOS --runner codex --issues 27,24,28 --wait
```

Dispatch wave 2 (after wave 1 completes):
```bash
tsctl agent dispatch xmms --repos-file $REPOS --runner codex --issue 36 --mode implement
```

### Available runners

`codex | copilot | claude | aider | ollama | gemini | amazonq | gptscript | openhands | shell`

Use `tsctl agent list --repos-file $REPOS` to check which runners are installed locally.

---

## Project Overview

**XMMS** (X Multimedia System) is a classic Winamp-style media player originally built with GTK+1/GTK+2. This project is being **modernized and resurrected** — migrating to GTK3/GTK4 while preserving the beloved Winamp-skin UI paradigm and plugin architecture.

- **Language:** C (primary), C++ (plugins, optional)
- **Build system:** Autotools (autoconf/automake) → migrating to CMake + Meson
- **UI Toolkit:** GTK+2 → **GTK3 / GTK4 target**
- **Audio subsystem:** OSS/ALSA/ESD/PulseAudio → PipeWire, PulseAudio
- **Platforms target:** Linux (Fedora, Ubuntu/Debian, Arch, Alpine)
- **Packaging targets:** RPM (.spec), DEB (.deb), APK (Alpine)

---

## Code Style & Formatting

### C Language Rules (enforced via clang-format)

- **Standard:** C11 (`-std=c11`) or C99 minimum
- **Indent:** 4 spaces — NO tabs
- **Brace style:** K&R — opening brace on same line for functions and control structures
- **Max line length:** 100 characters
- **Pointer alignment:** type-side (`int *ptr`, not `int* ptr`)
- **Include ordering (clang-tidy/iwyu enforced):**
  1. Corresponding `.h` for the `.c` file (own header first)
  2. System/OS headers (`<glib.h>`, `<gtk/gtk.h>`, `<stdlib.h>`, …)
  3. Project-local headers (`"xmms.h"`, `"skin.h"`, …)
  - Separated by blank lines between groups

```c
/* Own header first */
#include "playlist.h"

/* System / library headers */
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

/* Project local */
#include "xmms.h"
#include "skin.h"
```

- **Comments:** `/* ... */` for multi-line, `//` acceptable for single-line in C99+
- **Naming:**
  - `snake_case` for all functions, variables, and file-scope globals
  - `SCREAMING_SNAKE_CASE` for `#define` macros and constants
  - Prefix public API with module name: `playlist_add()`, `skin_load()`, `eq_set_band()`
  - Prefix private/static helpers with `_` or suffix with `_internal`/`_priv`
- **Structs / typedefs:** `typedef struct _Foo { … } Foo;` pattern (GObject-compatible)
- **Error handling:** Always check return values; propagate via `gboolean`/`GError` or errno
- **Memory:** Every `g_malloc` / `malloc` must have a corresponding `g_free` / `free`; prefer GLib allocation in GTK code
- **NULL checks:** Always guard pointer dereferences; use `g_return_if_fail()` / `g_return_val_if_fail()` in GLib contexts
- **No magic numbers:** Use named constants for all numeric literals other than 0 and 1

### GTK API Rules (GTK3/GTK4 migration target)

- **Never use deprecated GTK+2 APIs** unless inside `#if GTK_CHECK_VERSION(2,0,0)` compat guards
- Prefer **GtkBuilder + .ui XML** over hand-coded widget construction
- Use **GObject properties** and **signals** correctly — connect via `g_signal_connect()`
- Use `G_DECLARE_FINAL_TYPE` / `G_DECLARE_DERIVABLE_TYPE` for custom GObject types
- All widget allocation: use `gtk_widget_new()` or builder patterns
- Drawing: migrate from `GdkPixmap` (GTK2) → `cairo_t` surfaces (GTK3/4)
- **CSS styling** via `GtkCssProvider` for theming/skinning in GTK3+

---

## Repository Structure

```
xmms/
├── .github/
│   ├── copilot-instructions.md   ← YOU ARE HERE
│   ├── ISSUE_TEMPLATE/
│   │   ├── bug_report.yml
│   │   ├── feature_request.yml
│   │   └── gtk_migration.yml
│   └── workflows/
│       ├── ci.yml                ← Build + lint + test on PRs
│       └── package.yml           ← RPM/DEB/APK packaging releases
├── .clang-format                 ← Enforced formatting
├── .clang-tidy                   ← Enforced linting
├── .editorconfig                 ← Editor normalization
├── .pre-commit-config.yaml       ← Pre-commit hook config
├── scripts/
│   ├── setup-dev.sh             ← Dev environment bootstrap
│   ├── create-gh-labels.sh      ← GitHub labels setup
│   └── format-check.sh          ← Manual format check helper
├── xmms/                        ← Core player (main window, skin, etc.)
├── Input/                       ← Input plugins (mp3, ogg, wav, cd…)
├── Output/                      ← Output plugins (ALSA, OSS, PulseAudio…)
├── Effect/                      ← Effect plugins (EQ, echo, stereo…)
├── General/                     ← General plugins (joystick, song_change…)
├── Visualization/               ← Visualization plugins
├── libxmms/                     ← Shared utility library
└── po/                          ← i18n translations
```

---

## SCM / Commit Hygiene

### Branching Strategy (GitHub Flow)

```
main          ← stable, always releasable
develop       ← integration branch
feature/...   ← feature branches (from develop)
fix/...       ← bug fixes (from develop or main if hotfix)
gtk3/...      ← GTK3 migration work branches
gtk4/...      ← GTK4 migration work branches
release/...   ← release prep branches
```

### Commit Message Format (Conventional Commits)

All commits **must** follow [Conventional Commits v1.0](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary in imperative mood>

[optional body — wrap at 72 chars]

[optional footer(s): Closes #123, Refs #456, BREAKING CHANGE: ...]
```

**Types:**
| Type | Use when |
|------|----------|
| `feat` | New feature or capability |
| `fix` | Bug fix |
| `refactor` | Code restructuring without behavior change |
| `style` | Formatting/whitespace only |
| `docs` | Documentation only |
| `test` | Adding or fixing tests |
| `build` | Build system, Makefile, CMake, packaging |
| `ci` | CI/CD workflow changes |
| `chore` | Maintenance tasks, deps updates |
| `perf` | Performance improvement |
| `gtk3` | GTK3 migration work (project-specific) |
| `gtk4` | GTK4 migration work (project-specific) |

**Scopes (examples):** `core`, `skin`, `playlist`, `eq`, `input`, `output`, `vis`, `prefs`, `i18n`, `build`, `ci`, `docs`, `alsa`, `pulse`, `mp3`, `ogg`

**Examples:**
```
feat(skin): add GtkCssProvider-based colour scheme loader
fix(playlist): prevent use-after-free in playlist_clear()
refactor(eq): replace GdkPixmap with cairo_t surface rendering
build(cmake): add FindGTK4.cmake module for GTK4 detection
ci: add Fedora 41 RPM packaging job
gtk3(mainwin): migrate GtkFixed layout to GtkOverlay
```

### PR Rules

- **Atomic PRs:** One logical change per PR — no mixing unrelated concerns
- **Draft PRs:** Open as draft while WIP; convert to ready when CI passes
- **PR title:** Must follow Conventional Commits format
- **PR description:** Fill in the template — what, why, how tested
- **Links:** Reference GH issues: `Closes #123` or `Refs #456`
- **Milestone:** The linked issue must be assigned to a milestone before the PR is opened
- **Review:** Minimum 1 approval; maintainer approval for GTK migration PRs
- **Squash and merge** for feature branches; **merge commit** for release branches

---

## Pre-Commit Hooks (enforced locally and in CI)

Managed via [pre-commit](https://pre-commit.com/). Install once:

```bash
pip install pre-commit
pre-commit install
pre-commit install --hook-type commit-msg
```

Hooks run on every `git commit`:
1. **clang-format** — auto-formats changed `.c`/`.h`/`.cpp` files
2. **clang-tidy** — static analysis on changed files
3. **include-what-you-use (IWYU)** — validates include hygiene
4. **trailing-whitespace** — removes trailing whitespace
5. **end-of-file-fixer** — ensures newline at EOF
6. **check-merge-conflict** — blocks accidental conflict markers
7. **commit-msg (commitizen)** — validates Conventional Commits format
8. **codespell** — spell-checks comments and strings

---

## Release Milestones & Agile Workflow

### Version scheme

XMMS Resurrection follows **Semantic Versioning** from the original 1.2.11 baseline:

| Version | Target | Description |
|---------|--------|-------------|
| `v1.3.0` | 2026-06-30 | **GTK3 Parity Release** — 100% functional parity with XMMS 1.2.11, zero `TODO(#gtk3)` stubs, all menus/shaping/skins working |
| `v1.4.0` | 2026-12-31 | **GTK4 + Enhancements** — GdkEventController, GtkListView playlist, Wayland-first, next-gen vis |
| `v1.5.0+` | TBD | Post-GTK4: skin editor, advanced features |

Rationale for `1.3` over `2.0`: this is a port, not a rewrite — the ABI, plugin interface, skin format, and control socket protocol remain compatible with XMMS 1.2.x.

### Milestone management

- Every open issue **must** be assigned to a milestone before it can be marked `in-progress`
- Copilot and contributors must set `--milestone` when creating issues via `gh issue create`
- Use `gh milestone list` to see current milestones and their progress
- A milestone is **closed** only when all its issues are closed and a git tag + GitHub Release is created

```bash
# Create a milestone
gh milestone create --title "v1.3.0 — GTK3 Parity Release" --due-date "2026-06-30" --description "..."

# Assign an issue
gh issue edit <N> --milestone "v1.3.0 — GTK3 Parity Release"

# Check milestone progress
gh milestone list
```

### Agile development workflow

This project uses a **lightweight Kanban flow** driven entirely by GitHub Issues + Milestones. There are no sprints — work is continuous and milestone-gated.

#### Issue lifecycle

```
[filed] → needs-triage → confirmed → in-progress → ready-for-review → closed
```

1. **File** every bug/feature/task as an issue before touching code
2. **Triage**: add labels (type + component + priority + effort) and assign to a milestone
3. **Start work**: move to `in-progress`, create a branch `fix/NNN-short-desc` or `feat/NNN-short-desc`
4. **Open a PR** referencing the issue (`Closes #NNN`); mark as Draft until CI passes
5. **Review**: convert Draft → Ready; minimum 1 approval required
6. **Merge**: squash-merge for features/fixes; the issue closes automatically via `Closes #NNN`

#### Branch naming

```
fix/25-child-window-raise       ← bug fix for issue #25
feat/27-skin-test-corpus        ← feature or test work for issue #27
gtk3/26-mainwin-shaping         ← GTK migration work
refactor/24-gtkclist-completion ← refactor sub-task of #24
release/v1.3.0                  ← release prep
```

#### PR checklist additions (milestone-aware)

- [ ] Issue is assigned to a milestone before PR is opened
- [ ] PR title references the issue: `fix(core): raise child windows on focus (Closes #25)`
- [ ] Milestone acceptance criteria are checked before marking ready-for-review

#### Working with Copilot on milestone items

When Copilot starts work on a tracked issue it should:
1. State which issue/milestone the work addresses at the start of the session
2. Create the branch with the `fix/NNN-` or `feat/NNN-` naming convention
3. Reference `Closes #NNN` in every commit that fully resolves an issue
4. After each commit, update the issue with a brief progress note if the issue spans multiple PRs

---

## Issue Tracking (GitHub Issues)

### When to file an issue

- **Every bug** found (even if you plan to fix it immediately)  
- **Every feature/enhancement** before starting implementation  
- **Every GTK migration task** per-component (one issue per major widget/subsystem)  
- **Every dependency/packaging change**

### Issue Labels (see `scripts/create-gh-labels.sh`)

**Type:** `bug`, `feature`, `refactor`, `docs`, `test`, `security`, `chore`  
**Component:** `comp:core`, `comp:skin`, `comp:playlist`, `comp:equalizer`, `comp:visualizer`, `comp:input-plugin`, `comp:output-plugin`, `comp:effect-plugin`, `comp:general-plugin`, `comp:prefs`, `comp:build`, `comp:ci-cd`, `comp:packaging`, `comp:i18n`  
**Migration:** `gtk3`, `gtk4`, `gtk-migration`, `compat-break`  
**Platform:** `platform:linux`, `platform:fedora`, `platform:debian-ubuntu`, `platform:arch`, `platform:alpine`, `platform:wayland`, `platform:x11`  
**Priority:** `P0-critical`, `P1-high`, `P2-medium`, `P3-low`  
**Status:** `needs-triage`, `confirmed`, `in-progress`, `blocked`, `ready-for-review`, `needs-more-info`, `wontfix`  
**Effort:** `effort-small`, `effort-medium`, `effort-large`, `effort-xlarge`

---

## GTK3/GTK4 Migration Guidelines

### Migration Philosophy

- **Incremental migration** — no big-bang rewrite; one subsystem at a time
- Keep GTK2 compatibility shims (`#ifdef GTK_VERSION_LT_3`) until full removal milestone
- Use **adaptation layer** (`xmms/gtk_compat.h`) to abstract version differences
- Document every deprecated API replaced with a `/* GTK3: replaced XYZ with ABC */` comment

### Key Migration Areas

| Subsystem | GTK2 API | GTK3/4 Replacement |
|-----------|----------|-------------------|
| Drawing | `GdkPixmap`, `GdkGC` | `cairo_t`, `GdkTexture` |
| Layout | `GtkFixed` coordinates | `GtkFixed` (GTK3 ok), `GtkOverlay` |
| Theming | Hardcoded colors, `GtkStyle` | `GtkCssProvider`, CSS variables |
| Pixmaps | `gdk_pixmap_create_*` | `GdkPixbuf`, `GdkTexture` |
| Events | `GdkEvent` direct fields | `GdkEventController` (GTK4) |
| Signals | `gtk_signal_connect` | `g_signal_connect` (already GTK2) |
| Cursors | `GdkCursor` | Same API, different constructor |
| Drag & Drop | `gtk_drag_*` | `GdkDrag` / `GdkDrop` (GTK4) |
| Menus | `GtkItemFactory` | `GtkPopoverMenu` + `GMenuModel` (GTK4) |
| Socket/Plug | `GtkSocket`/`GtkPlug` | Removed in GTK4 — use X11/Wayland alt |

### Wayland Compatibility

- Replace all `GDK_DISPLAY()` X11 direct calls with GDK abstraction layer
- Use `gdk_wayland_*` / `gdk_x11_*` only inside `#ifdef GDK_WINDOWING_WAYLAND` guards
- Window positioning (skin anchored windows): use `GtkWindow` hints instead of raw X11 atoms

---

## Build System

### Current (Autotools) Build

```bash
./autogen.sh  # or autoreconf -fi
./configure --prefix=/usr --enable-alsa --disable-esd
make -j$(nproc)
sudo make install
```

### Target (CMake/Meson) Build — migration in progress

```bash
# CMake (target)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_GTK4=ON
cmake --build build -j$(nproc)

# Meson (alternative target)
meson setup builddir -Dgtk_version=4
ninja -C builddir
```

### Build Flags for Development

```makefile
CFLAGS := -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
          -Wno-unused-parameter -pedantic -std=c11 -g -O0 \
          $(shell pkg-config --cflags gtk+-3.0 glib-2.0)
```

---

## Testing Strategy

- **Unit tests:** [Check](https://libcheck.github.io/check/) or [cmocka](https://cmocka.org/) framework for C
- **Integration tests:** Shell scripts exercising `xmms-remote` / `xmmsctrl` API
- **Visual regression:** Screenshot comparison of skin rendering (GTK migration guard)
- Test files live in `tests/` (to be created)
- CI runs all tests on: Ubuntu LTS, Fedora latest, Arch, Alpine

---

## Packaging

| Format | Tool | Source file | CI job |
|--------|------|-------------|--------|
| RPM | `rpmbuild` | `xmms.spec` | `package.yml` → `build-rpm` |
| DEB | `dpkg-buildpackage` | `debian/` | `package.yml` → `build-deb` |  
| APK | `abuild` | `APKBUILD` | `package.yml` → `build-apk` |

Release packages are built on GitHub Actions and attached to GitHub Releases.

---

## Dependency Management

- **GTK3:** `gtk3-devel` / `libgtk-3-dev` / `gtk3`
- **GTK4:** `gtk4-devel` / `libgtk-4-dev` / `gtk4`
- **GLib 2.x:** included with GTK
- **Audio:** `alsa-lib-devel`, `libpulse-devel`, `pipewire-devel`
- **Codecs:** `libmad` / `libmpg123` (mp3), `libvorbis` / `libogg` (ogg), `libFLAC`
- **Build tools:** `autoconf`, `automake`, `libtool`, `pkg-config`, `cmake` ≥3.20, `meson` ≥0.60

---

## Code Review Checklist

When reviewing a PR, check:

- [ ] Follows Conventional Commits format
- [ ] References a GH issue (`Closes #N` or `Refs #N`)
- [ ] No new GTK2-specific APIs introduced without compat guard
- [ ] No `malloc`/`free` without corresponding pair (use Valgrind/ASAN)
- [ ] clang-format passes (CI enforces)
- [ ] clang-tidy passes with no new warnings
- [ ] Include ordering correct (own → system → project)
- [ ] No hardcoded paths (`/usr/lib/xmms/...` → use `PKGLIBDIR`)
- [ ] i18n strings wrapped in `_()` / `N_()`
- [ ] Wayland compatibility considered for any X11-touching code
- [ ] Memory-safe: no buffer overflows, no format string issues

---

## Security Guidelines

- Never use `sprintf` → use `g_snprintf()` or `snprintf()` with size
- Never use `gets()` — use `fgets()` or GLib I/O
- Validate all external input (playlist files, skin archives, plugin paths)
- Skin loading: validate ZIP/tar extraction paths (prevent path traversal)
- Plugin loading: verify plugin ABI version before calling function pointers
- No hardcoded credentials or sensitive data in source
- Use `G_GNUC_PRINTF` attribute on printf-like functions for compiler checking

---

## Copilot Interaction Guidelines

When GitHub Copilot assists with this project:

1. **Always use C11 standard** — suggest modern C features (designated initializers, `_Static_assert`, etc.) where appropriate
2. **Always prefer GLib/GObject APIs** over raw POSIX equivalents in GTK-facing code
3. **Always check GTK version compatibility** — flag any GTK2-only API with migration suggestion
4. **When refactoring drawing code**, suggest cairo-based equivalent immediately
5. **When adding new functions**, generate the corresponding `.h` declaration
6. **When fixing memory issues**, suggest Valgrind/AddressSanitizer annotation
7. **When touching plugin interfaces**, verify ABI stability and version guards
8. **For i18n**: wrap user-visible strings in `_()` macro
9. **Search before creating**: use symbol lookups before adding duplicate utilities
10. **File issues for found bugs**: note TODOs as `/* TODO(#NN): description */`
11. **Always assign a milestone** when creating issues — v1.3.0 for GTK3 parity work, v1.4.0 for GTK4/enhancement work
12. **State the target issue/milestone** at the start of any multi-step coding session
13. **Use branch naming convention**: `fix/NNN-short-desc`, `feat/NNN-short-desc`, `gtk3/NNN-desc`
14. **Every commit resolving an issue** must include `Closes #NNN` or `Refs #NNN` in the message
