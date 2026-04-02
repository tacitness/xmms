#include "skin_preview.h"

#include <cairo.h>
#include <gdk/gdk.h>
#include <glib/gstdio.h>
#include <string.h>

#include "skin_archive.h"

typedef struct {
    const gchar *name;
    gint x;
    gint y;
} SkinPreviewAsset;

static const SkinPreviewAsset skin_preview_assets[] = {
    {"main.bmp", 0, 0},         {"pledit.bmp", 0, 116},     {"eqmain.bmp", 275, 0},
    {"titlebar.bmp", 275, 116}, {"cbuttons.bmp", 275, 144}, {"nums_ex.bmp", 275, 232},
    {"numbers.bmp", 275, 232},  {"volume.bmp", 383, 232},   {"shufrep.bmp", 275, 262},
    {"posbar.bmp", 331, 262},   {"playpaus.bmp", 275, 292}, {"monoster.bmp", 331, 292},
    {"text.bmp", 387, 292},
};

static gchar *skin_preview_find_asset_recursive(const gchar *dir_path, const gchar *needle);

static void skin_preview_cleanup_directory(const gchar *path)
{
    gchar *argv[] = {(gchar *)"rm", (gchar *)"-rf", (gchar *)path, NULL};

    g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
}

static gboolean skin_preview_has_asset(const gchar *skin_dir, const gchar *needle)
{
    gchar *asset_path = skin_preview_find_asset_recursive(skin_dir, needle);
    gboolean found = asset_path != NULL;

    g_free(asset_path);
    return found;
}

static gchar *skin_preview_find_asset_recursive(const gchar *dir_path, const gchar *needle)
{
    GDir *dir;
    const gchar *name;
    gchar *result = NULL;

    dir = g_dir_open(dir_path, 0, NULL);
    if (dir == NULL)
        return NULL;

    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *path;

        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;

        path = g_build_filename(dir_path, name, NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR))
            result = skin_preview_find_asset_recursive(path, needle);
        else if (!g_ascii_strcasecmp(name, needle))
            result = g_strdup(path);

        g_free(path);
        if (result != NULL)
            break;
    }

    g_dir_close(dir);
    return result;
}

static void skin_preview_draw_background(cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0.06, 0.07, 0.08);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0.14, 0.16, 0.18);
    cairo_rectangle(cr, 0, 0, 275, 116);
    cairo_rectangle(cr, 0, 116, 275, 116);
    cairo_rectangle(cr, 275, 0, 275, 116);
    cairo_fill(cr);
}

static gboolean skin_preview_draw_asset(cairo_t *cr, const gchar *skin_dir,
                                        const SkinPreviewAsset *asset, gboolean *drawn_assets,
                                        GError **error)
{
    gchar *asset_path;
    GdkPixbuf *pixbuf;

    asset_path = skin_preview_find_asset_recursive(skin_dir, asset->name);
    if (asset_path == NULL)
        return TRUE;

    pixbuf = gdk_pixbuf_new_from_file(asset_path, error);
    g_free(asset_path);
    if (pixbuf == NULL)
        return FALSE;

    gdk_cairo_set_source_pixbuf(cr, pixbuf, asset->x, asset->y);
    cairo_paint(cr);
    *drawn_assets = TRUE;
    g_object_unref(pixbuf);
    return TRUE;
}

static gboolean skin_preview_render_to_surface(const gchar *skin_path,
                                               cairo_surface_t **surface_out, gchar **cleanup_dir,
                                               GError **error)
{
    const gchar *ending;
    const gchar *skin_dir = skin_path;
    cairo_surface_t *surface;
    cairo_t *cr;
    gboolean drawn_assets = FALSE;
    guint i;
    gboolean numbers_drawn = FALSE;

    ending = strrchr(skin_path, '.');
    if (ending != NULL &&
        (!g_ascii_strcasecmp(ending, ".wsz") || !g_ascii_strcasecmp(ending, ".zip") ||
         !g_ascii_strcasecmp(ending, ".tgz") || !g_ascii_strcasecmp(ending, ".gz") ||
         !g_ascii_strcasecmp(ending, ".bz2") || !g_ascii_strcasecmp(ending, ".tar"))) {
        *cleanup_dir = skin_archive_extract_to_tempdir(skin_path, error);
        if (*cleanup_dir == NULL)
            return FALSE;
        skin_dir = *cleanup_dir;
    }

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 550, 348);
    cr = cairo_create(surface);
    skin_preview_draw_background(cr);

    for (i = 0; i < G_N_ELEMENTS(skin_preview_assets); i++) {
        const SkinPreviewAsset *asset = &skin_preview_assets[i];

        if (!strcmp(asset->name, "numbers.bmp") && numbers_drawn)
            continue;
        if (!strcmp(asset->name, "nums_ex.bmp"))
            numbers_drawn = skin_preview_has_asset(skin_dir, "nums_ex.bmp");
        if (!skin_preview_draw_asset(cr, skin_dir, asset, &drawn_assets, error)) {
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            return FALSE;
        }
    }

    cairo_destroy(cr);
    if (!drawn_assets) {
        cairo_surface_destroy(surface);
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Preview render did not find any skin assets");
        return FALSE;
    }

    *surface_out = surface;
    return TRUE;
}

gboolean skin_preview_write_png(const gchar *skin_path, const gchar *output_png, GError **error)
{
    cairo_surface_t *surface = NULL;
    gchar *cleanup_dir = NULL;
    cairo_status_t status;
    gboolean ok = FALSE;

    g_return_val_if_fail(skin_path != NULL, FALSE);
    g_return_val_if_fail(output_png != NULL, FALSE);

    if (!skin_preview_render_to_surface(skin_path, &surface, &cleanup_dir, error))
        return FALSE;

    status = cairo_surface_write_to_png(surface, output_png);
    if (status != CAIRO_STATUS_SUCCESS) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "Failed to write preview PNG: %s",
                    cairo_status_to_string(status));
        goto out;
    }

    ok = TRUE;

out:
    if (surface != NULL)
        cairo_surface_destroy(surface);
    if (cleanup_dir != NULL) {
        skin_preview_cleanup_directory(cleanup_dir);
        g_free(cleanup_dir);
    }
    return ok;
}

guint64 skin_preview_png_ahash(const gchar *png_path, GError **error)
{
    GdkPixbuf *pixbuf;
    gint width;
    gint height;
    gint rowstride;
    gint n_channels;
    guchar *pixels;
    guint64 hash = 0;
    guint64 sum = 0;
    guint i;
    guint j;
    guint8 gray[64];

    pixbuf = gdk_pixbuf_new_from_file_at_scale(png_path, 8, 8, FALSE, error);
    if (pixbuf == NULL)
        return 0;

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);

    for (j = 0; j < (guint)height; j++) {
        for (i = 0; i < (guint)width; i++) {
            guchar *pixel = pixels + (j * rowstride) + (i * n_channels);
            guint8 value = (guint8)((pixel[0] * 30 + pixel[1] * 59 + pixel[2] * 11) / 100);

            gray[(j * width) + i] = value;
            sum += value;
        }
    }

    sum /= 64;
    for (i = 0; i < 64; i++) {
        if (gray[i] >= sum)
            hash |= ((guint64)1 << i);
    }

    g_object_unref(pixbuf);
    return hash;
}

guint skin_preview_hamming_distance(guint64 lhs, guint64 rhs)
{
    guint distance = 0;
    guint64 value = lhs ^ rhs;

    while (value != 0) {
        distance += (guint)(value & 1u);
        value >>= 1;
    }

    return distance;
}
