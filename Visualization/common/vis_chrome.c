/*  XMMS - Cross-platform multimedia player
 *  Parity mod: vis_chrome — XMMS-skin-style titlebar chrome for vis plugins.
 *
 *  Replaces the standard WM titlebar on visualization plugin windows with a
 *  slim dark-navy strip that matches the playlist-editor / equalizer aesthetic,
 *  complete with a drag-to-move title and a close button.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include "vis_chrome.h"

#include <gtk/gtk.h>

/* ---------------------------------------------------------------------------
 * CSS — colours taken from the Winamp default skin PLEDIT titlebar palette:
 *   bg active   #0e2030   very dark navy
 *   bg inactive #0a1820   slightly darker when unfocused
 *   text        #6ba0b5   muted steel-blue
 *   border      #1a384e   1 px bottom separator
 *   hover-close #1a384e   darkened for close-button hover
 * --------------------------------------------------------------------------- */
static const char VIS_CHROME_CSS[] =
    /* titlebar event-box */
    ".vis-chrome-bar {"
    "    background-color: #0e2030;"
    "    border-bottom: 1px solid #1a384e;"
    "    padding: 0px 0px;"
    "}"
    /* inner horizontal layout box */
    ".vis-chrome-inner {"
    "    min-height: 14px;"
    "    padding: 1px 2px;"
    "}"
    /* plugin name label */
    ".vis-chrome-title {"
    "    color: #6ba0b5;"
    "    font-size: 8pt;"
    "    padding-left: 4px;"
    "}"
    /* close button */
    ".vis-chrome-close {"
    "    background: none;"
    "    border: none;"
    "    box-shadow: none;"
    "    outline: none;"
    "    color: #6ba0b5;"
    "    font-size: 8pt;"
    "    padding: 0px 5px;"
    "    min-width: 0;"
    "    min-height: 0;"
    "    border-radius: 0;"
    "}"
    ".vis-chrome-close:hover {"
    "    background-color: #1a384e;"
    "    color: #c8e0f0;"
    "}"
    ".vis-chrome-close:active {"
    "    background-color: #0a1820;"
    "    color: #ffffff;"
    "}";

/* ---------------------------------------------------------------------------
 * Titlebar mouse-button handler — begin_move_drag on left-button press
 * --------------------------------------------------------------------------- */
static gboolean on_bar_press(GtkWidget *widget, GdkEventButton *ev, gpointer data)
{
    (void)widget;
    if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS) {
        gtk_window_begin_move_drag(GTK_WINDOW(data), (gint)ev->button, (gint)ev->x_root,
                                   (gint)ev->y_root, ev->time);
        return TRUE;
    }
    return FALSE;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */
void vis_chrome_apply(GtkWindow *window, GtkWidget *content, const char *title)
{
    static gboolean css_loaded = FALSE;
    GtkWidget *vbox, *bar, *bar_inner, *lbl, *close_btn;

    /* Load the CSS provider once for the entire session */
    if (!css_loaded) {
        GtkCssProvider *prov = gtk_css_provider_new();
        gtk_css_provider_load_from_data(prov, VIS_CHROME_CSS, -1, NULL);
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                  GTK_STYLE_PROVIDER(prov),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(prov);
        css_loaded = TRUE;
    }

    /* Remove WM titlebar — must be called before gtk_widget_show() */
    gtk_window_set_decorated(window, FALSE);
    /* Borderless windows still need a frame for re-stacking; skip resizable here
     * — each plugin controls that independently via gtk_window_set_resizable(). */

    /* ------------------------------------------------------------------
     * Layout:
     *   vbox
     *   ├── bar (GtkEventBox, "vis-chrome-bar")
     *   │    └── bar_inner (GtkBox H, "vis-chrome-inner")
     *   │         ├── lbl   ("vis-chrome-title", expand)
     *   │         └── close_btn ("vis-chrome-close")
     *   └── content (caller's drawing area / GL area)
     * ------------------------------------------------------------------ */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Titlebar — GtkEventBox so we can paint the background via CSS */
    bar = gtk_event_box_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), "vis-chrome-bar");
    gtk_widget_add_events(bar, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(bar, "button-press-event", G_CALLBACK(on_bar_press), window);

    bar_inner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar_inner), "vis-chrome-inner");
    gtk_container_add(GTK_CONTAINER(bar), bar_inner);

    lbl = gtk_label_new(title);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "vis-chrome-title");
    gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(bar_inner), lbl, TRUE, TRUE, 0);

    close_btn = gtk_button_new_with_label("\xc3\x97"); /* UTF-8 × (U+00D7) */
    gtk_style_context_add_class(gtk_widget_get_style_context(close_btn), "vis-chrome-close");
    gtk_style_context_remove_class(gtk_widget_get_style_context(close_btn), "button");
    gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
    gtk_widget_set_valign(close_btn, GTK_ALIGN_CENTER);
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_widget_destroy), window);
    gtk_box_pack_end(GTK_BOX(bar_inner), close_btn, FALSE, FALSE, 0);

    /* Assemble vbox */
    gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), content, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* Show all internal chrome widgets (bar, label, close button).
     * The window itself is shown by the caller after this returns. */
    gtk_widget_show_all(vbox);
}
