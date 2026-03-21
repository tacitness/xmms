bug(mainwin): child windows (playlist, EQ) do not raise to foreground on parent focus
## Description

When XMMS is obscured by another application and the user clicks the main window to bring it back into focus, the **playlist window** and **equalizer window** remain behind other windows — they do not raise to the foreground together with the main window.

The original XMMS GTK2 behaviour treated all three skin windows (main, playlist, EQ) as a single logical surface group; focusing or clicking any one of them raised all three together.

## Steps to reproduce

1. Launch XMMS — main window, playlist, and EQ are all visible
2. Open another application window so it covers XMMS completely
3. Click the XMMS main window to refocus it
4. **Expected:** all three windows come to the foreground simultaneously
5. **Actual:** main window raises; playlist and EQ remain behind other windows

## Root cause (analysis)

In GTK2 the behaviour was driven by calling `gdk_window_raise()` on all three windows whenever either received `GDK_FOCUS_IN`. The GTK3 port needs to:

1. Connect to the `focus-in-event` signal on `mainwin`, `playlistwin`, and `equalizerwin`
2. In each handler: if the other two windows are visible, call `gtk_window_present()` on them
3. Alternatively (cleaner): use `gtk_window_set_transient_for()` to declare playlist and EQ as transient children of mainwin — the window manager then handles the raise automatically as part of focus management

Option 3 is preferred as it is the standard GTK pattern and requires less manual bookkeeping.

## Files

- `xmms/main.c` — `mainwin_focus_in_cb` signal handler or `mainwin_create()`
- `xmms/playlistwin.c` — `create_playlist_window()`
- `xmms/equalizer.c` — `create_equalizer_window()`
