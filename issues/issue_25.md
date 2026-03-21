bug(mainwin): child windows (playlist, EQ) do not raise to foreground on parent focus
## Description

When XMMS is obscured by another application window and the user clicks the XMMS main window to bring it back into focus, the **playlist window** and **equalizer window** remain beneath other windows — they do not raise together with the main window.

The original XMMS GTK2 behaviour was that all three skin windows (main, playlist, EQ) act as a single surface group: focusing or clicking any one of them brings all three to the foreground together.

## Steps to Reproduce

1. Launch XMMS (main window + playlist + EQ visible)
2. Open another application/window that covers XMMS
3. Click the XMMS main window to refocus
4. **Expected:** all three windows come to the foreground together
5. **Actual:** main window raises; playlist and EQ remain behind other windows

## Root Cause (analysis)

In GTK2, this was handled via `gdk_window_raise()` called on all transient windows when the main window received `GDK_FOCUS_IN`.  The GTK3 port needs to:

- Connect to the `focus-in-event` signal on `mainwin` (and each child window)
- On focus-in: call `gtk_window_present()` on playlist and EQ windows if they are visible
- Consider using `gtk_window_set_transient_for()` to make playlist/EQ transient children of mainwin, which causes the window manager to handle raising automatically

## Files

`xmms/main.c` — `mainwin_focus_in_cb` or equivalent signal handler
`xmms/playlistwin.c`
`xmms/equalizer.c`
