feat(vis): port in-window visualizations to cairo and enable external vis plugins
## Overview

Two separate visualization systems need work:

### 1. In-window Visualizer (mainwin small vis widget + svis shaded widget)

The built-in analyzer/scope/VU meter in the main window (`xmms/vis.c`, `xmms/svis.c`) currently draws into the skin backing surface via`cairo_t`. This is **working** (visualizations render in the main window).

However the **doublesize mode** path uses removed GTK2 API:
```c
// equalizer.c ~line 268 — still TODO stubs:
img  = NULL /* TODO(#gtk3): gdk_image_get removed */;
img2 = NULL /* TODO(#gtk3): create_dblsize_image removed */;
/* TODO(#gtk3): gdk_draw_image removed */
```

GTK3 replacement: use `cairo_surface_create_similar()` + `cairo_scale(2, 2)` to produce the 2× surface without `GdkImage`.

### 2. External Visualization Plugins (Visualization/ directory)

The three existing external vis plugins are built as shared libraries:
- `Visualization/blur_scope/` — oscilloscope with blur effect
- `Visualization/sanalyzer/` — stereo spectrum analyzer  
- `Visualization/opengl_spectrum/` — OpenGL spectrum (requires GLX)

**Current status:** Plugins build successfully. However the plugin selection UI (in the Preferences dialog) is the stub from issue #15 — users cannot enable/configure vis plugins without a working prefs dialog.

**Additional issue:** External vis plugins originally used `GtkSocket`/`GtkPlug` to embed their drawing canvas into the XMMS visualization widget area. `GtkSocket`/`GtkPlug` are removed in GTK4 and have limited support under Wayland. Under X11/GTK3 they still work. This should be documented and a Wayland fallback path designed.

### 3. Voiceprint / VU mode

`VIS_VOICEPRINT` mode was present in the original code path but was removed during migration (stubs exist for it). Can be re-added alongside the spectrum analyzer once the drawing path is confirmed stable.

## Dependencies

- **Blocked by #15** (prefs dialog) for external plugin enable/configure UI
- Doublesize path can be fixed independently
