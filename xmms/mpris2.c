/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2003  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2004  Haavard Kvaalen
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

/*
 * mpris2.c — MPRIS 2.2 D-Bus service for XMMS
 *
 * Registers org.mpris.MediaPlayer2.xmms on the session bus so that:
 *  - GNOME media keys (XF86AudioPlay/Pause/Stop/Next/Prev) control playback
 *  - playerctl and other MPRIS2 clients work
 *  - GNOME Shell "Now Playing" overlay shows current track
 *
 * Uses GDBus (part of GLib 2.26+, already a mandatory dependency).
 * Closes #37.
 */

#include "mpris2.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include <gio/gio.h>

/* xmms.h is the catch-all — includes plugin.h, widget.h, skin.h, vis.h and
 * the full set of xmms headers in the required dependency order. */
#include "xmms.h"

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */

#define MPRIS2_SERVICE "org.mpris.MediaPlayer2.xmms"
#define MPRIS2_OBJECT_PATH "/org/mpris/MediaPlayer2"
#define MPRIS2_ROOT_IFC "org.mpris.MediaPlayer2"
#define MPRIS2_PLAYER_IFC "org.mpris.MediaPlayer2.Player"
#define MPRIS2_PROPS_IFC "org.freedesktop.DBus.Properties"

/* ------------------------------------------------------------------ */
/* Introspection XML                                                    */
/* ------------------------------------------------------------------ */

static const gchar MPRIS2_XML[] =
    "<node>"
    "  <interface name='org.mpris.MediaPlayer2'>"
    "    <method name='Raise'/>"
    "    <method name='Quit'/>"
    "    <property name='CanQuit'             type='b'  access='read'/>"
    "    <property name='CanRaise'            type='b'  access='read'/>"
    "    <property name='HasTrackList'        type='b'  access='read'/>"
    "    <property name='Identity'            type='s'  access='read'/>"
    "    <property name='DesktopEntry'        type='s'  access='read'/>"
    "    <property name='SupportedUriSchemes' type='as' access='read'/>"
    "    <property name='SupportedMimeTypes'  type='as' access='read'/>"
    "  </interface>"
    "  <interface name='org.mpris.MediaPlayer2.Player'>"
    "    <method name='Next'/>"
    "    <method name='Previous'/>"
    "    <method name='Pause'/>"
    "    <method name='PlayPause'/>"
    "    <method name='Stop'/>"
    "    <method name='Play'/>"
    "    <method name='Seek'>"
    "      <arg direction='in' type='x' name='Offset'/>"
    "    </method>"
    "    <method name='SetPosition'>"
    "      <arg direction='in' type='o' name='TrackId'/>"
    "      <arg direction='in' type='x' name='Position'/>"
    "    </method>"
    "    <method name='OpenUri'>"
    "      <arg direction='in' type='s' name='Uri'/>"
    "    </method>"
    "    <property name='PlaybackStatus' type='s'     access='read'/>"
    "    <property name='LoopStatus'     type='s'     access='readwrite'/>"
    "    <property name='Rate'           type='d'     access='readwrite'/>"
    "    <property name='Shuffle'        type='b'     access='readwrite'/>"
    "    <property name='Metadata'       type='a{sv}' access='read'/>"
    "    <property name='Volume'         type='d'     access='readwrite'/>"
    "    <property name='Position'       type='x'     access='read'/>"
    "    <property name='MinimumRate'    type='d'     access='read'/>"
    "    <property name='MaximumRate'    type='d'     access='read'/>"
    "    <property name='CanGoNext'      type='b'     access='read'/>"
    "    <property name='CanGoPrevious'  type='b'     access='read'/>"
    "    <property name='CanPlay'        type='b'     access='read'/>"
    "    <property name='CanPause'       type='b'     access='read'/>"
    "    <property name='CanSeek'        type='b'     access='read'/>"
    "    <property name='CanControl'     type='b'     access='read'/>"
    "    <signal name='Seeked'>"
    "      <arg name='Position' type='x'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

/* ------------------------------------------------------------------ */
/* Module state                                                         */
/* ------------------------------------------------------------------ */

static GDBusConnection *mpris2_conn = NULL;
static guint mpris2_regid_root = 0;
static guint mpris2_regid_player = 0;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static const gchar *_playback_status(void)
{
    if (!get_input_playing())
        return "Stopped";
    if (get_input_paused())
        return "Paused";
    return "Playing";
}

static GVariant *_build_metadata(void)
{
    GVariantBuilder b;
    gint pos, len;
    gchar *title, *filename, *trackid;

    g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));

    pos = get_playlist_position();
    title = playlist_get_songtitle(pos);
    filename = playlist_get_filename(pos);
    len = playlist_get_songtime(pos); /* milliseconds, -1 if unknown */

    /* mpris:trackid — must be a valid D-Bus object path */
    trackid = g_strdup_printf("%s/Track/%d", MPRIS2_OBJECT_PATH, pos);
    g_variant_builder_add(&b, "{sv}", "mpris:trackid", g_variant_new_object_path(trackid));
    g_free(trackid);

    /* mpris:length — MPRIS2 uses microseconds */
    if (len > 0)
        g_variant_builder_add(&b, "{sv}", "mpris:length", g_variant_new_int64((gint64)len * 1000));

    /* xesam:title */
    if (title && *title)
        g_variant_builder_add(&b, "{sv}", "xesam:title", g_variant_new_string(title));
    g_free(title);

    /* xesam:url — convert absolute path to file:// URI */
    if (filename && *filename) {
        gchar *uri = NULL;
        if (g_path_is_absolute(filename))
            uri = g_filename_to_uri(filename, NULL, NULL);
        if (!uri)
            uri = g_strdup(filename); /* already a URI (http:// etc.) */
        g_variant_builder_add(&b, "{sv}", "xesam:url", g_variant_new_string(uri));
        g_free(uri);
    }
    g_free(filename);

    return g_variant_builder_end(&b);
}

static GVariant *_get_volume_variant(void)
{
    int l = 0, r = 0;
    input_get_volume(&l, &r);
    return g_variant_new_double(CLAMP(MAX(l, r), 0, 100) / 100.0);
}

/* ------------------------------------------------------------------ */
/* org.mpris.MediaPlayer2 (root interface)                             */
/* ------------------------------------------------------------------ */

static void root_handle_method(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path, const gchar *interface_name,
                               const gchar *method_name, GVariant *parameters,
                               GDBusMethodInvocation *invocation, gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;
    (void)parameters;
    (void)user_data;

    if (g_strcmp0(method_name, "Raise") == 0) {
        gtk_window_present(GTK_WINDOW(mainwin));
    } else if (g_strcmp0(method_name, "Quit") == 0) {
        mainwin_quit_cb();
    }
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static GVariant *root_handle_get_property(GDBusConnection *connection, const gchar *sender,
                                          const gchar *object_path, const gchar *interface_name,
                                          const gchar *property_name, GError **error,
                                          gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;
    (void)error;
    (void)user_data;

    static const gchar *uri_schemes[] = {"file", NULL};
    static const gchar *mime_types[] = {"audio/mpeg",  "audio/ogg",          "audio/flac",
                                        "audio/x-wav", "audio/x-vorbis+ogg", "audio/x-flac",
                                        NULL};

    if (g_strcmp0(property_name, "CanQuit") == 0)
        return g_variant_new_boolean(TRUE);
    if (g_strcmp0(property_name, "CanRaise") == 0)
        return g_variant_new_boolean(TRUE);
    if (g_strcmp0(property_name, "HasTrackList") == 0)
        return g_variant_new_boolean(FALSE);
    if (g_strcmp0(property_name, "Identity") == 0)
        return g_variant_new_string("XMMS");
    if (g_strcmp0(property_name, "DesktopEntry") == 0)
        return g_variant_new_string("xmms");
    if (g_strcmp0(property_name, "SupportedUriSchemes") == 0)
        return g_variant_new_strv(uri_schemes, -1);
    if (g_strcmp0(property_name, "SupportedMimeTypes") == 0)
        return g_variant_new_strv(mime_types, -1);

    return NULL;
}

static const GDBusInterfaceVTable root_vtable = {root_handle_method, root_handle_get_property,
                                                 NULL};

/* ------------------------------------------------------------------ */
/* org.mpris.MediaPlayer2.Player                                       */
/* ------------------------------------------------------------------ */

static void player_handle_method(GDBusConnection *connection, const gchar *sender,
                                 const gchar *object_path, const gchar *interface_name,
                                 const gchar *method_name, GVariant *parameters,
                                 GDBusMethodInvocation *invocation, gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;
    (void)user_data;

    if (g_strcmp0(method_name, "Play") == 0) {
        if (get_input_paused())
            input_pause(); /* unpause */
        else if (get_playlist_length())
            playlist_play();
    } else if (g_strcmp0(method_name, "Pause") == 0) {
        if (get_input_playing() && !get_input_paused())
            input_pause();
    } else if (g_strcmp0(method_name, "PlayPause") == 0) {
        if (get_input_playing())
            input_pause();
        else if (get_playlist_length())
            playlist_play();
    } else if (g_strcmp0(method_name, "Stop") == 0) {
        mainwin_stop_pushed();
    } else if (g_strcmp0(method_name, "Next") == 0) {
        playlist_next();
    } else if (g_strcmp0(method_name, "Previous") == 0) {
        playlist_prev();
    } else if (g_strcmp0(method_name, "Seek") == 0) {
        gint64 offset_us;
        g_variant_get(parameters, "(x)", &offset_us);
        if (get_input_playing() && playlist_get_current_length() > 0) {
            gint new_ms = (gint)((gint64)input_get_time() + offset_us / 1000);
            new_ms = CLAMP(new_ms, 0, playlist_get_current_length());
            input_seek(new_ms / 1000);
        }
    } else if (g_strcmp0(method_name, "SetPosition") == 0) {
        const gchar *track_id;
        gint64 pos_us;
        g_variant_get(parameters, "(&ox)", &track_id, &pos_us);
        if (get_input_playing() && playlist_get_current_length() > 0) {
            gint ms = (gint)(pos_us / 1000);
            ms = CLAMP(ms, 0, playlist_get_current_length());
            input_seek(ms / 1000);
        }
    } else if (g_strcmp0(method_name, "OpenUri") == 0) {
        const gchar *uri;
        g_variant_get(parameters, "(&s)", &uri);
        if (uri && *uri) {
            playlist_ins_url_string((gchar *)uri, -1);
            playlist_set_position(get_playlist_length() - 1);
            playlist_play();
        }
    }

    g_dbus_method_invocation_return_value(invocation, NULL);
}

static GVariant *player_handle_get_property(GDBusConnection *connection, const gchar *sender,
                                            const gchar *object_path, const gchar *interface_name,
                                            const gchar *property_name, GError **error,
                                            gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;
    (void)error;
    (void)user_data;

    gboolean playing = get_input_playing();
    gboolean has_list = get_playlist_length() > 0;
    gboolean can_seek = playing && (playlist_get_current_length() > 0);

    if (g_strcmp0(property_name, "PlaybackStatus") == 0)
        return g_variant_new_string(_playback_status());
    if (g_strcmp0(property_name, "LoopStatus") == 0)
        return g_variant_new_string(cfg.repeat ? "Track" : "None");
    if (g_strcmp0(property_name, "Rate") == 0)
        return g_variant_new_double(1.0);
    if (g_strcmp0(property_name, "Shuffle") == 0)
        return g_variant_new_boolean(cfg.shuffle);
    if (g_strcmp0(property_name, "Metadata") == 0)
        return _build_metadata();
    if (g_strcmp0(property_name, "Volume") == 0)
        return _get_volume_variant();
    if (g_strcmp0(property_name, "Position") == 0)
        return g_variant_new_int64(playing ? (gint64)input_get_time() * 1000 : 0);
    if (g_strcmp0(property_name, "MinimumRate") == 0)
        return g_variant_new_double(1.0);
    if (g_strcmp0(property_name, "MaximumRate") == 0)
        return g_variant_new_double(1.0);
    if (g_strcmp0(property_name, "CanGoNext") == 0)
        return g_variant_new_boolean(has_list);
    if (g_strcmp0(property_name, "CanGoPrevious") == 0)
        return g_variant_new_boolean(has_list);
    if (g_strcmp0(property_name, "CanPlay") == 0)
        return g_variant_new_boolean(has_list);
    if (g_strcmp0(property_name, "CanPause") == 0)
        return g_variant_new_boolean(playing);
    if (g_strcmp0(property_name, "CanSeek") == 0)
        return g_variant_new_boolean(can_seek);
    if (g_strcmp0(property_name, "CanControl") == 0)
        return g_variant_new_boolean(TRUE);

    return NULL;
}

static gboolean player_handle_set_property(GDBusConnection *connection, const gchar *sender,
                                           const gchar *object_path, const gchar *interface_name,
                                           const gchar *property_name, GVariant *value,
                                           GError **error, gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;
    (void)error;
    (void)user_data;

    if (g_strcmp0(property_name, "Volume") == 0) {
        int v = (int)CLAMP(g_variant_get_double(value) * 100.0, 0.0, 100.0);
        input_set_volume(v, v);
    } else if (g_strcmp0(property_name, "LoopStatus") == 0) {
        gboolean repeat = g_strcmp0(g_variant_get_string(value, NULL), "None") != 0;
        mainwin_repeat_pushed(repeat);
    } else if (g_strcmp0(property_name, "Shuffle") == 0) {
        mainwin_shuffle_pushed(g_variant_get_boolean(value));
    }
    /* Rate changes not supported — silently accept */
    return TRUE;
}

static const GDBusInterfaceVTable player_vtable = {player_handle_method, player_handle_get_property,
                                                   player_handle_set_property};

/* ------------------------------------------------------------------ */
/* Bus ownership callbacks                                              */
/* ------------------------------------------------------------------ */

static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    GDBusNodeInfo *info;
    GError *err = NULL;

    (void)name;
    (void)user_data;
    mpris2_conn = connection;

    info = g_dbus_node_info_new_for_xml(MPRIS2_XML, &err);
    if (!info) {
        g_warning("mpris2: failed to parse introspection XML: %s", err->message);
        g_error_free(err);
        return;
    }

    mpris2_regid_root =
        g_dbus_connection_register_object(connection, MPRIS2_OBJECT_PATH,
                                          g_dbus_node_info_lookup_interface(info, MPRIS2_ROOT_IFC),
                                          &root_vtable, NULL, NULL, &err);
    if (!mpris2_regid_root) {
        g_warning("mpris2: failed to register root interface: %s", err->message);
        g_clear_error(&err);
    }

    mpris2_regid_player =
        g_dbus_connection_register_object(connection, MPRIS2_OBJECT_PATH,
                                          g_dbus_node_info_lookup_interface(info,
                                                                            MPRIS2_PLAYER_IFC),
                                          &player_vtable, NULL, NULL, &err);
    if (!mpris2_regid_player) {
        g_warning("mpris2: failed to register player interface: %s", err->message);
        g_clear_error(&err);
    }

    g_dbus_node_info_unref(info);
}

static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    (void)connection;
    (void)user_data;
    g_warning("mpris2: lost D-Bus name '%s' — media key integration unavailable", name);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void mpris2_init(void)
{
    g_bus_own_name(G_BUS_TYPE_SESSION, MPRIS2_SERVICE, G_BUS_NAME_OWNER_FLAGS_NONE, on_bus_acquired,
                   NULL, on_name_lost, NULL, NULL);
}

void mpris2_update_state(void)
{
    GVariantBuilder changed, invalidated;
    GError *err = NULL;
    gboolean playing = get_input_playing();
    gboolean has_list = get_playlist_length() > 0;

    if (!mpris2_conn)
        return;

    g_variant_builder_init(&changed, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_init(&invalidated, G_VARIANT_TYPE("as"));

    g_variant_builder_add(&changed, "{sv}", "PlaybackStatus",
                          g_variant_new_string(_playback_status()));
    g_variant_builder_add(&changed, "{sv}", "Metadata", _build_metadata());
    g_variant_builder_add(&changed, "{sv}", "CanGoNext", g_variant_new_boolean(has_list));
    g_variant_builder_add(&changed, "{sv}", "CanGoPrevious", g_variant_new_boolean(has_list));
    g_variant_builder_add(&changed, "{sv}", "CanPlay", g_variant_new_boolean(has_list));
    g_variant_builder_add(&changed, "{sv}", "CanPause", g_variant_new_boolean(playing));
    g_variant_builder_add(&changed, "{sv}", "CanSeek",
                          g_variant_new_boolean(playing && playlist_get_current_length() > 0));

    g_dbus_connection_emit_signal(mpris2_conn, NULL, MPRIS2_OBJECT_PATH, MPRIS2_PROPS_IFC,
                                  "PropertiesChanged",
                                  g_variant_new("(sa{sv}as)", MPRIS2_PLAYER_IFC, &changed,
                                                &invalidated),
                                  &err);

    if (err) {
        g_warning("mpris2: PropertiesChanged emit failed: %s", err->message);
        g_error_free(err);
    }
}
