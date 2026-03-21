/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2001  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
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
#include "xmms.h"

typedef struct {
    GtkWidget *window;
    gint num_items;
    gint *nx, *ny;
    gint *sx, *sy;
    gint barx, bary;
    gint active, base;
    void (*handler)(gint item);
} PlaylistPopup;

static PlaylistPopup *popup = NULL;

static void playlist_popup_do_draw(cairo_t *cr)
{
    gint i;
    gint pw = 25, ph = popup->num_items * 18;

    /* GTK3 fix: pre-fill with skin background colour so any window area outside
     * the 25-px skin art strips is not left as opaque black (app_paintable=TRUE
     * suppresses GTK's default theme fill, and EXTEND_NONE outside the surface
     * makes unpainted pixels transparent → black on most compositors). */
    {
        GdkColor *bg = get_skin_color(SKIN_PLEDIT_NORMALBG);
        if (bg)
            cairo_set_source_rgb(cr, bg->red / 65535.0, bg->green / 65535.0, bg->blue / 65535.0);
        else
            cairo_set_source_rgb(cr, 0.0, 0.039, 0.118);
        /* cairo_paint() fills the ENTIRE allocated GdkWindow, including any
         * surplus GTK3 gives beyond our requested 25×ph — prevents black overflow. */
        cairo_paint(cr);
    }

    skin_draw_pixmap(cr, SKIN_PLEDIT, popup->barx, popup->bary, 0, 0, 3, ph);
    for (i = 0; i < popup->num_items; i++) {
        if (i == popup->active)
            skin_draw_pixmap(cr, SKIN_PLEDIT, popup->sx[i], popup->sy[i], 3, i * 18, 22, 18);
        else
            skin_draw_pixmap(cr, SKIN_PLEDIT, popup->nx[i], popup->ny[i], 3, i * 18, 22, 18);
    }
    /* gdk_flush() inside a draw callback causes recursive redraws — removed */
}

static void playlist_popup_draw(PlaylistPopup *popup)
{
    gtk_widget_queue_draw(popup->window);
}

void playlist_popup_destroy(void)
{
    if (popup) {
        gdk_pointer_ungrab(GDK_CURRENT_TIME);
        gdk_flush();
        gtk_widget_destroy(popup->window);
        g_free(popup->nx);
        g_free(popup->ny);
        g_free(popup->sx);
        g_free(popup->sy);
        if (popup->handler && popup->active != -1)
            popup->handler(popup->active + popup->base);
        g_free(popup);
        popup = NULL;
    }
}

static gboolean playlist_popup_expose(GtkWidget *widget, cairo_t *cr, gpointer callback_data)
{
    playlist_popup_do_draw(cr);
    return FALSE;
}

static void playlist_popup_motion(GtkWidget *widget, GdkEventMotion *event, gpointer callback_data)
{
    gint active;

    if (event->x >= 0 && event->x < 25 && event->y >= 0 && event->y < popup->num_items * 18) {
        active = event->y / 18;
        if (popup->active != active) {
            popup->active = active;
            playlist_popup_draw(popup);
        }
    } else if (popup->active != -1) {
        popup->active = -1;
        playlist_popup_draw(popup);
    }
}

static void playlist_popup_release(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
    playlist_popup_destroy();
}

void playlist_popup(gint x, gint y, gint num_items, gint *nx, gint *ny, gint *sx, gint *sy,
                    gint barx, gint bary, gint base, void (*handler)(gint item))
{
    if (popup)
        playlist_popup_destroy();
    popup = g_malloc0(sizeof(PlaylistPopup));
    popup->num_items = num_items;
    popup->nx = g_malloc0(sizeof(gint) * num_items);
    memcpy(popup->nx, nx, sizeof(gint) * num_items);
    popup->ny = g_malloc0(sizeof(gint) * num_items);
    memcpy(popup->ny, ny, sizeof(gint) * num_items);
    popup->sx = g_malloc0(sizeof(gint) * num_items);
    memcpy(popup->sx, sx, sizeof(gint) * num_items);
    popup->sy = g_malloc0(sizeof(gint) * num_items);
    memcpy(popup->sy, sy, sizeof(gint) * num_items);
    popup->barx = barx;
    popup->bary = bary;
    popup->handler = handler;
    popup->active = num_items - 1;
    popup->base = base;
    popup->window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_widget_set_app_paintable(popup->window, TRUE);
    gtk_widget_set_events(popup->window,
                          GDK_BUTTON_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_EXPOSURE_MASK);
    gtk_widget_realize(popup->window);

    gtk_widget_set_size_request(popup->window, 25, num_items * 18);
    g_signal_connect(G_OBJECT(popup->window), "draw", G_CALLBACK(playlist_popup_expose), NULL);
    g_signal_connect(G_OBJECT(popup->window), "motion-notify-event",
                     G_CALLBACK(playlist_popup_motion), NULL);
    g_signal_connect(G_OBJECT(popup->window), "button-release-event",
                     G_CALLBACK(playlist_popup_release), NULL);
    util_set_cursor(popup->window);
    gtk_window_move(GTK_WINDOW(popup->window), x - 1, y - 1);
    gtk_widget_show(popup->window);
    /* GTK3 fix (#21): force exact pixel size — set_size_request() only sets the
     * minimum; GTK3 may allocate a larger window leaving unpainted area black. */
    gtk_window_resize(GTK_WINDOW(popup->window), 25, num_items * 18);
    gdk_window_raise(gtk_widget_get_window(popup->window));
    gdk_flush();
    playlist_popup_draw(popup);
    gdk_pointer_grab(gtk_widget_get_window(popup->window), FALSE,
                     GDK_BUTTON_MOTION_MASK | GDK_BUTTON_RELEASE_MASK, NULL, NULL,
                     GDK_CURRENT_TIME);
    gdk_flush();
}
