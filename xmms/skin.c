/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2002  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2002  Haavard Kvaalen
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
#include <ctype.h>

#include "defskin/cbuttons.xpm"
#include "defskin/eq_ex.xpm"
#include "defskin/eqmain.xpm"
#include "defskin/main.xpm"
#include "defskin/monoster.xpm"
#include "defskin/nums_ex.xpm"
#include "defskin/playpaus.xpm"
#include "defskin/pledit.xpm"
#include "defskin/posbar.xpm"
#include "defskin/shufrep.xpm"
#include "defskin/text.xpm"
#include "defskin/titlebar.xpm"
#include "defskin/volume.xpm"
#include "xmms.h"

#ifndef HAVE_MKDTEMP
char *mkdtemp(char *path);
#endif

Skin *skin;
static int skin_current_num;

static const gint skin_default_viscolor[24][3] = {{9, 34, 53},   {10, 18, 26},  {0, 54, 108},
                                                  {0, 58, 116},  {0, 62, 124},  {0, 66, 132},
                                                  {0, 70, 140},  {0, 74, 148},  {0, 78, 156},
                                                  {0, 82, 164},  {0, 86, 172},  {0, 92, 184},
                                                  {0, 98, 196},  {0, 104, 208}, {0, 110, 220},
                                                  {0, 116, 232}, {0, 122, 244}, {0, 128, 255},
                                                  {0, 128, 255}, {0, 104, 208}, {0, 80, 160},
                                                  {0, 56, 112},  {0, 32, 64},   {200, 200, 200}};

static void setup_skin_masks(void)
{
    if (cfg.show_wm_decorations)
        return;
    if (cfg.player_visible) {
        gtk_widget_shape_combine_region(mainwin, skin_get_mask(SKIN_MASK_MAIN, cfg.doublesize,
                                                               cfg.player_shaded));
    }

    gtk_widget_shape_combine_region(equalizerwin, skin_get_mask(SKIN_MASK_EQ, EQUALIZER_DOUBLESIZE,
                                                                cfg.equalizer_shaded));
}

static cairo_region_t *create_default_mask(gint w, gint h)
{
    cairo_rectangle_int_t rect = {0, 0, w, h};
    return cairo_region_create_rectangle(&rect);
}

static void load_def_pixmap(SkinPixmap *skinpixmap, gchar **skindata)
{
    GdkPixbuf *pb = gdk_pixbuf_new_from_xpm_data((const gchar **)skindata);
    if (pb == NULL)
        return;
    skinpixmap->width = gdk_pixbuf_get_width(pb);
    skinpixmap->height = gdk_pixbuf_get_height(pb);
    skinpixmap->def_pixmap = gdk_cairo_surface_create_from_pixbuf(pb, 1, NULL);
    g_object_unref(pb);
}

/* GTK3: extract r/g/b from an ARGB32 cairo pixel value (8-bit → 16-bit range) */
static void skin_query_color_from_pixel(guint32 pixel, GdkColor *c)
{
    c->red = ((pixel >> 16) & 0xffu) * 0x101u;
    c->green = ((pixel >> 8) & 0xffu) * 0x101u;
    c->blue = (pixel & 0xffu) * 0x101u;
}

static glong skin_calc_luminance(GdkColor *c)
{
    return (0.212671 * c->red + 0.715160 * c->green + 0.072169 * c->blue);
}

/* GTK3: skin_get_textcolors reads pixel data directly from an ARGB32 cairo surface */
static void skin_get_textcolors(cairo_surface_t *text, GdkColor *bgc, GdkColor *fgc)
{
    /*
     * Try to extract reasonable background and foreground colors
     * from the font surface (first 6 rows, 152 columns).
     */
    int i;
    guchar *data;
    gint stride, surf_w, surf_h;

    if (text == NULL)
        return;

    cairo_surface_flush(text);
    data = cairo_image_surface_get_data(text);
    stride = cairo_image_surface_get_stride(text);
    surf_w = cairo_image_surface_get_width(text);
    surf_h = cairo_image_surface_get_height(text);

    for (i = 0; i < 6 && i < surf_h; i++) {
        guint32 *row = (guint32 *)(void *)(data + i * stride);
        GdkColor c;
        gint x;
        glong d, max_d;

        /* Background: pixel at far right (position 151, or last if narrower) */
        gint bg_x = MIN(151, surf_w - 1);
        skin_query_color_from_pixel(row[bg_x], &bgc[i]);

        max_d = 0;
        for (x = 1; x < MIN(150, surf_w); x++) {
            skin_query_color_from_pixel(row[x], &c);
            d = labs(skin_calc_luminance(&c) - skin_calc_luminance(&bgc[i]));
            if (d > max_d) {
                memcpy(&fgc[i], &c, sizeof(GdkColor));
                max_d = d;
            }
        }
    }
}

void init_skins(void)
{
    gint i;

    skin = (Skin *)g_malloc0(sizeof(Skin));
    load_def_pixmap(&skin->main, skin_main);
    load_def_pixmap(&skin->cbuttons, skin_cbuttons);
    load_def_pixmap(&skin->titlebar, skin_titlebar);
    load_def_pixmap(&skin->shufrep, skin_shufrep);
    load_def_pixmap(&skin->text, skin_text);
    skin_get_textcolors(skin->text.def_pixmap, skin->def_textbg, skin->def_textfg);
    load_def_pixmap(&skin->volume, skin_volume);
    load_def_pixmap(&skin->balance, skin_volume);
    load_def_pixmap(&skin->monostereo, skin_monoster);
    load_def_pixmap(&skin->playpause, skin_playpaus);
    load_def_pixmap(&skin->numbers, skin_nums_ex);
    load_def_pixmap(&skin->posbar, skin_posbar);
    load_def_pixmap(&skin->pledit, skin_pledit);
    load_def_pixmap(&skin->eqmain, skin_eqmain);
    load_def_pixmap(&skin->eq_ex, skin_eq_ex);

    skin->def_pledit_normal.red = 0x2400;
    skin->def_pledit_normal.green = 0x9900;
    skin->def_pledit_normal.blue = 0xffff;
    /* GTK3: pixel field not needed with CSS-based drawing */
    skin->def_pledit_current.red = 0xffff;
    skin->def_pledit_current.green = 0xee00;
    skin->def_pledit_current.blue = 0xffff;
    /* GTK3: pixel field not needed with CSS-based drawing */
    skin->def_pledit_normalbg.red = 0x0A00;
    skin->def_pledit_normalbg.green = 0x1200;
    skin->def_pledit_normalbg.blue = 0x0A00;
    /* GTK3: pixel field not needed with CSS-based drawing */
    skin->def_pledit_selectedbg.red = 0x0A00;
    skin->def_pledit_selectedbg.green = 0x1200;
    skin->def_pledit_selectedbg.blue = 0x4A00;
    /* GTK3: pixel field not needed with CSS-based drawing */
    for (i = 0; i < 24; i++) {
        skin->vis_color[i][0] = skin_default_viscolor[i][0];
        skin->vis_color[i][1] = skin_default_viscolor[i][1];
        skin->vis_color[i][2] = skin_default_viscolor[i][2];
    }
    skin->def_mask = create_default_mask(275, 116);
    skin->def_mask_ds = create_default_mask(550, 232);
    skin->def_mask_shade = create_default_mask(275, 14);
    skin->def_mask_shade_ds = create_default_mask(550, 28);

    setup_skin_masks();

    create_skin_window();
}

static guint hex_chars_to_int(gchar c, gchar d)
{
    /*
     * Converts a value in the range 0x00-0xFF
     * to a integer in the range 0-65535
     */
    gchar str[3];

    str[0] = c;
    str[1] = d;
    str[2] = '\0';

    return (CLAMP(strtol(str, NULL, 16), 0, 255) * 256);
}

GdkColor *load_skin_color(const gchar *path, const gchar *file, const gchar *section,
                          const gchar *key)
{
    gchar *filename, *value;
    GdkColor *color = NULL;

    filename = find_file_recursively(path, file);
    if (filename) {
        value = read_ini_string(filename, section, key);
        if (value) {
            gchar *ptr = value;
            gint len;
            color = g_malloc0(sizeof(GdkColor));
            g_strchug(g_strchomp(value));
            if (value[0] == '#')
                ptr++;
            len = strlen(ptr);

            /*
             * The handling of incomplete values is done this way
             * to maximize winamp compatibility
             */
            if (len >= 6) {
                color->red = hex_chars_to_int(*ptr, *(ptr + 1));
                ptr += 2;
            }
            if (len >= 4) {
                color->green = hex_chars_to_int(*ptr, *(ptr + 1));
                ptr += 2;
            }
            if (len >= 2)
                color->blue = hex_chars_to_int(*ptr, *(ptr + 1));


            /* GTK3: color->pixel not needed */
            g_free(value);
        }
        g_free(filename);
    }
    return color;
}

static void load_skin_pixmap(SkinPixmap *skinpixmap, const gchar *path, const gchar *file)
{
    char *filename;
    gint w, h;

    filename = find_file_recursively(path, file);

    if (!filename)
        return;

    skinpixmap->pixmap = read_bmp(filename);

    g_free(filename);

    if (!skinpixmap->pixmap)
        return;
    w = cairo_image_surface_get_width(skinpixmap->pixmap);
    h = cairo_image_surface_get_height(skinpixmap->pixmap);

    skinpixmap->current_width = MIN(w, skinpixmap->width);
    skinpixmap->current_height = MIN(h, skinpixmap->height);
}

/* GTK3: skin_create_transparent_mask uses an CAIRO_FORMAT_A1 surface rendered with
 * polygon fills, then converts to cairo_region_t via gdk_cairo_region_create_from_surface.
 */
cairo_region_t *skin_create_transparent_mask(const gchar *path, const gchar *file,
                                             const gchar *section, GdkWindow *window, gint width,
                                             gint height, gboolean doublesize)
{
    gchar *filename;
    cairo_surface_t *mask_surface;
    cairo_t *cr;
    cairo_region_t *region;
    gboolean created_mask = FALSE;
    GArray *num, *point;
    gint i, j, k;

    if (!path)
        return NULL;
    filename = find_file_recursively(path, file);
    if (!filename)
        return NULL;

    if ((num = read_ini_array(filename, section, "NumPoints")) == NULL) {
        g_free(filename);
        return NULL;
    }

    if ((point = read_ini_array(filename, section, "PointList")) == NULL) {
        g_array_free(num, TRUE);
        g_free(filename);
        return NULL;
    }

    /* Render polygon(s) onto a 1-bit alpha surface */
    mask_surface = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
    cr = cairo_create(mask_surface);

    /* Start fully transparent */
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);

    j = 0;
    for (i = 0; i < (gint)num->len; i++) {
        gint npts = g_array_index(num, gint, i);
        if ((gint)(point->len - j) >= npts * 2) {
            created_mask = TRUE;
            cairo_new_path(cr);
            for (k = 0; k < npts; k++) {
                gdouble x = g_array_index(point, gint, j + k * 2) * (1 + doublesize);
                gdouble y = g_array_index(point, gint, j + k * 2 + 1) * (1 + doublesize);
                if (k == 0)
                    cairo_move_to(cr, x, y);
                else
                    cairo_line_to(cr, x, y);
            }
            cairo_close_path(cr);
            cairo_fill(cr);
            j += k * 2;
        }
    }
    g_array_free(num, TRUE);
    g_array_free(point, TRUE);
    g_free(filename);

    if (!created_mask) {
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
    region = gdk_cairo_region_create_from_surface(mask_surface);
    cairo_surface_destroy(mask_surface);
    return region;
}

void load_skin_viscolor(const gchar *path, const gchar *file)
{
    FILE *f;
    gint i, c;
    gchar line[256], *filename;
    GArray *a;

    for (i = 0; i < 24; i++) {
        skin->vis_color[i][0] = skin_default_viscolor[i][0];
        skin->vis_color[i][1] = skin_default_viscolor[i][1];
        skin->vis_color[i][2] = skin_default_viscolor[i][2];
    }

    filename = find_file_recursively(path, file);
    if (!filename)
        return;

    if ((f = fopen(filename, "r")) == NULL) {
        g_free(filename);
        return;
    }

    for (i = 0; i < 24; i++) {
        if (fgets(line, 255, f)) {
            a = string_to_garray(line);
            if (a->len > 2) {
                for (c = 0; c < 3; c++)
                    skin->vis_color[i][c] = g_array_index(a, gint, c);
            }
            g_array_free(a, TRUE);
        } else
            break;
    }
    fclose(f);
    g_free(filename);
}

static void skin_numbers_generate_dash(SkinPixmap *numbers)
{
    cairo_surface_t *pixmap;
    cairo_t *cr;

    if (numbers->pixmap == NULL || numbers->current_width < 99)
        return;

    pixmap = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 108, numbers->current_height);
    cr = cairo_create(pixmap);
    skin_draw_pixmap(cr, SKIN_NUMBERS, 0, 0, 0, 0, 99, 13);
    skin_draw_pixmap(cr, SKIN_NUMBERS, 90, 0, 99, 0, 9, 13);
    skin_draw_pixmap(cr, SKIN_NUMBERS, 20, 6, 101, 6, 5, 1);
    cairo_destroy(cr);
    cairo_surface_destroy(numbers->pixmap);
    numbers->pixmap = pixmap;
    numbers->current_width = 108;
}


static void skin_free_pixmap(SkinPixmap *p)
{
    if (p->pixmap)
        cairo_surface_destroy(p->pixmap);
    p->pixmap = NULL;
}

void free_skin(void)
{
    gint i;

    skin_free_pixmap(&skin->main);
    skin_free_pixmap(&skin->cbuttons);
    skin_free_pixmap(&skin->titlebar);
    skin_free_pixmap(&skin->shufrep);
    skin_free_pixmap(&skin->text);
    skin_free_pixmap(&skin->volume);
    skin_free_pixmap(&skin->balance);
    skin_free_pixmap(&skin->monostereo);
    skin_free_pixmap(&skin->playpause);
    skin_free_pixmap(&skin->numbers);
    skin_free_pixmap(&skin->posbar);
    skin_free_pixmap(&skin->pledit);
    skin_free_pixmap(&skin->eqmain);
    skin_free_pixmap(&skin->eq_ex);

    if (skin->mask_main)
        cairo_region_destroy(skin->mask_main);
    if (skin->mask_main_ds)
        cairo_region_destroy(skin->mask_main_ds);
    if (skin->mask_shade)
        cairo_region_destroy(skin->mask_shade);
    if (skin->mask_shade_ds)
        cairo_region_destroy(skin->mask_shade_ds);
    if (skin->mask_eq)
        cairo_region_destroy(skin->mask_eq);
    if (skin->mask_eq_ds)
        cairo_region_destroy(skin->mask_eq_ds);
    if (skin->mask_eq_shade)
        cairo_region_destroy(skin->mask_eq_shade);
    if (skin->mask_eq_shade_ds)
        cairo_region_destroy(skin->mask_eq_shade_ds);

    skin->mask_main = NULL;
    skin->mask_main_ds = NULL;
    skin->mask_shade = NULL;
    skin->mask_shade_ds = NULL;
    skin->mask_eq = NULL;
    skin->mask_eq_ds = NULL;
    skin->mask_eq_shade = NULL;
    skin->mask_eq_shade_ds = NULL;

    if (skin->pledit_normal)
        g_free(skin->pledit_normal);
    if (skin->pledit_current)
        g_free(skin->pledit_current);
    if (skin->pledit_normalbg)
        g_free(skin->pledit_normalbg);
    if (skin->pledit_selectedbg)
        g_free(skin->pledit_selectedbg);

    skin->pledit_normal = NULL;
    skin->pledit_current = NULL;
    skin->pledit_normalbg = NULL;
    skin->pledit_selectedbg = NULL;
    for (i = 0; i < 24; i++) {
        skin->vis_color[i][0] = skin_default_viscolor[i][0];
        skin->vis_color[i][1] = skin_default_viscolor[i][1];
        skin->vis_color[i][2] = skin_default_viscolor[i][2];
    }
}

static void skin_load_pixmaps(const char *path)
{
    load_skin_pixmap(&skin->main, path, "main.bmp");
    load_skin_pixmap(&skin->cbuttons, path, "cbuttons.bmp");
    load_skin_pixmap(&skin->titlebar, path, "titlebar.bmp");
    load_skin_pixmap(&skin->shufrep, path, "shufrep.bmp");
    load_skin_pixmap(&skin->text, path, "text.bmp");
    if (skin->text.current_width < 152 || skin->text.current_height < 6)
        skin_get_textcolors(skin->text.def_pixmap, skin->textbg, skin->textfg);
    else
        skin_get_textcolors(skin->text.pixmap, skin->textbg, skin->textfg);
    load_skin_pixmap(&skin->volume, path, "volume.bmp");
    load_skin_pixmap(&skin->balance, path, "balance.bmp");
    if (skin->balance.pixmap == NULL)
        load_skin_pixmap(&skin->balance, path, "volume.bmp");
    load_skin_pixmap(&skin->monostereo, path, "monoster.bmp");
    load_skin_pixmap(&skin->playpause, path, "playpaus.bmp");
    load_skin_pixmap(&skin->numbers, path, "nums_ex.bmp");
    if (skin->numbers.pixmap == NULL) {
        load_skin_pixmap(&skin->numbers, path, "numbers.bmp");
        skin_numbers_generate_dash(&skin->numbers);
    }
    load_skin_pixmap(&skin->posbar, path, "posbar.bmp");
    load_skin_pixmap(&skin->pledit, path, "pledit.bmp");
    load_skin_pixmap(&skin->eqmain, path, "eqmain.bmp");
    load_skin_pixmap(&skin->eq_ex, path, "eq_ex.bmp");
    skin->pledit_normal = load_skin_color(path, "pledit.txt", "text", "normal");
    skin->pledit_current = load_skin_color(path, "pledit.txt", "text", "current");
    skin->pledit_normalbg = load_skin_color(path, "pledit.txt", "text", "normalbg");
    skin->pledit_selectedbg = load_skin_color(path, "pledit.txt", "text", "selectedbg");
    skin->mask_main = skin_create_transparent_mask(path, "region.txt", "Normal",
                                                   gtk_widget_get_window(mainwin), 275, 116, FALSE);
    skin->mask_main_ds =
        skin_create_transparent_mask(path, "region.txt", "Normal", gtk_widget_get_window(mainwin),
                                     550, 232, TRUE);
    skin->mask_shade = skin_create_transparent_mask(path, "region.txt", "WindowShade",
                                                    gtk_widget_get_window(mainwin), 275, 14, FALSE);
    skin->mask_shade_ds =
        skin_create_transparent_mask(path, "region.txt", "WindowShade",
                                     gtk_widget_get_window(mainwin), 550, 28, TRUE);
    skin->mask_eq =
        skin_create_transparent_mask(path, "region.txt", "Equalizer",
                                     gtk_widget_get_window(equalizerwin), 275, 116, FALSE);
    skin->mask_eq_ds =
        skin_create_transparent_mask(path, "region.txt", "Equalizer",
                                     gtk_widget_get_window(equalizerwin), 550, 232, TRUE);
    skin->mask_eq_shade =
        skin_create_transparent_mask(path, "region.txt", "EqualizerWS",
                                     gtk_widget_get_window(equalizerwin), 275, 14, FALSE);
    skin->mask_eq_shade_ds =
        skin_create_transparent_mask(path, "region.txt", "EqualizerWS",
                                     gtk_widget_get_window(equalizerwin), 550, 28, TRUE);

    load_skin_viscolor(path, "viscolor.txt");
}

/*
 * escape_shell_chars()
 *
 * Escapes characters that are special to the shell inside double quotes.
 */

static char *escape_shell_chars(const char *string)
{
    const char *special = "$`\"\\"; /* Characters to escape */
    const char *in = string;
    char *out, *escaped;
    int num = 0;

    while (*in != '\0')
        if (strchr(special, *in++))
            num++;

    escaped = g_malloc(strlen(string) + num + 1);

    in = string;
    out = escaped;

    while (*in != '\0') {
        if (strchr(special, *in))
            *out++ = '\\';
        *out++ = *in++;
    }
    *out = '\0';

    return escaped;
}

static char *skin_decompress_skin(const char *path)
{
    char *tmp = NULL, *tempdir, *unzip, *tar, *ending, *escaped;

    unzip = getenv("UNZIPCMD");
    if (!unzip)
        unzip = "unzip";
    tar = getenv("TARCMD");
    if (!tar)
        tar = "tar";

    if ((ending = strrchr(path, '.')) == NULL)
        return NULL;

    tempdir = g_strconcat(g_get_tmp_dir(), "/xmmsskin.XXXXXXXX", NULL);
    if (!mkdtemp(tempdir)) {
        g_free(tempdir);
        g_message("Failed to create temporary directory: %s.  Unable to load skin.",
                  strerror(errno));
        return NULL;
    }

    escaped = escape_shell_chars(path);

    if (!strcasecmp(ending, ".zip") || !strcasecmp(ending, ".wsz"))
        tmp = g_strdup_printf("%s >/dev/null -o -j \"%s\" -d %s", unzip, escaped, tempdir);
    if (!strcasecmp(ending, ".tgz") || !strcasecmp(ending, ".gz"))
        tmp = g_strdup_printf("%s >/dev/null xzf \"%s\" -C %s", tar, escaped, tempdir);
    if (!strcasecmp(ending, ".bz2"))
        tmp = g_strdup_printf("bzip2 -dc \"%s\" | %s >/dev/null xf - -C %s", escaped, tar, tempdir);
    if (!strcasecmp(ending, ".tar"))
        tmp = g_strdup_printf("%s >/dev/null xf \"%s\" -C %s", tar, escaped, tempdir);

    system(tmp);
    g_free(escaped);
    g_free(tmp);
    return tempdir;
}


static void _load_skin(const gchar *path, gboolean force)
{
    char *ending;

    if (!force) {
        if (skin->path && path)
            if (!strcmp(skin->path, path))
                return;
        if (!skin->path && !path)
            return;
        free_skin();
    }
    skin_current_num++;
    if (path) {
        skin->path = g_realloc(skin->path, strlen(path) + 1);
        strcpy(skin->path, path);

        ending = strrchr(path, '.');
        if (ending && (!strcasecmp(ending, ".zip") || !strcasecmp(ending, ".wsz") ||
                       !strcasecmp(ending, ".tgz") || !strcasecmp(ending, ".gz") ||
                       !strcasecmp(ending, ".bz2") || !strcasecmp(ending, ".tar"))) {
            char *cpath = skin_decompress_skin(path);
            if (cpath != NULL) {
                skin_load_pixmaps(cpath);
                del_directory(cpath);
                g_free(cpath);
            }
        } else
            skin_load_pixmaps(path);
    } else {
        if (skin->path)
            g_free(skin->path);
        skin->path = NULL;
    }

    setup_skin_masks();

    draw_main_window(TRUE);
    draw_playlist_window(TRUE);
    draw_equalizer_window(TRUE);
    playlistwin_update_list();
}

void load_skin(const gchar *path)
{
    _load_skin(path, FALSE);
}

void reload_skin(void)
{
    _load_skin(skin->path, TRUE);
}

void cleanup_skins(void)
{
    free_skin();
}

static SkinPixmap *get_skin_pixmap(SkinIndex si)
{
    switch (si) {
    case SKIN_MAIN:
        return &skin->main;
    case SKIN_CBUTTONS:
        return &skin->cbuttons;
    case SKIN_TITLEBAR:
        return &skin->titlebar;
    case SKIN_SHUFREP:
        return &skin->shufrep;
    case SKIN_TEXT:
        return &skin->text;
    case SKIN_VOLUME:
        return &skin->volume;
    case SKIN_BALANCE:
        return &skin->balance;
    case SKIN_MONOSTEREO:
        return &skin->monostereo;
    case SKIN_PLAYPAUSE:
        return &skin->playpause;
    case SKIN_NUMBERS:
        return &skin->numbers;
    case SKIN_POSBAR:
        return &skin->posbar;
    case SKIN_PLEDIT:
        return &skin->pledit;
    case SKIN_EQMAIN:
        return &skin->eqmain;
    case SKIN_EQ_EX:
        return &skin->eq_ex;
    }
    g_error("Unable to find skin pixmap");

    return NULL;
}

cairo_region_t *skin_get_mask(MaskIndex mi, gboolean doublesize, gboolean shaded)
{
    cairo_region_t *ret = NULL;

    switch (mi) {
    case SKIN_MASK_MAIN:
        if (!shaded) {
            if (!doublesize) {
                ret = skin->mask_main;
                if (!ret)
                    ret = skin->def_mask;
            } else {
                ret = skin->mask_main_ds;
                if (!ret)
                    ret = skin->def_mask_ds;
                break;
            }
        } else {
            if (!doublesize) {
                ret = skin->mask_shade;
                if (!ret)
                    ret = skin->def_mask_shade;
            } else {
                ret = skin->mask_shade_ds;
                if (!ret)
                    ret = skin->def_mask_shade_ds;
                break;
            }
        }
        break;
    case SKIN_MASK_EQ:
        if (!shaded) {
            if (!doublesize) {
                ret = skin->mask_eq;
                if (!ret)
                    ret = skin->def_mask;
            } else {
                ret = skin->mask_eq_ds;
                if (!ret)
                    ret = skin->def_mask_ds;
            }
        } else {
            if (!doublesize) {
                ret = skin->mask_eq_shade;
                if (!ret)
                    ret = skin->def_mask_shade;
            } else {
                ret = skin->mask_eq_shade_ds;
                if (!ret)
                    ret = skin->def_mask_shade_ds;
            }
        }
        break;
    }
    return ret;
}

GdkColor *get_skin_color(SkinColorIndex si)
{
    GdkColor *ret = NULL;

    switch (si) {
    case SKIN_PLEDIT_NORMAL:
        ret = skin->pledit_normal;
        if (!ret)
            ret = &skin->def_pledit_normal;
        break;
    case SKIN_PLEDIT_CURRENT:
        ret = skin->pledit_current;
        if (!ret)
            ret = &skin->def_pledit_current;
        break;
    case SKIN_PLEDIT_NORMALBG:
        ret = skin->pledit_normalbg;
        if (!ret)
            ret = &skin->def_pledit_normalbg;
        break;
    case SKIN_PLEDIT_SELECTEDBG:
        ret = skin->pledit_selectedbg;
        if (!ret)
            ret = &skin->def_pledit_selectedbg;
        break;
    case SKIN_TEXTBG:
        if (skin->text.pixmap)
            ret = skin->textbg;
        else
            ret = skin->def_textbg;
        break;
    case SKIN_TEXTFG:
        if (skin->text.pixmap)
            ret = skin->textfg;
        else
            ret = skin->def_textfg;
        break;
    }
    return ret;
}

void get_skin_viscolor(guchar vis_color[24][3])
{
    gint i;

    for (i = 0; i < 24; i++) {
        vis_color[i][0] = skin->vis_color[i][0];
        vis_color[i][1] = skin->vis_color[i][1];
        vis_color[i][2] = skin->vis_color[i][2];
    }
}

int skin_get_id(void)
{
    return skin_current_num;
}

void skin_draw_pixmap(cairo_t *cr, SkinIndex si, gint xsrc, gint ysrc, gint xdest, gint ydest,
                      gint width, gint height)
{
    SkinPixmap *pixmap = get_skin_pixmap(si);
    cairo_surface_t *tmp;

    if (pixmap->pixmap != NULL) {
        if (xsrc > pixmap->current_width || ysrc > pixmap->current_height)
            return;

        tmp = pixmap->pixmap;
        width = MIN(width, pixmap->current_width - xsrc);
        height = MIN(height, pixmap->current_height - ysrc);
    } else
        tmp = pixmap->def_pixmap;

    /* GTK3: blit surface region via cairo */
    cairo_save(cr);
    cairo_rectangle(cr, xdest, ydest, width, height);
    cairo_clip(cr);
    cairo_set_source_surface(cr, tmp, (gdouble)(xdest - xsrc), (gdouble)(ydest - ysrc));
    cairo_paint(cr);
    cairo_restore(cr);
}

void skin_get_eq_spline_colors(guint32 (*colors)[19])
{
    gint i;
    cairo_surface_t *pixmap;
    guchar *data;
    gint stride;

    if (skin->eqmain.pixmap != NULL && skin->eqmain.current_width >= 116 &&
        skin->eqmain.current_height >= 313)
        pixmap = skin->eqmain.pixmap;
    else
        pixmap = skin->eqmain.def_pixmap;

    /* GTK3: read pixel column 115 from rows 294..312 via cairo surface data (ARGB32) */
    cairo_surface_flush(pixmap);
    data = cairo_image_surface_get_data(pixmap);
    stride = cairo_image_surface_get_stride(pixmap);
    for (i = 0; i < 19; i++) {
        guint32 *row = (guint32 *)(void *)(data + (294 + i) * stride);
        (*colors)[i] = row[115];
    }
}
