bug(eq): EQ presets menu appears at top-left of monitor instead of over the Presets button
## Description
Clicking the **Presets** button on the Equalizer window pops up the presets
menu at the top-left corner of the active monitor instead of near the button.

## Root Cause
`equalizerwin_presets_pushed()` calls `util_get_root_pointer()` to obtain
the menu anchor position. Under Wayland (and some GTK3/X11 composited sessions)
`gdk_device_get_position()` returns `(0, 0)` for global coordinates.
`util_menu_position()` clamps that to `(0-2, 0-2)` → `(0, 0)`, placing
the menu at the origin of the primary monitor.

```c
// equalizer.c — equalizerwin_presets_pushed()
void equalizerwin_presets_pushed(void)
{
    gint x, y;
    util_get_root_pointer(&x, &y);   // returns (0,0) on Wayland/composited
    util_item_factory_popup(equalizerwin_presets_menu, x, y, 1, GDK_CURRENT_TIME);
}
```

## Fix
Calculate screen position from the EQ window position plus the presets button
offset (button is at x=217, y=18, height=12 within the 275×116 EQ window):
```c
void equalizerwin_presets_pushed(void)
{
    gint wx, wy;
    gtk_window_get_position(GTK_WINDOW(equalizerwin), &wx, &wy);
    util_item_factory_popup(equalizerwin_presets_menu,
                            wx + 217, wy + 18 + 12, 1, GDK_CURRENT_TIME);
}
```
