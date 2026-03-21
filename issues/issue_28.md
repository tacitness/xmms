test(skin): acquire Winamp 2.x skin corpus from archive.org and build compatibility test suite
## Goal

Before the v1.3.0 parity release we need verified skin loading coverage. The original XMMS supported Winamp 2.x skins (BMP-format ZIP archives, `.wsz`). No skin loading has been systematically tested against the GTK3 port.

## Tasks

### 1 — Acquire test skin corpus

Download a representative set of Winamp 2.x skins from the Internet Archive:
- Primary collection: https://archive.org/details/winampskins (10 000+ skins)
- `ia download winampskins --glob='*.wsz'` (Internet Archive CLI)

Minimum test set should cover:
- Default/stock skin bundled with XMMS
- Winamp Classic 2.x reference skin
- Skins with non-rectangular alpha-mask window shapes
- Skins with custom EQ and playlist overlays
- Skins with custom number fonts, title-bar styling, and multi-state buttons
- At least one corrupt/malformed skin (robustness / no-crash testing)

### 2 — Manual smoke-test matrix

For each skin in the test set:

- [ ] Loads without crash
- [ ] Main window renders correctly (buttons, sliders, volume knob, seekbar)
- [ ] Playlist window skin overlay applied
- [ ] EQ window skin overlay applied
- [ ] Window shape mask matches skin alpha channel (depends on #26 being resolved)
- [ ] Skin unloads cleanly when switching
- [ ] No memory leak on repeated load/unload (run with ASAN or Valgrind)
- [ ] Graceful fallback when optional skin components are missing (no `EQMAIN.BMP`, etc.)

### 3 — Automate via CI screenshot regression

- Add `tests/skin/` directory with a headless test harness (`Xvfb` + `xdotool` or `weston` for Wayland)
- Capture screenshot of each skin state after load
- Store golden reference images in `tests/skin/golden/`
- Compare on each PR touching `xmms/skin.c`, `xmms/main.c`, `xmms/skinwin.c` (perceptual hash diff)
- Report failures as PR check annotations

### 4 — Security: skin ZIP path traversal guard

Verify that `xmms/skin.c` rejects ZIP entries whose extracted path escapes the destination directory (e.g. `../../.bashrc`). Add a test case with a crafted malicious `.wsz`.

## References

- Internet Archive Winamp skin collection: https://archive.org/details/winampskins
- Skin format: Winamp SDK 2.x docs; `libxmms/` skin loader source
- Related: #26 (window shaping must be resolved before shape-mask tests are valid)
