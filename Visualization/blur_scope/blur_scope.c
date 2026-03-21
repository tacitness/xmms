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
 *  w
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "blur_scope.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <string.h>

#include "config.h"
#include "libxmms/configfile.h"
#include "libxmms/util.h"
#include "vis_chrome.h"
#include "xmms/i18n.h"
#include "xmms/plugin.h"
#include "xmms_logo.xpm"

/* GTK3: GdkPixmap removed; using cairo_image_surface_t for off-screen rendering */
static GtkWidget *window = NULL, *area;
static cairo_surface_t *surface = NULL;
static guint32 colors[256]; /* ARGB32 palette derived from bscope_cfg.color */
static gboolean config_read = FALSE;

static void bscope_init(void);
static void bscope_cleanup(void);
static void bscope_playback_stop(void);
static void bscope_render_pcm(gint16 data[2][512]);

BlurScopeConfig bscope_cfg;

VisPlugin bscope_vp = {
    NULL,
    NULL,
    0,                    /* XMMS Session ID, filled in by XMMS */
    NULL,                 /* description */
    1,                    /* Number of PCM channels wanted */
    0,                    /* Number of freq channels wanted */
    bscope_init,          /* init */
    bscope_cleanup,       /* cleanup */
    NULL,                 /* about */
    bscope_configure,     /* configure */
    NULL,                 /* disable_plugin */
    NULL,                 /* playback_start */
    bscope_playback_stop, /* playback_stop */
    bscope_render_pcm,    /* render_pcm */
    NULL                  /* render_freq */
};

VisPlugin *get_vplugin_info(void)
{
    bscope_vp.description = g_strdup_printf(_("Blur Scope %s"), VERSION);
    return &bscope_vp;
}

#define WIDTH 256
#define HEIGHT 128
#define min(x, y) ((x) < (y) ? (x) : (y))
#define BPL ((WIDTH + 2))

static guchar rgb_buf[(WIDTH + 2) * (HEIGHT + 2)];

static void inline draw_pixel_8(guchar *buffer, gint x, gint y, guchar c)
{
    buffer[((y + 1) * BPL) + (x + 1)] = c;
}


void bscope_read_config(void)
{
    ConfigFile *cfg;
    gchar *filename;

    if (!config_read) {
        bscope_cfg.color = 0xFF3F7F;
        filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
        cfg = xmms_cfg_open_file(filename);

        if (cfg) {
            xmms_cfg_read_int(cfg, "BlurScope", "color", &bscope_cfg.color);
            xmms_cfg_free(cfg);
        }
        g_free(filename);
        config_read = TRUE;
    }
}


#ifndef I386_ASSEM
void bscope_blur_8(guchar *ptr, gint w, gint h, gint bpl)
{
    register guint i, sum;
    register guchar *iptr;

    iptr = ptr + bpl + 1;
    i = bpl * h;
    while (i--) {
        sum = (iptr[-bpl] + iptr[-1] + iptr[1] + iptr[bpl]) >> 2;
        if (sum > 2)
            sum -= 2;
        *(iptr++) = sum;
    }
}
#else
extern void bscope_blur_8(guchar *ptr, gint w, gint h, gint bpl);
#endif

/* GTK3: populates the module-static colors[] instead of a GdkRgbCmap */
void generate_cmap(void)
{
    guint32 i, red, green, blue;

    red = bscope_cfg.color >> 16;
    green = (bscope_cfg.color >> 8) & 0xFF;
    blue = bscope_cfg.color & 0xFF;
    for (i = 255; i > 0; i--) {
        colors[i] =
            0xFF000000u | ((i * red / 256) << 16) | ((i * green / 256) << 8) | (i * blue / 256);
    }
    colors[0] = 0xFF000000u; /* fully opaque black */
}

static gboolean bscope_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    if (surface) {
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_paint(cr);
    }
    return FALSE;
}

static void bscope_destroy_cb(GtkWidget *w, gpointer data)
{
    (void)w;
    (void)data;
    /* Defer disable_plugin so we are not inside the destroy-signal emission
     * when cleanup() calls gtk_widget_destroy(window) again.  Using g_idle_add
     * mirrors the pattern used in opengl_spectrum to avoid GTK_IS_WIDGET
     * CRITICAL assertions and double-free of the cairo surface. */
    if (bscope_vp.disable_plugin)
        g_idle_add((GSourceFunc)bscope_vp.disable_plugin, &bscope_vp);
}

static void bscope_init(void)
{
    if (window)
        return;
    bscope_read_config();

    /* parity: WM titlebar replaced with XMMS-skin chrome (vis_chrome_apply) */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(bscope_destroy_cb), NULL);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_widget_destroyed), &window);
    gtk_widget_set_size_request(window, WIDTH, HEIGHT);

    area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, WIDTH, HEIGHT);
    g_signal_connect(G_OBJECT(area), "destroy", G_CALLBACK(gtk_widget_destroyed), &area);
    g_signal_connect(G_OBJECT(area), "draw", G_CALLBACK(bscope_draw_cb), NULL);
    vis_chrome_apply(GTK_WINDOW(window), area, _("Blur Scope"));

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
    generate_cmap();
    memset(rgb_buf, 0, (WIDTH + 2) * (HEIGHT + 2));

    gtk_widget_show(area);
    gtk_widget_show(window);
}

static void bscope_cleanup(void)
{
    if (window)
        gtk_widget_destroy(window);
    if (surface) {
        cairo_surface_destroy(surface);
        surface = NULL;
    }
}

static void bscope_playback_stop(void)
{
    /* GTK3: GTK_WIDGET_REALIZED macro removed; clear and redraw via queue */
    if (area && gtk_widget_get_realized(area)) {
        memset(rgb_buf, 0, (WIDTH + 2) * (HEIGHT + 2));
        gtk_widget_queue_draw(area);
    }
}

static inline void draw_vert_line(guchar *buffer, gint x, gint y1, gint y2)
{
    int y;
    if (y1 < y2) {
        for (y = y1; y <= y2; y++)
            draw_pixel_8(buffer, x, y, 0xFF);
    } else if (y2 < y1) {
        for (y = y2; y <= y1; y++)
            draw_pixel_8(buffer, x, y, 0xFF);
    } else
        draw_pixel_8(buffer, x, y1, 0xFF);
}

static void bscope_render_pcm(gint16 data[2][512])
{
    guint32 *pixels;
    gint stride, i, y, prev_y;

    if (!window || !surface)
        return;

    bscope_blur_8(rgb_buf, WIDTH, HEIGHT, BPL);
    prev_y = y = (HEIGHT / 2) + (data[0][0] >> 9);
    for (i = 0; i < WIDTH; i++) {
        y = (HEIGHT / 2) + (data[0][i >> 1] >> 9);
        if (y < 0)
            y = 0;
        if (y >= HEIGHT)
            y = HEIGHT - 1;
        draw_vert_line(rgb_buf, i, prev_y, y);
        prev_y = y;
    }

    /* GTK3: write indexed palette buffer into ARGB32 cairo_image_surface */
    cairo_surface_flush(surface);
    pixels = (guint32 *)cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface) / 4;
    for (y = 0; y < HEIGHT; y++)
        for (i = 0; i < WIDTH; i++)
            pixels[y * stride + i] = colors[rgb_buf[(y + 1) * BPL + (i + 1)]];
    cairo_surface_mark_dirty(surface);
    gtk_widget_queue_draw(area);
}
