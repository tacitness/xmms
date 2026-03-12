/*  XMMS - Cross-platform multimedia player
 *  Parity mod: vis_chrome — remove WM titlebar from visualizer plugin windows
 *  and replace with a skinned chrome strip matching the playlist/EQ aesthetic.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef VIS_CHROME_H
#define VIS_CHROME_H

#include <gtk/gtk.h>

/* Height of the chrome titlebar strip in pixels */
#define VIS_CHROME_BAR_HEIGHT 14

/*
 * vis_chrome_apply() — strip WM decorations from *window* and wrap *content*
 * inside a minimal XMMS-skin-style titlebar chrome (dark navy bar, label,
 * drag-to-move, × close button).
 *
 * Call this INSTEAD of:
 *   gtk_window_set_title(GTK_WINDOW(window), title);
 *   gtk_container_add(GTK_CONTAINER(window), content);
 *
 * The window must NOT yet be shown when this is called.
 * All "destroy" signal handlers already connected to *window* continue to fire
 * because the close button calls gtk_widget_destroy(window).
 */
void vis_chrome_apply(GtkWindow *window, GtkWidget *content, const char *title);

#endif /* VIS_CHROME_H */
