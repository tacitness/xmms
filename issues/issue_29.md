feat(skin): built-in skin editor for creating and modifying Winamp 2.x skins
## Summary

Add a built-in skin editor / skin creation tool to XMMS, allowing users to create and modify Winamp 2.x compatible skins without needing an external image editor.

## Motivation

Currently users must use GIMP or another image editor plus knowledge of the Winamp skin format to create custom skins. A native editor would lower the barrier significantly and reinvigorate the XMMS skin ecosystem.

## Proposed scope (v1.x enhancement, post-parity)

### Minimum viable skin editor
- Open an existing `.wsz` skin and present its component bitmaps for editing
- Integrated colour picker and basic draw/fill tools (no external dep on GIMP)
- Live preview: apply edits to a running XMMS instance in real time
- Export to `.wsz` (ZIP archive) with correct file names and structure

### Stretch goals
- Import from GIMP `.xcf` via Script-Fu / batch export template
- Procedural/texture-based skin generation (noise, gradients, patterns)
- AI-assisted colour scheme generation (post-v1.4 future concept)
- Skin pack browsing/download UI backed by the Internet Archive collection (see #27)

## Technical approach

- GTK4 `GtkDrawingArea`-based canvas widget per skin component bitmap
- GdkPixbuf for pixel manipulation; cairo for overlay/preview rendering
- Can ship as a `General/` plugin that adds a "Skin Editor" entry under Options menu
- Skin format: document in `docs/skin-format.md` with full bitmap layout reference

## Priority

**P3 — low priority. Target: post v1.4.0 (v1.5.0 or later).** Do not start until GTK4 migration (#7, #8) is complete.

## Out of scope

- Winamp 3.x / modern skin format support
- Vector / SVG skin format (separate future issue)
