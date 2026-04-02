#include "skin_archive.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

static gboolean skin_archive_has_windows_drive_prefix(const gchar *path)
{
    return g_ascii_isalpha(path[0]) && path[1] == ':';
}

static gboolean skin_archive_run_command(gchar **argv, gchar **stdout_data, gchar **stderr_data,
                                         gint *status, GError **error)
{
    return g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, stdout_data,
                        stderr_data, status, error);
}

static const gchar *skin_archive_get_tool(const gchar *env_name, const gchar *fallback)
{
    const gchar *tool = g_getenv(env_name);

    if (tool == NULL || *tool == '\0')
        return fallback;
    return tool;
}

static gboolean skin_archive_validate_listing_output(const gchar *listing, GError **error)
{
    gchar **lines;
    gint i;

    if (listing == NULL || *listing == '\0') {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "Archive does not contain any files");
        return FALSE;
    }

    lines = g_strsplit(listing, "\n", -1);
    for (i = 0; lines[i] != NULL; i++) {
        if (*lines[i] == '\0')
            continue;
        if (!skin_archive_path_is_safe(lines[i])) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                        "Unsafe archive entry rejected: %s", lines[i]);
            g_strfreev(lines);
            return FALSE;
        }
    }

    g_strfreev(lines);
    return TRUE;
}

static gboolean skin_archive_validate_with_unzip(const gchar *path, GError **error)
{
    const gchar *unzip;
    gchar *argv[] = {(gchar *)"", (gchar *)"-Z1", (gchar *)path, NULL};
    gchar *stdout_data = NULL;
    gchar *stderr_data = NULL;
    gint status = 0;

    unzip = skin_archive_get_tool("UNZIPCMD", "unzip");
    argv[0] = (gchar *)unzip;

    if (!skin_archive_run_command(argv, &stdout_data, &stderr_data, &status, error))
        return FALSE;

    if (!g_spawn_check_wait_status(status, error)) {
        g_free(stdout_data);
        g_free(stderr_data);
        return FALSE;
    }

    if (!skin_archive_validate_listing_output(stdout_data, error)) {
        g_free(stdout_data);
        g_free(stderr_data);
        return FALSE;
    }

    g_free(stdout_data);
    g_free(stderr_data);
    return TRUE;
}

static gboolean skin_archive_validate_with_tar(const gchar *path, const gchar *mode, GError **error)
{
    const gchar *tar;
    gchar *argv[] = {(gchar *)"", (gchar *)"", (gchar *)path, NULL};
    gchar *stdout_data = NULL;
    gchar *stderr_data = NULL;
    gint status = 0;

    tar = skin_archive_get_tool("TARCMD", "tar");
    argv[0] = (gchar *)tar;
    argv[1] = (gchar *)mode;

    if (!skin_archive_run_command(argv, &stdout_data, &stderr_data, &status, error))
        return FALSE;

    if (!g_spawn_check_wait_status(status, error)) {
        g_free(stdout_data);
        g_free(stderr_data);
        return FALSE;
    }

    if (!skin_archive_validate_listing_output(stdout_data, error)) {
        g_free(stdout_data);
        g_free(stderr_data);
        return FALSE;
    }

    g_free(stdout_data);
    g_free(stderr_data);
    return TRUE;
}

static gboolean skin_archive_validate(const gchar *path, const gchar *ending, GError **error)
{
    if (!g_ascii_strcasecmp(ending, ".zip") || !g_ascii_strcasecmp(ending, ".wsz"))
        return skin_archive_validate_with_unzip(path, error);
    if (!g_ascii_strcasecmp(ending, ".tgz") || !g_ascii_strcasecmp(ending, ".gz"))
        return skin_archive_validate_with_tar(path, "-tzf", error);
    if (!g_ascii_strcasecmp(ending, ".bz2"))
        return skin_archive_validate_with_tar(path, "-tjf", error);
    if (!g_ascii_strcasecmp(ending, ".tar"))
        return skin_archive_validate_with_tar(path, "-tf", error);

    g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOSYS, "Unsupported skin archive format: %s",
                ending);
    return FALSE;
}

static gboolean skin_archive_extract_with_unzip(const gchar *archive_path, const gchar *tempdir,
                                                GError **error)
{
    const gchar *unzip;
    gchar *argv[] = {(gchar *)"", (gchar *)"-qq", (gchar *)"-o", (gchar *)"-j",
                     (gchar *)archive_path, (gchar *)"-d", (gchar *)tempdir, NULL};
    gchar *stdout_data = NULL;
    gchar *stderr_data = NULL;
    gint status = 0;

    unzip = skin_archive_get_tool("UNZIPCMD", "unzip");
    argv[0] = (gchar *)unzip;

    if (!skin_archive_run_command(argv, &stdout_data, &stderr_data, &status, error))
        return FALSE;

    g_free(stdout_data);
    g_free(stderr_data);
    return g_spawn_check_wait_status(status, error);
}

static gboolean skin_archive_extract_with_tar(const gchar *archive_path, const gchar *tempdir,
                                              const gchar *mode, GError **error)
{
    const gchar *tar;
    gchar *argv[] = {(gchar *)"", (gchar *)"", (gchar *)archive_path, (gchar *)"-C",
                     (gchar *)tempdir, NULL};
    gchar *stdout_data = NULL;
    gchar *stderr_data = NULL;
    gint status = 0;

    tar = skin_archive_get_tool("TARCMD", "tar");
    argv[0] = (gchar *)tar;
    argv[1] = (gchar *)mode;

    if (!skin_archive_run_command(argv, &stdout_data, &stderr_data, &status, error))
        return FALSE;

    g_free(stdout_data);
    g_free(stderr_data);
    return g_spawn_check_wait_status(status, error);
}

gboolean skin_archive_path_is_safe(const gchar *entry_name)
{
    gchar *normalized;
    gchar **parts;
    gint i;
    gboolean safe = TRUE;

    g_return_val_if_fail(entry_name != NULL, FALSE);

    if (*entry_name == '\0')
        return FALSE;

    normalized = g_strdup(entry_name);
    g_strdelimit(normalized, "\\", '/');

    if (normalized[0] == '/' || skin_archive_has_windows_drive_prefix(normalized)) {
        g_free(normalized);
        return FALSE;
    }

    parts = g_strsplit(normalized, "/", -1);
    for (i = 0; parts[i] != NULL; i++) {
        if (*parts[i] == '\0' || !strcmp(parts[i], "."))
            continue;
        if (!strcmp(parts[i], "..")) {
            safe = FALSE;
            break;
        }
    }

    g_strfreev(parts);
    g_free(normalized);
    return safe;
}

gchar *skin_archive_extract_to_tempdir(const gchar *path, GError **error)
{
    const gchar *ending;
    gchar *tempdir;

    g_return_val_if_fail(path != NULL, NULL);

    ending = strrchr(path, '.');
    if (ending == NULL) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Skin archive path does not have a filename extension");
        return NULL;
    }

    if (!skin_archive_validate(path, ending, error))
        return NULL;

    tempdir = g_build_filename(g_get_tmp_dir(), "xmmsskin.XXXXXX", NULL);
    if (g_mkdtemp(tempdir) == NULL) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Failed to create temporary directory: %s", g_strerror(errno));
        g_free(tempdir);
        return NULL;
    }

    if (!g_ascii_strcasecmp(ending, ".zip") || !g_ascii_strcasecmp(ending, ".wsz")) {
        if (!skin_archive_extract_with_unzip(path, tempdir, error))
            goto failure;
    } else if (!g_ascii_strcasecmp(ending, ".tgz") || !g_ascii_strcasecmp(ending, ".gz")) {
        if (!skin_archive_extract_with_tar(path, tempdir, "-xzf", error))
            goto failure;
    } else if (!g_ascii_strcasecmp(ending, ".bz2")) {
        if (!skin_archive_extract_with_tar(path, tempdir, "-xjf", error))
            goto failure;
    } else if (!g_ascii_strcasecmp(ending, ".tar")) {
        if (!skin_archive_extract_with_tar(path, tempdir, "-xf", error))
            goto failure;
    } else {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOSYS,
                    "Unsupported skin archive format: %s", ending);
        goto failure;
    }

    return tempdir;

failure:
    g_rmdir(tempdir);
    g_free(tempdir);
    return NULL;
}
