/*  XMMS - OpenGL Spectrum Analyzer visualization plugin
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Tech
 *  GTK3/GtkGLArea port  Copyright (C) 2024  XMMS Resurrection Project
 *
 *  GL backend: upgraded from GL 1.x fixed-function to GL 3.3 core profile.
 *  GtkGLArea enforces >= 3.2; glBegin/glEnd/glMatrixMode are unavailable.
 *  Renderer now uses GLSL 330 shaders + VAO/VBO + manual mat4 math.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <math.h>
#include <string.h>

/* epoxy must be included before gtk/gtk.h to intercept GL function pointers */
#include <gtk/gtk.h>

#include <epoxy/gl.h>

#include "config.h"
#include "libxmms/configfile.h"
#include "libxmms/util.h"
#include "opengl_spectrum.h"
#include "vis_chrome.h"
#include "xmms/i18n.h"
#include "xmms/plugin.h"

#define NUM_BANDS 16
#define ANIM_MS 16                          /* ~60 fps */
#define VERTS_PER_BAR 36                    /* 6 faces × 2 triangles × 3 vertices */
#define MAX_VERTS (16 * 16 * VERTS_PER_BAR) /* 9216 */

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

/* GL objects */
static GLuint gl_program = 0;
static GLuint gl_vao = 0;
static GLuint gl_vbo = 0;
static GLint u_mvp = -1;

/* CPU-side vertex buffer: interleaved [x,y,z, r,g,b] per vertex */
static GLfloat vbuf[MAX_VERTS * 6];
static int vbuf_count = 0;

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
 * Minimal column-major 4×4 matrix helpers (GLSL convention)
 * Column n occupies m[n*4 .. n*4+3].
 * -------------------------------------------------------------------------- */
typedef GLfloat Mat4[16];

static void mat4_identity(Mat4 m)
{
    memset(m, 0, sizeof(Mat4));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_mul(Mat4 out, const Mat4 a, const Mat4 b)
{
    int c, r;
    for (c = 0; c < 4; c++)
        for (r = 0; r < 4; r++)
            out[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] +
                             a[2 * 4 + r] * b[c * 4 + 2] + a[3 * 4 + r] * b[c * 4 + 3];
}

/* Replicates glFrustum(l,r,b,t,n,f) */
static void mat4_frustum(Mat4 m, GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    mat4_identity(m);
    m[0] = 2.0f * n / (r - l);
    m[5] = 2.0f * n / (t - b);
    m[8] = (r + l) / (r - l);
    m[9] = (t + b) / (t - b);
    m[10] = -(f + n) / (f - n);
    m[11] = -1.0f;
    m[14] = -2.0f * f * n / (f - n);
    m[15] = 0.0f;
}

static void mat4_translate(Mat4 m, GLfloat tx, GLfloat ty, GLfloat tz)
{
    mat4_identity(m);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}

static void mat4_rotate_x(Mat4 m, GLfloat deg)
{
    GLfloat rad = deg * (GLfloat)M_PI / 180.0f;
    GLfloat c = cosf(rad), s = sinf(rad);
    mat4_identity(m);
    m[5] = c;
    m[6] = s;
    m[9] = -s;
    m[10] = c;
}

static void mat4_rotate_y(Mat4 m, GLfloat deg)
{
    GLfloat rad = deg * (GLfloat)M_PI / 180.0f;
    GLfloat c = cosf(rad), s = sinf(rad);
    mat4_identity(m);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

static void mat4_rotate_z(Mat4 m, GLfloat deg)
{
    GLfloat rad = deg * (GLfloat)M_PI / 180.0f;
    GLfloat c = cosf(rad), s = sinf(rad);
    mat4_identity(m);
    m[0] = c;
    m[1] = s;
    m[4] = -s;
    m[5] = c;
}

/* --------------------------------------------------------------------------
 * GLSL 330 shaders
 * -------------------------------------------------------------------------- */
static const char *vert_src = "#version 330 core\n"
                              "layout(location = 0) in vec3 aPos;\n"
                              "layout(location = 1) in vec3 aColor;\n"
                              "uniform mat4 uMVP;\n"
                              "out vec3 vColor;\n"
                              "void main() {\n"
                              "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
                              "    vColor = aColor;\n"
                              "}\n";

static const char *frag_src = "#version 330 core\n"
                              "in vec3 vColor;\n"
                              "out vec4 FragColor;\n"
                              "void main() {\n"
                              "    FragColor = vec4(vColor, 1.0);\n"
                              "}\n";

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        g_warning("OpenGL Spectrum: shader compile error: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint build_program(void)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        g_warning("OpenGL Spectrum: program link error: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

/* --------------------------------------------------------------------------
 * CPU-side geometry builder (same faces as the original draw_bar)
 * -------------------------------------------------------------------------- */

/* Push one triangle (pos + colour for each vertex) */
static inline void push_tri(GLfloat x0, GLfloat y0, GLfloat z0, GLfloat x1, GLfloat y1, GLfloat z1,
                            GLfloat x2, GLfloat y2, GLfloat z2, GLfloat r, GLfloat g, GLfloat b)
{
    GLfloat *v = &vbuf[vbuf_count * 6];
    v[0] = x0;
    v[1] = y0;
    v[2] = z0;
    v[3] = r;
    v[4] = g;
    v[5] = b;
    v += 6;
    v[0] = x1;
    v[1] = y1;
    v[2] = z1;
    v[3] = r;
    v[4] = g;
    v[5] = b;
    v += 6;
    v[0] = x2;
    v[1] = y2;
    v[2] = z2;
    v[3] = r;
    v[4] = g;
    v[5] = b;
    vbuf_count += 3;
}

/* Mirrors original draw_rectangle: two triangles sharing a colour */
static void push_rect(GLfloat x1, GLfloat y1, GLfloat z1, GLfloat x2, GLfloat y2, GLfloat z2,
                      GLfloat r, GLfloat g, GLfloat b)
{
    if (y1 == y2) {
        push_tri(x1, y1, z1, x2, y1, z1, x2, y2, z2, r, g, b);
        push_tri(x2, y2, z2, x1, y2, z2, x1, y1, z1, r, g, b);
    } else {
        push_tri(x1, y1, z1, x2, y1, z2, x2, y2, z2, r, g, b);
        push_tri(x2, y2, z2, x1, y2, z1, x1, y1, z1, r, g, b);
    }
}

/* Mirrors original draw_bar */
static void push_bar(GLfloat x_off, GLfloat z_off, GLfloat h, GLfloat r, GLfloat g, GLfloat b)
{
    const GLfloat w = 0.1f;
    /* top + bottom (full brightness) */
    push_rect(x_off, h, z_off, x_off + w, h, z_off + 0.1f, r, g, b);
    push_rect(x_off, 0.0f, z_off, x_off + w, 0.0f, z_off + 0.1f, r, g, b);
    /* front + back (half brightness) */
    push_rect(x_off, 0.0f, z_off + 0.1f, x_off + w, h, z_off + 0.1f, 0.5f * r, 0.5f * g, 0.5f * b);
    push_rect(x_off, 0.0f, z_off, x_off + w, h, z_off, 0.5f * r, 0.5f * g, 0.5f * b);
    /* left + right (quarter brightness) */
    push_rect(x_off, 0.0f, z_off, x_off, h, z_off + 0.1f, 0.25f * r, 0.25f * g, 0.25f * b);
    push_rect(x_off + w, 0.0f, z_off, x_off + w, h, z_off + 0.1f, 0.25f * r, 0.25f * g, 0.25f * b);
}

/* Build full vertex buffer for the current heights[][] and upload */
static void build_and_upload_bars(void)
{
    int x, y;
    vbuf_count = 0;
    for (y = 0; y < 16; y++) {
        GLfloat z_off = -1.6f + ((15 - y) * 0.2f);
        GLfloat b_base = y * (1.0f / 15.0f);
        GLfloat r_base = 1.0f - b_base;
        for (x = 0; x < 16; x++) {
            GLfloat x_off = -1.6f + (x * 0.2f);
            if (heights[y][x] > 0.0f)
                push_bar(x_off, z_off, heights[y][x], r_base - (x * (r_base / 15.0f)),
                         x * (1.0f / 15.0f), b_base);
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    glBufferData(GL_ARRAY_BUFFER, vbuf_count * 6 * sizeof(GLfloat), vbuf, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* --------------------------------------------------------------------------
 * GtkGLArea callbacks
 * -------------------------------------------------------------------------- */

static void on_realize(GtkGLArea *area, gpointer data)
{
    (void)data;
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area))
        return;

    gl_program = build_program();
    if (!gl_program)
        return;
    u_mvp = glGetUniformLocation(gl_program, "uMVP");

    glGenVertexArrays(1, &gl_vao);
    glGenBuffers(1, &gl_vbo);

    glBindVertexArray(gl_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    /* Allocate buffer (will be filled each frame) */
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTS * 6 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
    /* location 0: position (3 floats) */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void *)0);
    /* location 1: colour (3 floats) */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                          (void *)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    gl_ready = TRUE;
}

static void on_unrealize(GtkGLArea *area, gpointer data)
{
    (void)data;
    gtk_gl_area_make_current(area);
    gl_ready = FALSE;
    if (gl_vao) {
        glDeleteVertexArrays(1, &gl_vao);
        gl_vao = 0;
    }
    if (gl_vbo) {
        glDeleteBuffers(1, &gl_vbo);
        gl_vbo = 0;
    }
    if (gl_program) {
        glDeleteProgram(gl_program);
        gl_program = 0;
    }
}

static gboolean on_render(GtkGLArea *area, GdkGLContext *ctx, gpointer data)
{
    (void)ctx;
    (void)data;
    if (!gl_ready || gtk_gl_area_get_error(area))
        return FALSE;

    /* Build MVP: projection × translate × rotX × rotY × rotZ */
    Mat4 proj, trans, rx, ry, rz, tmp1, tmp2, mvp;
    int w = gtk_widget_get_allocated_width(GTK_WIDGET(area));
    int h = gtk_widget_get_allocated_height(GTK_WIDGET(area));
    GLfloat asp = (h > 0) ? (GLfloat)w / (GLfloat)h : 1.0f;
    mat4_frustum(proj, -asp, asp, -1.0f, 1.0f, 1.5f, 10.0f);
    mat4_translate(trans, 0.0f, -0.5f, -5.0f);
    mat4_rotate_x(rx, x_angle);
    mat4_rotate_y(ry, y_angle);
    mat4_rotate_z(rz, z_angle);
    mat4_mul(tmp1, rx, rz);
    mat4_mul(tmp2, ry, tmp1);
    mat4_mul(tmp1, trans, tmp2);
    mat4_mul(mvp, proj, tmp1);

    build_and_upload_bars();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gl_program);
    glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp);
    glBindVertexArray(gl_vao);
    if (vbuf_count > 0)
        glDrawArrays(GL_TRIANGLES, 0, vbuf_count);
    glBindVertexArray(0);
    glUseProgram(0);

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

/* Called whenever the window is destroyed by ANY means: WM close button,
 * vis-chrome × button, or Escape key idle-disabling the plugin.  Removes
 * the animation timer and nulls the stale pointers so anim_tick() cannot
 * dereference a finalized GtkWidget and crash.
 *
 * We distinguish two cases:
 *   was_live == TRUE  → window closed externally (× / WM / Escape); schedule
 *                        disable_plugin so XMMS tracks the state change.
 *   was_live == FALSE → stop_display() already removed the timer before
 *                        calling gtk_widget_destroy(); disable_plugin is
 *                        already in progress, no idle needed. */
static void on_window_destroy(GtkWidget *widget, gpointer data)
{
    gboolean was_live;
    (void)widget;
    (void)data;
    was_live = (anim_id != 0);
    if (anim_id) {
        g_source_remove(anim_id);
        anim_id = 0;
    }
    gl_ready = FALSE;
    /* win and gl_area are NULLed by the gtk_widget_destroyed() handlers
     * that are connected alongside this one in start_display(). */
    if (was_live && oglspectrum_vp.disable_plugin)
        g_idle_add((GSourceFunc)oglspectrum_vp.disable_plugin, &oglspectrum_vp);
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
    gtk_window_set_default_size(GTK_WINDOW(win), 640, 480);
    gtk_widget_add_events(win, GDK_KEY_PRESS_MASK);

    gl_area = gtk_gl_area_new();
    /* GL 3.3 core profile — GtkGLArea minimum is 3.2; 1.x fixed-function unavailable */
    gtk_gl_area_set_required_version(GTK_GL_AREA(gl_area), 3, 3);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl_area), TRUE);

    if (oglspectrum_cfg.tdfx_mode) {
        /* Fullscreen mode: strip WM decorations and fill the whole screen.
         * Skip vis_chrome chrome bar — it makes no sense in full-screen. */
        gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
        gtk_container_add(GTK_CONTAINER(win), gl_area);
        gtk_window_fullscreen(GTK_WINDOW(win));
    } else {
        /* parity: WM titlebar replaced with XMMS-skin chrome */
        vis_chrome_apply(GTK_WINDOW(win), gl_area, _("OpenGL Spectrum Analyzer"));
    }

    /* Safety: null win/gl_area and kill the timer when the window is destroyed
     * by ANY means (vis-chrome × button, WM close, Escape key, stop_display).
     * Without these, anim_tick() races on a finalized GtkWidget and segfaults. */
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(on_window_destroy), NULL);
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(gtk_widget_destroyed), &win);
    g_signal_connect(G_OBJECT(gl_area), "destroy", G_CALLBACK(gtk_widget_destroyed), &gl_area);

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
    /* Remove the timer first so anim_tick() cannot fire on a half-destroyed
     * window between here and gtk_widget_destroy() completing. */
    if (anim_id) {
        g_source_remove(anim_id);
        anim_id = 0;
    }
    gl_ready = FALSE;
    if (win) {
        gtk_widget_destroy(win);
        /* win and gl_area are NULLed synchronously inside on_window_destroy
         * / gtk_widget_destroyed() which fires during gtk_widget_destroy(). */
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
{
}
static void oglspectrum_playback_stop(void)
{
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
 * Frequency data  — called from XMMS vis dispatch
 * -------------------------------------------------------------------------- */

static void oglspectrum_render_freq(gint16 data[2][256])
{
    static const gint xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};
    gint i, c, y;

    /* Cascade rows down */
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
