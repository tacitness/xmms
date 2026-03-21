bug(skin): window shaping not applied to main window and equalizer window
## Description

The main window and equalizer window render as standard **rectangular GTK windows** instead of adopting the non-rectangular skin shape. The playlist window shaping was already completed via `cairo_region_t` in `playlistwin_create_mask()` but the equivalent is still stubbed in mainwin and equalizerwin.

**Note:** The Preferences dialog, Skin Chooser, and all other standard GTK dialogs are **intentionally rectangular** — they are not skin windows and do not require shaping.

## Stubbed locations

```
xmms/main.c:803    /* TODO(#gtk3): gtk_widget_shape_combine_mask removed */
xmms/main.c:2773   nullmask = NULL; /* TODO(#gtk3): cairo_image_surface_create */
xmms/main.c:2779   /* TODO(#gtk3): gtk_widget_shape_combine_mask removed */
xmms/main.c:4263   /* TODO(#gtk3): gtk_widget_shape_combine_mask removed */
xmms/equalizer.c:136 /* TODO(#gtk3): gtk_widget_shape_combine_mask removed */
```

## Required changes

### mainwin (`xmms/main.c`)

- Implement `nullmask` creation: `cairo_image_surface_create(CAIRO_FORMAT_A1, w, h)` then fill fully opaque via `cairo_paint()`; extract region with `gdk_cairo_region_create_from_surface()`
- Replace all `gtk_widget_shape_combine_mask(widget, bitmap, 0, 0)` calls with `gtk_widget_shape_combine_region(widget, region)`
- Implement `mainwin_bg_dblsize` cairo surface for 2× doublesize mode
- Remove the dead `gdk_gc_new` / `gdk_gc_set_foreground` / `gdk_draw_rectangle` / `gdk_gc_destroy` stubs

### equalizerwin (`xmms/equalizer.c`)

- Implement `equalizerwin_set_shape_mask()` using the same `cairo_region_t` pattern
- The EQ window is a fixed 275×116 px surface — apply a fully-opaque rectangular region (same result as original GTK2 code) or read alpha from the skin surface if the skin provides one

## Reference

- Playlist shaping (done — use as template): `xmms/playlistwin.c:332–341`
- Parent tracking issue: #24 (sub-task 3 — window shaping)
