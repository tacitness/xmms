build(rpm): rewrite xmms.spec for GTK3 + CMake build system
## Problem

`xmms.spec` / `xmms.spec.in` are original XMMS 1.2.11 spec files from the GTK+1 era:

- `Version: 1.2.11` — stale; must derive from git tag / CMake
- `Requires: gtk+ >= 1:1.2.2` — GTK+1, dead
- `BuildPrereq: gtk+-devel` — GTK+1/2 macro, dead
- Build steps call `./configure` (autotools) — we use CMake now
- Conditional probes via `rpm -q` — outdated pattern
- No PulseAudio, cairo, libopenmpt, libxext deps
- ESD sub-package references dead ESD daemon

## Acceptance Criteria

- [ ] `Version` injected at build time from git tag or `-D_version=` macro (already done in `package.yml`)
- [ ] `BuildRequires` updated: `cmake >= 3.20`, `ninja-build`, `gtk3-devel`, `cairo-devel`, `alsa-lib-devel`, `pulseaudio-libs-devel`, `libvorbis-devel`, `libogg-devel`, `mpg123-devel`, `flac-devel`, `libX11-devel`, `libXext-devel`, `gettext-devel`
- [ ] Build steps replaced with `cmake -B build … && cmake --build build && cmake --install build`
- [ ] ESD sub-package removed
- [ ] Plugin sub-packages reflect current plugin directory layout
- [ ] `%description` updated to describe GTK3 Resurrection
- [ ] `Vendor` / `Url` updated to resurrection repo
- [ ] `rpmlint` passes with zero errors on Fedora 41 (warnings accepted)
- [ ] `xmms.spec.in` either removed or kept in sync (prefer removing; version comes from git)

## Dependencies

Depends on #30 (CMake install rules must be complete for `cmake --install` to work)
