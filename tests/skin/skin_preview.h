#ifndef TESTS_SKIN_PREVIEW_H
#define TESTS_SKIN_PREVIEW_H

#include <glib.h>

gboolean skin_preview_write_png(const gchar *skin_path, const gchar *output_png, GError **error);
guint64 skin_preview_png_ahash(const gchar *png_path, GError **error);
guint skin_preview_hamming_distance(guint64 lhs, guint64 rhs);

#endif
