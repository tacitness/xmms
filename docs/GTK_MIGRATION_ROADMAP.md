# GTK Migration Roadmap — XMMS Resurrection

This document tracks the incremental migration from GTK+2 to GTK3, and
eventually GTK4.  Each row is a GitHub issue / milestone.

Legend: ⬛ Not started | 🔵 In progress | ✅ Done | 🔴 Blocked

---

## Phase 0 — Infrastructure (Milestone: v1.3.0-pre)

Issues to file:

| # | Task | Labels | Effort |
|---|------|--------|--------|
| – | Set up CMake build alongside Autotools | `build`, `gtk-migration`, `effort-medium` | M |
| – | Create `xmms/gtk_compat.h` abstraction header | `gtk-migration`, `comp:core`, `effort-small` | S |
| – | Add GTK version detection to configure/CMake | `build`, `gtk-migration`, `effort-small` | S |
| – | Enable `-Wdeprecated-declarations` in CI | `ci-cd`, `gtk-migration`, `effort-small` | S |
| – | Audit all GTK+2 deprecated API usage (grep report) | `gtk-migration`, `docs`, `effort-medium` | M |
| – | Set up visual regression screenshot baseline | `test`, `gtk-migration`, `effort-large` | L |

---

## Phase 1 — GTK3 Migration (Milestone: v1.3.0)

### 1.1 Core Drawing System

| # | Subsystem | GTK2 API → GTK3 API | Files | Effort |
|---|-----------|----------------------|-------|--------|
| – | Pixmap drawing — skin background | `GdkPixmap` → `cairo_surface_t` | `skin.c`, `skinwin.c` | L |
| – | Graphics context | `GdkGC` → `cairo_t` | `mainwin.c`, `playlistwin.c` | L |
| – | Widget drawing (`expose-event`) | `GtkWidget::expose-event` → `::draw` signal | all widgets | XL |
| – | Double-buffer rendering | `GdkPixmap` back-buffer → `cairo_surface_t` | `mainwin.c`, `playlistwin.c`, `equalizer.c` | L |
| – | Number / digit bitmaps | `GdkPixmap` slices → `GdkPixbuf` sub-pixbuf | `number.c` | M |
| – | Visualizer drawing | `GdkPixmap` → `cairo_t` on `GdkFrameClock` | `vis.c`, `svis.c` | L |
| – | Equalizer graph | `GdkPixmap` → `cairo_t` | `eq_graph.c` | M |

### 1.2 Widget System

| # | Subsystem | Migration | Files | Effort |
|---|-----------|-----------|-------|--------|
| – | PButton (push button) | `GtkWidget` subclass → proper GObject | `pbutton.c` | M |
| – | TButton (toggle button) | Same | `tbutton.c` | M |
| – | SButton (small button) | Same | `sbutton.c` | M |
| – | HSlider | Replace custom rubber-band with `GtkRange` or keep custom with cairo | `hslider.c` | M |
| – | TextBox | Replace `GdkFont` → Pango | `textbox.c` | L |
| – | MenuRow | Replace `GtkItemFactory` with `GMenu` + `GtkPopoverMenu` | `menurow.c` | M |

### 1.3 Menus & Dialogs

| # | Subsystem | Migration | Files | Effort |
|---|-----------|-----------|-------|--------|
| – | Main options menu | `GtkItemFactory` → `GMenu` + `GtkPopoverMenu` | `mainwin.c` | L |
| – | Playlist context menu | Same | `playlistwin.c` | M |
| – | About dialog | `GtkDialog` API update | `about.c` | S |
| – | Preferences dialog | Audit for deprecated widgets | `prefswin.c` | L |
| – | Directory browser | Replace `dirbrowser.c` with `GtkFileChooser` | `libxmms/dirbrowser.c` | M |

### 1.4 Theme / Skin System

| # | Subsystem | Migration | Files | Effort |
|---|-----------|-----------|-------|--------|
| – | Skin BMP loading | `GdkPixmap` → `GdkPixbuf` → `cairo` | `skin.c` | L |
| – | Skin colour extraction | `GdkColor` → `GdkRGBA` | `skin.c` | M |
| – | CSS theming layer | New `GtkCssProvider` integration | new: `skin_css.c` | L |
| – | Winamp skin archive (.wsz) | Preserve ZIP extraction; add path traversal validation | `skin.c` | M |

### 1.5 Window Management

| # | Subsystem | Migration | Files | Effort |
|---|-----------|-----------|-------|--------|
| – | Dock / snap-to-edge | X11 atom hints → `GtkWindow` geometry hints | `dock.c`, `hints.c` | L |
| – | Shade / mini-mode | Replace direct X11 code with GDK abstraction | `mainwin.c`, `sm.c` | M |
| – | Always-on-top | `gdk_window_set_keep_above()` | `hints.c` | S |

### 1.6 Event Handling

| # | Subsystem | Migration | Files | Effort |
|---|-----------|-----------|-------|--------|
| – | Keyboard shortcuts | `GtkAccelGroup` already GTK2 compatible — audit only | `mainwin.c` | S |
| – | Mouse drag for volume/balance | `GdkEvent` fields → accessor functions | `hslider.c`, `mainwin.c` | M |
| – | Drag & Drop playlist | `gtk_drag_*` → same API in GTK3 (mostly compatible) | `dnd.h`, `playlistwin.c` | M |

### 1.7 Output & Plugin APIs

| # | Subsystem | Migration | Files | Effort |
|---|-----------|-----------|-------|--------|
| – | ALSA output plugin | Audit for GTK refs (should be minimal) | `Output/alsa/` | S |
| – | PulseAudio output | Audit; add PipeWire support option | `Output/` new | M |
| – | OSS output | Mark as deprecated; keep building | `Output/OSS/` | S |
| – | mpg123 input | Replace `libmad` with `libmpg123` | `Input/mpg123/` | M |
| – | Vorbis input | Update to modern `libvorbisfile` API | `Input/vorbis/` | S |

---

## Phase 2 — GTK4 Migration (Milestone: v2.0.0)

> GTK4 is stricter: no `GtkSocket`/`GtkPlug`, DnD rewritten, menus changed.

| # | Subsystem | Migration | Effort |
|---|-----------|-----------|--------|
| – | Event controllers | `GtkEventController*` gesture API | XL |
| – | Menus | `GtkPopoverMenu` + `GMenuModel` (already done in Phase 1) | – |
| – | DnD | `GdkDrag` / `GdkDrop` new API | L |
| – | Socket/Plug | Remove; use X11/Wayland compositor window embedding alt | L |
| – | Rendering | `GdkPaintable` / `GskRenderer` for GPU-accelerated vis | XL |
| – | CSS variables | Migrate CSS to GTK4 CSS subset | M |
| – | Layout | Ensure no `gdk_window_*` direct calls remain | L |

---

## Phase 3 — Wayland Full Support (Milestone: v2.1.0)

| # | Subsystem | Task | Effort |
|---|-----------|------|--------|
| – | Window positioning | Remove all `XMoveWindow` / atom hacks | L |
| – | Fullscreen | `gtk_window_fullscreen()` only | S |
| – | Screen saver inhibit | XDG portal or `GnomeIdleMonitor` | M |
| – | Clipboard | `GdkClipboard` (GTK4) | S |
| – | Multi-monitor | `GdkMonitor` API | S |

---

## Migration Resources

- [GTK2 → GTK3 Migration Guide](https://docs.gtk.org/gtk3/migrating-2to3.html)
- [GTK3 → GTK4 Migration Guide](https://docs.gtk.org/gtk4/migrating-3to4.html)
- [Cairo drawing tutorial](https://www.cairographics.org/tutorial/)
- [GObject best practices](https://docs.gtk.org/gobject/concepts.html)
- [GLib memory management](https://docs.gtk.org/glib/memory-allocation.html)
- [Winamp skin format reference](https://github.com/WACUP/Winamp-Skin-Museum/wiki/Skin-Format)
