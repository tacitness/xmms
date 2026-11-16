#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xmmsctrl.h"

static volatile sig_atomic_t keep_running = 1;

static void stop_sync(int signal_number)
{
    (void)signal_number;
    keep_running = 0;
}

static gchar *remote_media_path(const gchar *source)
{
    gchar *scheme = g_uri_parse_scheme(source);

    if (scheme && g_str_equal(scheme, "file")) {
        gchar *path = g_filename_from_uri(source, NULL, NULL);
        g_free(scheme);
        return path;
    }

    if (scheme)
        return g_strdup(source);

    return g_path_is_absolute(source) ? g_strdup(source) : NULL;
}

static gchar *remote_destination(const gchar *source, const gchar *remote_dir)
{
    gchar *basename = g_path_get_basename(source);
    GString *safe_name = g_string_sized_new(strlen(basename));
    gchar *checksum;
    gchar *destination;
    gsize i;

    for (i = 0; basename[i] != '\0' && i < 160; i++) {
        gchar ch = basename[i];
        if (g_ascii_isalnum(ch) || ch == '.' || ch == '-' || ch == '_')
            g_string_append_c(safe_name, ch);
        else if (safe_name->len == 0 || safe_name->str[safe_name->len - 1] != '_')
            g_string_append_c(safe_name, '_');
    }

    checksum = g_compute_checksum_for_string(G_CHECKSUM_SHA256, source, -1);
    destination = g_strdup_printf("%s/%s/%s", remote_dir, checksum,
                                  safe_name->len ? safe_name->str : "track.audio");
    g_free(checksum);
    g_string_free(safe_name, TRUE);
    g_free(basename);
    return destination;
}

static gchar *legacy_remote_destination(const gchar *source, const gchar *remote_dir)
{
    gchar *basename = g_path_get_basename(source);
    gchar *dot = strrchr(basename, '.');
    gchar *extension = NULL;
    gchar *checksum;
    gchar *destination;
    gsize i;

    if (dot && dot != basename && dot[1] != '\0') {
        extension = g_strdup(dot + 1);
        for (i = 0; extension[i] != '\0'; i++) {
            if (!g_ascii_isalnum(extension[i])) {
                g_clear_pointer(&extension, g_free);
                break;
            }
        }
    }
    checksum = g_compute_checksum_for_string(G_CHECKSUM_SHA256, source, -1);
    destination = g_strdup_printf("%s/%s%s%s", remote_dir, checksum, extension ? "." : "",
                                  extension ? extension : "");
    g_free(checksum);
    g_free(extension);
    g_free(basename);
    return destination;
}

static gboolean copy_track(const gchar *host, const gchar *source, const gchar *destination)
{
    gchar *directory = g_path_get_dirname(destination);
    gchar *mkdir_command = g_strdup_printf("mkdir -p -- %s", directory);
    gchar *mkdir_argv[] = {"ssh", "-o", "BatchMode=yes", (gchar *)host, mkdir_command, NULL};
    gchar *remote_target = g_strdup_printf("%s:%s", host, destination);
    gchar *argv[] = {"scp",           "-q",          "-o", "BatchMode=yes", "--",
                     (gchar *)source, remote_target, NULL};
    gint exit_status = 0;
    GError *error = NULL;
    gboolean ok = g_spawn_sync(NULL, mkdir_argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL,
                               &exit_status, &error);

    if (!ok || !g_spawn_check_wait_status(exit_status, &error)) {
        g_printerr("xmms-remote-sync: could not create the remote track directory: %s\n",
                   error ? error->message : "ssh failed");
        g_clear_error(&error);
        g_free(directory);
        g_free(mkdir_command);
        g_free(remote_target);
        return FALSE;
    }

    ok = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &exit_status,
                      &error);

    if (!ok || !g_spawn_check_wait_status(exit_status, &error)) {
        g_printerr("xmms-remote-sync: could not copy %s to %s: %s\n", source, host,
                   error ? error->message : "scp failed");
        g_clear_error(&error);
        g_free(directory);
        g_free(mkdir_command);
        g_free(remote_target);
        return FALSE;
    }

    g_free(directory);
    g_free(mkdir_command);
    g_free(remote_target);
    return TRUE;
}

static gchar *sync_playlist(gint local_session, gint remote_session, const gchar *socket_dir,
                            const gchar *remote_dir, gint selected, gint position,
                            gchar **playlist_signature)
{
    xmms_remote_set_socket_dir(NULL);
    gint count = xmms_remote_get_playlist_length(local_session);
    gchar **tracks = g_new0(gchar *, count + 1);
    GString *signature = g_string_new(NULL);
    gchar *current = NULL;

    for (gint i = 0; i < count; i++) {
        gchar *source = xmms_remote_get_playlist_file(local_session, i);
        gchar *media = source ? remote_media_path(source) : NULL;
        if (!media) {
            g_printerr("xmms-remote-sync: playlist item %d has no usable media path\n", i + 1);
            xmms_remote_set_socket_dir(socket_dir);
            g_free(source);
            g_free(media);
            g_strfreev(tracks);
            g_string_free(signature, TRUE);
            return NULL;
        }
        if (g_path_is_absolute(media)) {
            gchar *normalized = g_canonicalize_filename(media, NULL);
            tracks[i] = remote_destination(normalized, remote_dir);
            g_free(normalized);
        } else {
            tracks[i] = g_strdup(media);
        }
        g_string_append_printf(signature, "%s\n", source);
        if (i == position)
            current = g_strdup(tracks[i]);
        g_free(source);
        g_free(media);
    }

    xmms_remote_set_socket_dir(socket_dir);
    if (count > 0) {
        gchar *digest = g_compute_checksum_for_string(G_CHECKSUM_SHA256, signature->str, -1);
        if (g_strcmp0(digest, *playlist_signature) == 0) {
            if (xmms_remote_get_playlist_pos(remote_session) != selected)
                xmms_remote_set_playlist_pos(remote_session, selected);
            g_free(digest);
            g_strfreev(tracks);
            g_string_free(signature, TRUE);
            return current;
        }
        xmms_remote_playlist(remote_session, tracks, count, FALSE);
        xmms_remote_set_playlist_pos(remote_session, selected);
        g_free(*playlist_signature);
        *playlist_signature = g_strdup(digest);
        g_free(digest);
    }
    g_strfreev(tracks);
    g_string_free(signature, TRUE);
    return current;
}

static gboolean sync_once(gint local_session, gint remote_session, const gchar *host,
                          const gchar *socket_dir, const gchar *remote_dir, gboolean start_current,
                          gchar **last_track, gint *last_position, gboolean *was_active,
                          gboolean *local_was_playing, gchar **playlist_signature)
{
    gint position;
    gboolean playing;
    gboolean paused;
    gint local_time;
    gchar *source = NULL;
    gchar *media = NULL;
    gchar *destination = NULL;
    gboolean changed = FALSE;

    xmms_remote_set_socket_dir(NULL);
    position = xmms_remote_get_playlist_pos(local_session);
    playing = xmms_remote_is_playing(local_session);
    paused = xmms_remote_is_paused(local_session);
    if (playing)
        *local_was_playing = TRUE;
    local_time = MAX(0, xmms_remote_get_output_time(local_session));
    if ((playing || start_current) && position >= 0) {
        source = xmms_remote_get_playlist_file(local_session, position);
        media = source ? remote_media_path(source) : NULL;
        changed = g_strcmp0(source, *last_track) != 0 || position != *last_position;
    }

    xmms_remote_set_socket_dir(socket_dir);
    if (!xmms_remote_is_running(remote_session)) {
        g_printerr("xmms-remote-sync: remote XMMS session %d is unavailable through the SSH "
                   "tunnel\n",
                   remote_session);
        g_free(media);
        g_free(source);
        return FALSE;
    }

    {
        gchar *current_remote = sync_playlist(local_session, remote_session, socket_dir, remote_dir,
                                              position, position, playlist_signature);
        if (current_remote) {
            g_free(current_remote);
        } else if (xmms_remote_get_playlist_length(local_session) > 0) {
            xmms_remote_set_socket_dir(socket_dir);
            g_printerr("xmms-remote-sync: could not resolve playlist selection %d of %d\n",
                       position, xmms_remote_get_playlist_length(remote_session));
            g_free(media);
            g_free(source);
            return FALSE;
        }
    }

    if ((!playing && (!start_current || *local_was_playing)) || position < 0) {
        if (*was_active || xmms_remote_is_playing(remote_session) ||
            xmms_remote_is_paused(remote_session)) {
            xmms_remote_stop(remote_session);
            *was_active = FALSE;
            g_print("Remote XMMS stopped with the local player.\n");
        }
        g_free(media);
        g_free(source);
        return TRUE;
    }

    {
        if (!media) {
            g_printerr("xmms-remote-sync: current track has no usable local path or URI\n");
            g_free(source);
            return FALSE;
        }

        if (changed) {
            if (g_path_is_absolute(media)) {
                gchar *normalized = g_canonicalize_filename(media, NULL);
                destination = remote_destination(normalized, remote_dir);
                g_free(normalized);
            } else {
                destination = g_strdup(media);
            }

            {
                gint remote_position = xmms_remote_get_playlist_pos(remote_session);
                gchar *remote_source =
                    remote_position >= 0
                        ? xmms_remote_get_playlist_file(remote_session, remote_position)
                        : NULL;
                gchar *legacy_destination =
                    g_path_is_absolute(media) ? legacy_remote_destination(media, remote_dir) : NULL;
                gboolean preserve_remote_position =
                    legacy_destination && g_strcmp0(remote_source, legacy_destination) == 0 &&
                    (xmms_remote_is_playing(remote_session) ||
                     xmms_remote_is_paused(remote_session));
                gint remote_time = preserve_remote_position
                                       ? MAX(0, xmms_remote_get_output_time(remote_session))
                                       : local_time;
                gboolean already_playing_track = g_strcmp0(remote_source, destination) == 0 &&
                                                 (xmms_remote_is_playing(remote_session) ||
                                                  xmms_remote_is_paused(remote_session));
                g_free(remote_source);
                g_free(legacy_destination);

                if (!already_playing_track) {
                    if (g_path_is_absolute(media) && !copy_track(host, media, destination)) {
                        g_free(destination);
                        g_free(media);
                        g_free(source);
                        return FALSE;
                    }
                    g_usleep(500000);
                    xmms_remote_jump_to_time(remote_session, remote_time);
                    g_print("Mirroring track: %s\n", source);
                } else {
                    g_print("Already playing selected track: %s\n", source);
                }
            }
            g_free(*last_track);
            *last_track = g_strdup(source);
            *last_position = position;
        } else if (playing) {
            gint remote_time = MAX(0, xmms_remote_get_output_time(remote_session));
            if (ABS(local_time - remote_time) > 2500)
                xmms_remote_jump_to_time(remote_session, local_time);
        }

        if (!playing && start_current) {
            if (!*was_active && !xmms_remote_is_playing(remote_session) &&
                !xmms_remote_is_paused(remote_session))
                xmms_remote_play(remote_session);
        } else if (paused) {
            if (!xmms_remote_is_playing(remote_session) && !xmms_remote_is_paused(remote_session))
                xmms_remote_play(remote_session);
            if (!xmms_remote_is_paused(remote_session))
                xmms_remote_pause(remote_session);
        } else if (!xmms_remote_is_playing(remote_session) ||
                   xmms_remote_is_paused(remote_session)) {
            xmms_remote_play(remote_session);
        }

        *was_active = TRUE;
        g_free(destination);
        g_free(media);
        g_free(source);
    }

    return TRUE;
}

int main(int argc, char **argv)
{
    gint local_session = 0;
    gint remote_session = 0;
    gint interval = 1;
    gchar *host = NULL;
    gchar *socket_dir = NULL;
    gchar *remote_dir = NULL;
    gboolean once = FALSE;
    gboolean start_current = FALSE;
    GOptionEntry entries[] =
        {{"host", 'H', 0, G_OPTION_ARG_STRING, &host, "SSH host alias", "HOST"},
         {"socket-dir", 0, 0, G_OPTION_ARG_STRING, &socket_dir,
          "Local SSH-forwarded socket directory", "DIR"},
         {"remote-dir", 0, 0, G_OPTION_ARG_STRING, &remote_dir, "Remote temporary media directory",
          "DIR"},
         {"session", 'n', 0, G_OPTION_ARG_INT, &local_session, "Local XMMS session", "N"},
         {"remote-session", 0, 0, G_OPTION_ARG_INT, &remote_session, "Remote XMMS session", "N"},
         {"interval", 'i', 0, G_OPTION_ARG_INT, &interval, "Follow interval in seconds", "SECONDS"},
         {"once", 0, 0, G_OPTION_ARG_NONE, &once, "Synchronize once and exit", NULL},
         {"start-current", 0, 0, G_OPTION_ARG_NONE, &start_current,
          "Play the selected local track remotely even if local playback is stopped", NULL},
         {NULL}};
    GOptionContext *context =
        g_option_context_new("- mirror local XMMS playback to a remote instance");
    GError *error = NULL;
    gchar *last_track = NULL;
    gint last_position = -1;
    gboolean was_active = FALSE;
    gboolean local_was_playing = FALSE;
    gchar *playlist_signature = NULL;
    gboolean ok = TRUE;

    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("xmms-remote-sync: %s\n", error->message);
        g_error_free(error);
        g_option_context_free(context);
        return 2;
    }
    g_option_context_free(context);

    if (!host || !socket_dir || !remote_dir || interval < 1 || local_session < 0 ||
        remote_session < 0) {
        g_printerr("xmms-remote-sync: --host, --socket-dir, and --remote-dir are required; session "
                   "numbers must be nonnegative and interval positive\n");
        return 2;
    }

    if (!xmms_remote_is_running(local_session)) {
        g_printerr("xmms-remote-sync: local XMMS session %d is not running\n", local_session);
        return 1;
    }

    signal(SIGINT, stop_sync);
    signal(SIGTERM, stop_sync);
    while (keep_running) {
        if (!sync_once(local_session, remote_session, host, socket_dir, remote_dir, start_current,
                       &last_track, &last_position, &was_active, &local_was_playing,
                       &playlist_signature)) {
            ok = FALSE;
            break;
        }
        if (once)
            break;
        for (gint remaining = interval; remaining > 0 && keep_running; remaining--)
            sleep(1);
    }

    g_free(last_track);
    g_free(playlist_signature);
    g_free(host);
    g_free(socket_dir);
    g_free(remote_dir);
    return ok ? 0 : 1;
}
