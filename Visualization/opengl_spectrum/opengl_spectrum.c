/*  XMMS - OpenGL Spectrum Analyzer visualization plugin
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Tech
 *  GTK3/GtkGLArea port  Copyright (C) 2024  XMMS Resurrection Project
 *
 *  Original: used raw X11/GLX + pthread draw-loop.
 *  GTK3 port: GtkGLArea + g_timeout_add animation + GTK key/window signals.
 *  No separate render thread needed -- GTK main loop drives everything.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <math.h>
#include <string.h>

/* GtkGLArea requires epoxy; include BEFORE gtk/gtk.h */
#include <gtk/gtk.h>

#include <epoxy/gl.h>

#include "config.h"
#include "libxmms/configfile.h"
#include "libxmms/util.h"
#include "opengl_spectrum.h"
#include "xmms/i18n.h"
#include "xmms/plugin.h"

#define NUM_BANDS 16
#define ANIM_MS 16 /* ~60 fps */

OGLSpectrumConfig oglspectrum_cfg;

static GLfloat y_angle = 45.0f, y_speed = 0.5f;
static GLfloat x_angle = 20.0f, x_speed = 0.0f;
static GLfloat z_angle = 0.0f, z_speed = 0.0f;

static GLfloat heights[16][16];
static GLfloat scale;

static GtkWidget *win = NULL;
static GtkWidget *gl_area = NULL;
static guint anim_id = 0;
static gboolean gl_ready = FALSE;

static void oglspectrum_init(void);
static void oglspectrum_cleanup(void);
static void oglspectrum_playback_start(void);
static void oglspectrum_playback_stop(void);
static void oglspectrum_render_freq(gint16 data[2][256]);

VisPlugin oglspectrum_vp = {
    NULL,
    NULL,
    0,
    NULL, /* description -- set in get_vplugin_info */
    0,
    1,
    oglspectrum_init,
    oglspectrum_cleanup,
    NULL,
    oglspectrum_configure,
    NULL,
    oglspectrum_playback_start,
    oglspectrum_playback_stop,
    NULL,
    oglspectrum_render_freq,
};

VisPlugin *get_vplugin_info(void)
{
    oglspectrum_vp.description = g_strdup_printf(_("OpenGL Spectrum analyzer %s"), VERSION);
    return &oglspectrum_vp;
}

/* --------------------------------------------------------------------------
 * OpenGL rendering (OpenGL 1.x fixed-function, compatibility context)
 * -------------------------------------------------------------------------- */

static void draw_rectangle(GLfloat x1, GLfloat y1, GLfloat z1, GLfloat x2, GLfloat y2, GLfloat z2)
{
    if (y1 == y2) {
        glVertex3f(x1, y1, z1);
        glVertex3f(x2, y1, z1);
        glVertex3f(x2, y2, z2);
        glVertex3f(x2, y2, z2);
        glVertex3f(x1, y2, z2);
        glVertex3f(x1, y1, z1);
    } else {
        glVertex3f(x1, y1, z1);
        glVertex3f(x2, y1, z2);
        glVertex3f(x2, y2, z2);
        glVertex3f(x2, y2, z2);
        glVertex3f(x1, y2, z1);
        glVertex3f(x1, y1, z1);
    }
}

static void draw_bar(GLfloat x_off, GLfloat z_off, GLfloat h, GLfloat r, GLfloat g, GLfloat b)
{
    const GLfloat w = 0.1f;
    glColor3f(r, g, b);
    draw_rectangle(x_off, h, z_off, x_off + w, h, z_off + 0.1f);
    draw_rectangle(x_off, 0.0f, z_off, x_off + w, 0.0f, z_off + 0.1f);
    glColor3f(0.5f * r, 0.5f * g, 0.5f * b);
    draw_rectangle(x_off, 0.0f, z_off + 0.1f, x_off + w, h, z_off + 0.1f);
    draw_rectangle(x_off, 0.0f, z_off, x_off + w, h, z_off);
    glColor3f(0.25f * r, 0.25f * g, 0.25f * b);
    draw_rectangle(x_off, 0.0f, z_off, x_off, h, z_off + 0.1f);
    draw_rectangle(x_off + w, 0.0f, z_off, x_off + w, h, z_off + 0.1f);
}

static void draw_bars(void)
{
    gint x, y;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushMatrix();
    glTranslatef(0.0f, -0.5f, -5.0f);
    glRotatef(x_angle, 1.0f, 0.0f, 0.0f);
    glRotatef(y_angle, 0.0f, 1.0f, 0.0f);
    glRotatef(z_angle, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    for (y = 0; y < 16; y++) {
        GLfloat z_off = -1.6f + ((15 - y) * 0.2f);
        GLfloat b_base = y * (1.0f / 15.0f);
        GLfloat r_base = 1.0f - b_base;
        for (x = 0; x < 16; x++) {
            GLfloat x_off = -1.6f + (x * 0.2f);
            draw_bar(x_off, z_off, heights[y][x], r_base - (x * (r_base / 15.0f)),
                     x * (1.0f / 15.0f), b_base);
        }
    }
    glEnd();
    glPopMatrix();
}

/* --------------------------------------------------------------------------
 * GtkGLArea callbacks
 * -------------------------------------------------------------------------- */

static void on_realize(GtkGLArea *area, gpointer data)
{
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area))
        return;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -1.0, 1.0, 1.5, 10.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    gl_ready = TRUE;
}

static void on_unrealize(GtkGLArea *area, gpointer data)
{
    (void)area;
    (void)data;
    gl_ready = FALSE;
}

static gboolean on_render(GtkGLArea *area, GdkGLContext *ctx, gpointer data)
{
    (void)ctx;
    (void)data;
    if (!gl_ready || gtk_gl_area_get_error(area))
        return FALSE;
    draw_bars();
    return TRUE;
}

static void on_resize(GtkGLArea *area, gint width, gint height, gpointer data)
{
    (void)data;
    gtk_gl_area_make_current(area);
    glViewport(0, 0, width, height);
}

/* --------------------------------------------------------------------------
 * Animation timer: advance angles then queue a redraw
 * -------------------------------------------------------------------------- */

static gboolean anim_tick(gpointer data)
{
    (void)data;
    if (!win || !gtk_widget_get_visible(win))
        return G_SOURCE_CONTINUE;

#define WRAP360(a)         \
    do {                   \
        if ((a) >= 360.0f) \
            (a) -= 360.0f; \
    } while (0)
    x_angle += x_speed;
    WRAP360(x_angle);
    y_angle += y_speed;
    WRAP360(y_angle);
    z_angle += z_speed;
    WRAP360(z_angle);
#undef WRAP360

    gtk_widget_queue_draw(gl_area);
    return G_SOURCE_CONTINUE;
}

/* --------------------------------------------------------------------------
 * Key and window event handlers
 * -------------------------------------------------------------------------- */

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    (void)widget;
    (void)data;
    switch (event->keyval) {
    case GDK_KEY_Escape:
        if (oglspectrum_vp.disable_plugin)
            g_idle_add((GSourceFunc)oglspectrum_vp.disable_plugin, &oglspectrum_vp);
        return TRUE;
    case GDK_KEY_Up:
        x_speed = CLAMP(x_speed - 0.1f, -3.0f, 3.0f);
        return TRUE;
    case GDK_KEY_Down:
        x_speed = CLAMP(x_speed + 0.1f, -3.0f, 3.0f);
        return TRUE;
    case GDK_KEY_Left:
        y_speed = CLAMP(y_speed - 0.1f, -3.0f, 3.0f);
        return TRUE;
    case GDK_KEY_Right:
        y_speed = CLAMP(y_speed + 0.1f, -3.0f, 3.0f);
        return TRUE;
    case GDK_KEY_w:
        z_speed = CLAMP(z_speed - 0.1f, -3.0f, 3.0f);
        return TRUE;
    case GDK_KEY_q:
        z_speed = CLAMP(z_speed + 0.1f, -3.0f, 3.0f);
        return TRUE;
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
        x_speed = 0.0f;
        y_speed = 0.5f;
        z_speed = 0.0f;
        x_angle = 20.0f;
        y_angle = 45.0f;
        z_angle = 0.0f;
        return TRUE;
    default:
        return FALSE;
    }
}

static gboolean on_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    (void)widget;
    (void)event;
    (void)data;
    if (oglspectrum_vp.disable_plugin)
        g_idle_add((GSourceFunc)oglspectrum_vp.disable_plugin, &oglspectrum_vp);
    return TRUE;
}

/* --------------------------------------------------------------------------
 * Plugin lifecycle
 * -------------------------------------------------------------------------- */

static void start_display(void)
{
    int x, y;

    for (x = 0; x < 16; x++)
        for (y = 0; y < 16; y++)
            heights[y][x] = 0.0f;
    scale = 1.0f / logf(256.0f);

    x_speed = 0.0f;
    y_speed = 0.5f;
    z_speed = 0.0f;
    x_angle = 20.0f;
    y_angle = 45.0f;
    z_angle = 0.0f;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), _("OpenGL Spectrum Analyzer"));
    gtk_window_set_default_size(GTK_WINDOW(win), 640, 480);
    gtk_widget_add_events(win, GDK_KEY_PRESS_MASK);

    gl_area = gtk_gl_area_new();
    gtk_gl_area_set_required_version(GTK_GL_AREA(gl_area), 1, 0);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl_area), TRUE);
    gtk_container_add(GTK_CONTAINER(win), gl_area);

    g_signal_connect(G_OBJECT(gl_area), "realize", G_CALLBACK(on_realize), NULL);
    g_signal_connect(G_OBJECT(gl_area), "unrealize", G_CALLBACK(on_unrealize), NULL);
    g_signal_connect(G_OBJECT(gl_area), "render", G_CALLBACK(on_render), NULL);
    g_signal_connect(G_OBJECT(gl_area), "resize", G_CALLBACK(on_resize), NULL);
    g_signal_connect(G_OBJECT(win), "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(G_OBJECT(win), "delete-event", G_CALLBACK(on_delete), NULL);

    gtk_widget_show_all(win);
    anim_id = g_timeout_add(ANIM_MS, anim_tick, NULL);
}

static void stop_display(void)
{
    if (anim_id) {
        g_source_remove(anim_id);
        anim_id = 0;
    }
    gl_ready = FALSE;
    if (win) {
        gtk_widget_destroy(win);
        win = NULL;
        gl_area = NULL;
    }
}

static void oglspectrum_init(void)
{
    if (win)
        return;
    oglspectrum_read_config();
    start_display();
}

static void oglspectrum_cleanup(void)
{
    stop_display();
}

static void oglspectrum_playback_start(void)
{ /* nothing — window already open */
}
static void oglspectrum_playback_stop(void)
{ /* keep window; bars freeze */
}

void oglspectrum_read_config(void)
{
    ConfigFile *cfg;
    gchar *filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
    oglspectrum_cfg.tdfx_mode = FALSE;
    cfg = xmms_cfg_open_file(filename);
    if (cfg) {
        xmms_cfg_read_boolean(cfg, "OpenGL Spectrum", "tdfx_fullscreen",
                              &oglspectrum_cfg.tdfx_mode);
        xmms_cfg_free(cfg);
    }
    g_free(filename);
}

/* --------------------------------------------------------------------------
 * Frequency data -- called from XMMS audio/vis dispatch thread
 * -------------------------------------------------------------------------- */

static void oglspectrum_render_freq(gint16 data[2][256])
{
    static const gint xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};
    gint i, c, y;

    /* Cascade existing rows down one slot */
    for (y = 15; y > 0; y--)
        for (i = 0; i < 16; i++)
            heights[y][i] = heights[y - 1][i];

    /* Fill top row */
    for (i = 0; i < NUM_BANDS; i++) {
        for (c = xscale[i], y = 0; c < xscale[i + 1]; c++)
            if (data[0][c] > y)
                y = data[0][c];
        y >>= 7;
        heights[0][i] = (y > 0) ? (logf(y) * scale) : 0.0f;
    }
}
