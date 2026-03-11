/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front
 * Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * TODO(#gtk3): prefswin.c GTK3 migration stub.
 *
 * The original prefswin.c used GTK+2-only widgets:
 *   - GtkCList       -> GtkTreeView + GtkListStore  (GTK3 replacement)
 *   - GtkOptionMenu  -> GtkComboBoxText              (GTK3 replacement)
 *   - GtkTooltips    -> gtk_widget_set_tooltip_text  (GTK3 replacement)
 *   - GTK_OBJECT()   -> G_OBJECT()                   (GTK3 replacement)
 *   - gtk_signal_connect() -> g_signal_connect()     (GTK3 replacement)
 *
 * The original file is preserved as prefswin.c.gtk2-bak for reference.
 * This stub provides the four exported symbols so the rest of xmms/ links.
 */

#include "prefswin.h"

#include <glib.h>
#include <gtk/gtk.h>

#include "xmms.h"

/* ---- module state ---- */

static GtkWidget *prefswin = NULL;

/* ---- public API ---- */

void create_prefs_window(void)
{
    if (prefswin != NULL)
        return;

    prefswin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(prefswin), _("XMMS Preferences  (GTK3 TODO)"));
    gtk_window_set_default_size(GTK_WINDOW(prefswin), 480, 320);
    gtk_window_set_resizable(GTK_WINDOW(prefswin), TRUE);

    /* Clear the pointer when the window is destroyed so callers can
     * recreate it cleanly after the first close. */
    g_signal_connect(prefswin, "destroy", G_CALLBACK(gtk_widget_destroyed), &prefswin);

    GtkWidget *label =
        gtk_label_new("Preferences dialog not yet ported to GTK3.\n"
                      "See xmms/prefswin.c.gtk2-bak for the original implementation.\n"
                      "Tracked in TODO(#gtk3).");
    gtk_container_add(GTK_CONTAINER(prefswin), label);
}

void show_prefs_window(void)
{
    if (prefswin == NULL)
        create_prefs_window();

    gtk_widget_show_all(prefswin);
    gtk_window_present(GTK_WINDOW(prefswin));
}

void prefswin_vplugins_rescan(void)
{
    /* TODO(#gtk3): rescan visualization plugin list and rebuild the plugin page */
}

void prefswin_show_vis_plugins_page(void)
{
    show_prefs_window();
    /* TODO(#gtk3): switch notebook tab to the visualization plugins page */
}
