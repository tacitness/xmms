/*  XMMS - Cross-platform multimedia player
 *  libxmms/snap.h — vis-plugin snap/dock registration vtable
 *
 *  Allows visualization plugin DSOs (which cannot directly link against the
 *  main xmms binary) to participate in the dock/snap window system.
 *
 *  The main binary fills in xmms_snap at startup; all pointers are NULL
 *  until then — callers must always guard with a NULL check before calling.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef XMMS_SNAP_H
#define XMMS_SNAP_H

#include <gtk/gtk.h>

/*
 * XmmsSnapInterface — vtable populated by the main xmms binary at startup
 * and consumed by vis plugin DSOs via vis_chrome.c.
 *
 * Using a vtable (rather than weak symbols) keeps the interface clean across
 * the DSO boundary: the global lives in libxmms.so which is linked by both
 * the main binary and every plugin DSO.
 */
typedef struct {
    /* Register a vis window into the snap/dock group (dock_window_list). */
    void (*register_window)(GtkWidget *w);
    /* Remove a vis window from the snap/dock group on destroy. */
    void (*unregister_window)(GtkWidget *w);
    /* Snap-aware drag — equivalent to dock_move_press / motion / release. */
    void (*move_press)(GtkWidget *w, GdkEventButton *ev);
    void (*move_motion)(GtkWidget *w, GdkEventMotion *ev);
    void (*move_release)(GtkWidget *w);
} XmmsSnapInterface;

/* Defined in snap.c; zero-initialised until main binary calls vis_snap_init() */
extern XmmsSnapInterface xmms_snap;

#endif /* XMMS_SNAP_H */
