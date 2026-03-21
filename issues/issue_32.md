build(apk): add APKBUILD for Alpine Linux packaging
## Problem

Like the `debian/` issue (#31), `package.yml` generates the `APKBUILD` on-the-fly inside the CI runner. No `APKBUILD` exists in the repo — contributors cannot locally test Alpine packaging, and the file is not version-controlled.

## Acceptance Criteria

- [ ] Add `APKBUILD` at repo root following Alpine packaging conventions
- [ ] `pkgname=xmms`, `url=` pointing to GitHub repo
- [ ] `makedepends` includes: `cmake`, `ninja`, `gtk+3.0-dev`, `glib2-dev`, `alsa-lib-dev`, `pulseaudio-dev`, `libvorbis-dev`, `libogg-dev`, `mpg123-dev`, `flac-dev`, `libx11-dev`, `libxext-dev`
- [ ] `build()` uses cmake: `cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build`
- [ ] `package()` uses `cmake --install build --destdir "$pkgdir"`
- [ ] `subpackages="$pkgname-dev:$pkgname-doc"` split
- [ ] Update `package.yml` `build-apk` job to use the checked-in `APKBUILD` (copy to `~/packages/xmms/`) rather than generating it via heredoc
- [ ] `abuild -F` succeeds on Alpine 3.20 and Alpine edge containers
