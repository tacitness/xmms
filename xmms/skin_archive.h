#ifndef SKIN_ARCHIVE_H
#define SKIN_ARCHIVE_H

#include <glib.h>

gboolean skin_archive_path_is_safe(const gchar *entry_name);
gchar *skin_archive_extract_to_tempdir(const gchar *path, GError **error);

#endif
