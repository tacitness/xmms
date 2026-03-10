/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Small modification of the entry widget where keyboard navigation
 * works even when the entry is not editable.
 * Copyright 2003 Haavard Kvaalen <havardk@xmms.org>
 */

/* GTK3: xentry.c — GtkEntry subclass with keyboard navigation for
 * non-editable entries, rewritten using G_DEFINE_TYPE.
 *
 * Original: Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald
 *           Modified 2003 Haavard Kvaalen <havardk@xmms.org>
 * GTK3 port: removed GTK1/2 GtkTypeInfo system, GdkWChar, GdkI18n internals.
 */

#include "xentry.h"

#include <gdk/gdkkeysyms.h>

/* GTK3: G_DEFINE_TYPE replaces GtkTypeInfo / gtk_type_unique() / GtkClassInitFunc */
struct _XmmsEntry {
    GtkEntry parent_instance;
};

G_DEFINE_TYPE(XmmsEntry, xmms_entry, GTK_TYPE_ENTRY)

static gboolean xmms_entry_key_press(GtkWidget *widget, GdkEventKey *event)
{
    GtkEditable *editable = GTK_EDITABLE(widget);
    gint pos;

    /* When editable, let GtkEntry handle everything normally */
    if (gtk_editable_get_editable(editable))
        return GTK_WIDGET_CLASS(xmms_entry_parent_class)->key_press_event(widget, event);

    /* Non-editable: provide keyboard navigation */
    switch (event->keyval) {
    /* GTK3: GDK_Insert -> GDK_KEY_Insert */
    case GDK_KEY_Insert:
        if (event->state & GDK_CONTROL_MASK) {
            gtk_editable_copy_clipboard(editable);
            return TRUE;
        }
        break;

    /* GTK3: GDK_Home -> GDK_KEY_Home */
    case GDK_KEY_Home:
        gtk_editable_set_position(editable, 0);
        return TRUE;

    /* GTK3: GDK_End -> GDK_KEY_End */
    case GDK_KEY_End:
        gtk_editable_set_position(editable, -1);
        return TRUE;

    /* GTK3: GDK_Left -> GDK_KEY_Left */
    case GDK_KEY_Left:
        pos = gtk_editable_get_position(editable);
        if (pos > 0)
            gtk_editable_set_position(editable, pos - 1);
        return TRUE;

    /* GTK3: GDK_Right -> GDK_KEY_Right */
    case GDK_KEY_Right:
        pos = gtk_editable_get_position(editable);
        gtk_editable_set_position(editable, pos + 1);
        return TRUE;

    /* GTK3: GDK_Return -> GDK_KEY_Return */
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
        gtk_widget_activate(widget);
        return TRUE;

    default:
        if (event->state & GDK_CONTROL_MASK) {
            switch (event->keyval) {
            case GDK_KEY_a: /* Ctrl+A: beginning of line */
                gtk_editable_set_position(editable, 0);
                return TRUE;
            case GDK_KEY_e: /* Ctrl+E: end of line */
                gtk_editable_set_position(editable, -1);
                return TRUE;
            case GDK_KEY_c: /* Ctrl+C: copy */
                gtk_editable_copy_clipboard(editable);
                return TRUE;
            default:
                break;
            }
        }
        break;
    }

    return GTK_WIDGET_CLASS(xmms_entry_parent_class)->key_press_event(widget, event);
}

static void xmms_entry_class_init(XmmsEntryClass *klass)
{
    /* GTK3: GtkWidgetClass cast via G_DEFINE_TYPE helpers */
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    widget_class->key_press_event = xmms_entry_key_press;
}

static void xmms_entry_init(XmmsEntry *self)
{
    (void)self; /* no per-instance initialisation needed */
}

GtkWidget *xmms_entry_new(void)
{
    /* GTK3: gtk_type_new() -> g_object_new() */
    return GTK_WIDGET(g_object_new(XMMS_TYPE_ENTRY, NULL));
}
