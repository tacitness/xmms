build(deb): add debian/ packaging directory for dpkg-buildpackage
## Problem

Currently `package.yml` generates all `debian/` files (changelog, control, rules, compat) on-the-fly inside the CI runner via heredocs. This is:

- **Unmaintainable** — changes require editing CI YAML, not version-controlled files
- **Non-standard** — every Debian/Ubuntu distro expects a real `debian/` directory
- **Not locally testable** — contributors can't run `dpkg-buildpackage` without running CI

## Acceptance Criteria

- [ ] Add `debian/` directory to repo with:
  - `debian/control` — correct `Build-Depends` for GTK3 + CMake (libgtk-3-dev, cmake, ninja-build, libpulse-dev, etc.)
  - `debian/rules` — cmake-based build: `dh $@` with `override_dh_auto_configure` using cmake
  - `debian/changelog` — valid initial entry for `1.3.0-1`
  - `debian/compat` → replaced by `debhelper-compat (= 13)` in control
  - `debian/copyright` — GPL-2.0 declaration
  - `debian/source/format` — `3.0 (quilt)`
  - `debian/watch` — GitHub releases watch URL
- [ ] Update `package.yml` `build-deb` job to use the checked-in `debian/` dir instead of the heredoc generation
- [ ] `lintian --pedantic` produces zero errors on Debian 12 / Ubuntu 24.04
- [ ] Package installs and `xmms --version` runs cleanly on Ubuntu 24.04 container

## Notes

- Use `debhelper-compat (= 13)` — compat 13 is current standard
- `DH_VERBOSE=1` in `debian/rules` for debug builds
- CMake `-DCMAKE_INSTALL_PREFIX=/usr` and `-DCMAKE_BUILD_TYPE=RelWithDebInfo`
