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
#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <math.h>

#include "config.h"
#include "libxmms/util.h"
#include "xmms/i18n.h"
#include "xmms/plugin.h"


#define NUM_BANDS 16

/* GTK3: GdkPixmap/GdkGC removed; off-screen rendering via cairo_surface_t */
static GtkWidget *window = NULL, *area;
static cairo_surface_t *draw_surface = NULL;
static gint16 bar_heights[NUM_BANDS];
static gint timeout_tag;
static gdouble scale;

static void sanalyzer_init(void);
static void sanalyzer_cleanup(void);
static void sanalyzer_playback_start(void);
static void sanalyzer_playback_stop(void);
static void sanalyzer_render_freq(gint16 data[2][256]);

VisPlugin sanalyzer_vp = {
    NULL,
    NULL,
    0,
    NULL, /* Description */
    0,
    1,
    sanalyzer_init,           /* init */
    sanalyzer_cleanup,        /* cleanup */
    NULL,                     /* about */
    NULL,                     /* configure */
    NULL,                     /* disable_plugin */
    sanalyzer_playback_start, /* playback_start */
    sanalyzer_playback_stop,  /* playback_stop */
    NULL,                     /* render_pcm */
    sanalyzer_render_freq     /* render_freq */
};

VisPlugin *get_vplugin_info(void)
{
    sanalyzer_vp.description = g_strdup_printf(_("Simple spectrum analyzer %s"), VERSION);
    return &sanalyzer_vp;
}

#define WIDTH 250
#define HEIGHT 100

/* Pre-computed bar gradient: one ARGB32 colour per pixel row */
static guint32 bar_colors[HEIGHT];


static void sanalyzer_destroy_cb(GtkWidget *w, gpointer data)
{
    sanalyzer_vp.disable_plugin(&sanalyzer_vp);
}

static gboolean sanalyzer_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    if (draw_surface) {
        cairo_set_source_surface(cr, draw_surface, 0, 0);
        cairo_paint(cr);
    }
    return FALSE;
}

static void sanalyzer_init(void)
{
    gint i;
    if (window)
        return;

    /* Pre-compute gradient: bottom half red->yellow, top half yellow->green */
    for (i = 0; i < HEIGHT / 2; i++) {
        guint32 g = (guint32)(((gdouble)i / (HEIGHT / 2)) * 255);
        bar_colors[i] = 0xFF000000u | (0xFFu << 16) | (g << 8);
    }
    for (i = 0; i < HEIGHT / 2; i++) {
        guint32 r = (guint32)((1.0 - (gdouble)i / (HEIGHT / 2)) * 255);
        bar_colors[HEIGHT / 2 + i] = 0xFF000000u | (r << 16) | (0xFFu << 8);
    }
    scale = HEIGHT / log(256);

    /* GTK3: GTK_WINDOW_DIALOG -> GTK_WINDOW_TOPLEVEL; gdk_pixmap/gc removed */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("Spectrum analyzer"));
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(sanalyzer_destroy_cb), NULL);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_widget_destroyed), &window);
    gtk_widget_set_size_request(window, WIDTH, HEIGHT);

    draw_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);

    area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, WIDTH, HEIGHT);
    gtk_container_add(GTK_CONTAINER(window), area);
    g_signal_connect(G_OBJECT(area), "draw", G_CALLBACK(sanalyzer_draw_cb), NULL);

    gtk_widget_show(area);
    gtk_widget_show(window);
}

static void sanalyzer_cleanup(void)
{
    if (window)
        gtk_widget_destroy(window);
    if (draw_surface) {
        cairo_surface_destroy(draw_surface);
        draw_surface = NULL;
    }
}

/* GTK3: rewritten to paint via cairo_surface_t */
static gint draw_func(gpointer data)
{
    guint32 *pixels;
    gint stride, i, x, row;
    gint band_w = WIDTH / NUM_BANDS;

    if (!window || !draw_surface) {
        timeout_tag = 0;
        return FALSE;
    }

    cairo_surface_flush(draw_surface);
    pixels = (guint32 *)cairo_image_surface_get_data(draw_surface);
    stride = cairo_image_surface_get_stride(draw_surface) / 4;

    /* Clear to black */
    for (row = 0; row < HEIGHT; row++)
        for (x = 0; x < WIDTH; x++)
            pixels[row * stride + x] = 0xFF000000u;

    /* Draw each bar from the bottom up using the pre-computed gradient */
    for (i = 0; i < NUM_BANDS; i++) {
        gint h = bar_heights[i];
        gint bx = i * band_w;
        for (row = HEIGHT - h; row < HEIGHT; row++) {
            guint32 color = bar_colors[HEIGHT - 1 - row];
            for (x = bx; x < bx + band_w - 1; x++)
                pixels[row * stride + x] = color;
        }
    }

    cairo_surface_mark_dirty(draw_surface);
    gtk_widget_queue_draw(area);
    return TRUE;
}

static void sanalyzer_playback_start(void)
{
    /* GTK3: nothing special; draw_surface is updated each render_freq call */
}

static void sanalyzer_playback_stop(void)
{
    /* GTK3: GTK_WIDGET_REALIZED removed; clear surface and redraw */
    if (draw_surface && area && gtk_widget_get_realized(area)) {
        guint32 *pixels;
        gint stride, row, x;
        cairo_surface_flush(draw_surface);
        pixels = (guint32 *)cairo_image_surface_get_data(draw_surface);
        stride = cairo_image_surface_get_stride(draw_surface) / 4;
        for (row = 0; row < HEIGHT; row++)
            for (x = 0; x < WIDTH; x++)
                pixels[row * stride + x] = 0xFF000000u;
        cairo_surface_mark_dirty(draw_surface);
        gtk_widget_queue_draw(area);
    }
}


static void sanalyzer_render_freq(gint16 data[2][256])
{
    gint i, c;
    gint y;

    gint xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};

    if (!window)
        return;
    for (i = 0; i < NUM_BANDS; i++) {
        for (c = xscale[i], y = 0; c < xscale[i + 1]; c++) {
            if (data[0][c] > y)
                y = data[0][c];
        }
        y >>= 7;
        if (y != 0) {
            y = (gint)(log(y) * scale);
            if (y > HEIGHT - 1)
                y = HEIGHT - 1;
        }

        if (y > bar_heights[i])
            bar_heights[i] = y;
        else if (bar_heights[i] > 4)
            bar_heights[i] -= 4;
        else
            bar_heights[i] = 0;
    }
    draw_func(NULL);
    return;
}
