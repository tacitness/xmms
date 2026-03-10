# Contributing to XMMS Resurrection

Welcome — and thank you for helping resurrect the best audio player ever made! 🎵

## Table of Contents

1. [Quick-Start Checklist](#quick-start)
2. [Development Environment Setup](#dev-setup)
3. [Branching & Workflow](#branching)
4. [Commit Message Rules](#commits)
5. [Code Style](#code-style)
6. [Pre-Commit Hooks](#pre-commit)
7. [Pull Requests](#pull-requests)
8. [Issue Tracking](#issues)
9. [GTK Migration Guidelines](#gtk-migration)
10. [Testing](#testing)
11. [Security Reports](#security)

---

## Quick-Start Checklist {#quick-start}

```bash
# 1. Fork + clone
git clone https://github.com/<you>/xmms.git
cd xmms
git remote add upstream https://github.com/xmms-resurrection/xmms.git

# 2. Bootstrap dev environment (installs all tools + pre-commit hooks)
./scripts/setup-dev.sh

# 3. Create your working branch
git checkout develop
git pull upstream develop
git checkout -b feat/your-awesome-feature

# 4. Build
autoreconf -fi
./configure --prefix=/usr --enable-alsa
make -j$(nproc)

# 5. Hack, test, commit
git add -p
git commit   # pre-commit hooks run automatically

# 6. Push + open PR
git push origin feat/your-awesome-feature
```

---

## Development Environment Setup {#dev-setup}

Run the bootstrap script once:

```bash
./scripts/setup-dev.sh
```

This installs:
- GCC, Clang, clang-format 18, clang-tidy 18
- GTK3/GTK4 dev headers, GLib, ALSA, PulseAudio, codec libs
- CMake ≥ 3.20, Meson ≥ 0.60, Ninja
- pre-commit, commitizen, detect-secrets (Python tools)
- GitHub CLI (`gh`)
- RPM/DEB build tools (if `--minimal` not passed)

Supported distros: Fedora/RHEL, Debian/Ubuntu, Arch Linux, Alpine.

---

## Branching & Workflow {#branching}

```
main          ← stable release, tagged
develop       ← integration; all feature PRs target here
feature/...   ← new features (from develop)
fix/...       ← bug fixes (from develop; from main for hotfixes)
gtk3/...      ← GTK3 migration work
gtk4/...      ← GTK4 migration work
release/X.Y   ← release preparation
```

**Rule:** Do **not** commit directly to `main` or `develop`.  
Open a PR; it must pass CI before merging.

---

## Commit Message Rules {#commits}

We enforce [Conventional Commits v1.0](https://www.conventionalcommits.org/).  
The pre-commit `commit-msg` hook will **reject** non-conforming messages.

```
<type>(<scope>): <summary in imperative mood, ≤72 chars>

[optional body — wrap at 72 chars]

[optional footer: Closes #123, BREAKING CHANGE: ...]
```

**Types:** `feat`, `fix`, `refactor`, `style`, `docs`, `test`, `build`, `ci`, `chore`, `perf`, `gtk3`, `gtk4`, `revert`

**Scopes (examples):** `skin`, `playlist`, `eq`, `input`, `output`, `vis`, `prefs`, `core`, `libxmms`, `alsa`, `pulse`, `mp3`, `ogg`, `build`, `ci`, `docs`, `i18n`

**Good examples:**
```
feat(skin): load Winamp 2.x skins via GdkPixbuf
fix(playlist): prevent use-after-free in playlist_clear()
gtk3(mainwin): replace GdkPixmap with cairo_surface_t
build(cmake): add FindGTK4.cmake module
ci: add Fedora 42 matrix entry
```

---

## Code Style {#code-style}

- **Language standard:** C11 (`-std=c11`)
- **Indentation:** 4 spaces — **no tabs** in `.c`/`.h` files
- **Brace style:** K&R (opening brace on same line)
- **Line length:** 100 characters maximum
- **Pointer alignment:** `int *ptr` (type-side, not `int* ptr`)
- **Naming:** `snake_case` functions/variables, `SCREAMING_SNAKE` macros/constants
- **Include order:**
  1. Own header (e.g., `#include "playlist.h"` in `playlist.c`)
  2. System/library headers (`<gtk/gtk.h>`, `<glib.h>`, `<stdlib.h>`)
  3. Project headers (`"xmms.h"`, `"skin.h"`)
  - Blank line between each group

Run `./scripts/format-check.sh --fix` to auto-format.  
`clang-format` runs automatically via pre-commit on every commit.

**GTK API mandatories:**
- Never introduce GTK+2-only APIs without a `#if !GTK_CHECK_VERSION(3,0,0)` guard
- Draw with `cairo_t` — never `GdkPixmap` / `GdkGC`
- i18n: wrap every user-visible string in `_()`
- Use `g_snprintf()` not `sprintf()`; never `gets()`

---

## Pre-Commit Hooks {#pre-commit}

`setup-dev.sh` installs the hooks automatically. To run manually:

```bash
pre-commit run --all-files          # run all checks
pre-commit run clang-format         # format only
pre-commit run shellcheck           # shell scripts only
pre-commit run codespell            # spell-check only
pre-commit autoupdate               # update hook versions
```

Hooks that run on every `git commit`:

| Hook | What it does |
|------|-------------|
| `clang-format` | Auto-formats `.c`/`.h` files to project style |
| `clang-tidy` | Static analysis (requires `build/compile_commands.json`) |
| `codespell` | Spell-checks code comments and docs |
| `shellcheck` | Lints shell scripts |
| `trailing-whitespace` | Removes trailing spaces |
| `end-of-file-fixer` | Ensures files end with newline |
| `check-merge-conflict` | Blocks accidental conflict markers |
| `conventional-pre-commit` | Validates commit message format |
| `detect-secrets` | Prevents accidentally committing credentials |

---

## Pull Requests {#pull-requests}

1. **Open a GH issue first** — no orphaned PRs
2. **One concern per PR** — don't mix a feature and a refactor
3. **Title** must follow Conventional Commits format
4. **Description** must include:
   - What changed and why
   - How it was tested
   - `Closes #NNN` or `Refs #NNN`
5. **Draft PRs** are encouraged for WIP; convert to Ready when CI is green
6. **Minimum 1 approval** from a maintainer
7. **Squash and merge** for feature branches

---

## Issue Tracking {#issues}

- File a GH issue for **every** bug, feature, or migration task
- Use the [bug report](.github/ISSUE_TEMPLATE/bug_report.yml),
  [feature request](.github/ISSUE_TEMPLATE/feature_request.yml), or
  [GTK migration](.github/ISSUE_TEMPLATE/gtk_migration.yml) templates
- Label your issue appropriately (maintainers will triage)
- Mark TODOs in code as `/* TODO(#NNN): description */`

---

## GTK Migration Guidelines {#gtk-migration}

The GTK3/4 migration is the top priority. Key rules:

- **One component per PR** — e.g., one PR for `skin.c` cairo migration
- Use [gtk_migration.yml](.github/ISSUE_TEMPLATE/gtk_migration.yml) template
- Document every deprecated API you replace:
  ```c
  /* GTK3: replaced GdkPixmap with cairo_surface_t (Refs #42) */
  ```
- Use `xmms/gtk_compat.h` (to be created) as abstraction layer
- Test on both GTK3 and GTK4 if possible
- Reference: [GTK3 Migration Guide](https://docs.gtk.org/gtk3/migrating-2to3.html) |
  [GTK4 Migration Guide](https://docs.gtk.org/gtk4/migrating-3to4.html)

---

## Testing {#testing}

```bash
# Run all tests
make check

# Run with verbose output
make check VERBOSE=1

# Run ASAN build
CC=clang CFLAGS="-fsanitize=address,undefined -g -O1" ./configure && make -j$(nproc)
```

New code should include unit tests in `tests/` using
[Check](https://libcheck.github.io/check/) or [cmocka](https://cmocka.org/).

---

## Security Reports {#security}

**Please do not file security vulnerabilities as public GH issues.**

Report security bugs privately via:
- GitHub's [Private Vulnerability Reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing/privately-reporting-a-security-vulnerability) feature
- Email: `security@xmms-resurrection.example.com`

Include: description, reproduction steps, impact assessment, and suggested fix if known.
