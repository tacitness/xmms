gtk3(mainwin): replace GtkFixed pixel layout with GtkOverlay for main window
The main window (xmms/mainwin.c) uses GtkFixed with absolute pixel positioning to lay out the classic Winamp-style controls. This is partially supported in GTK3 but needs refactoring for Wayland compatibility.

Migration path:
- Replace direct GdkWindow X11 position calls with GtkWindow hints
- Use GtkOverlay for stacked drawing of skin layers
- Ensure Wayland compatibility (no raw GDK_DISPLAY() X11 calls)
- Preserve pixel-perfect skin layout compatibility
