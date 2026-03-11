/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2001  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2006  Haavard Kvaalen <havardk@xmms.org>
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
#include <X11/Xlib.h>
#include <ctype.h>
/* GTK3: gdk/gdkprivate.h removed */
#include <sys/ipc.h>

#include "xmms.h"
#ifdef HAVE_FTS_H
#    include <fts.h>
#endif

static GQuark quark_popup_data;


/*
 * find_file_recursively() by J�rg Schuler Wed, 17 Feb 1999 23:50:52
 * +0100 Placed under GPL version 2 or (at your option) any later
 * version
 */
/*
 * find <file> in directory <dirname> or subdirectories.  return
 * pointer to complete filename which has to be freed by calling
 * "g_free()" after use. Returns NULL if file could not be found.
 */
gchar *find_file_recursively(const char *dirname, const char *file)
{
    DIR *dir;
    struct dirent *dirent;
    char *result, *found;
    struct stat buf;

    if ((dir = opendir(dirname)) == NULL)
        return NULL;

    while ((dirent = readdir(dir)) != NULL) {
        /* Don't scan "." and ".." */
        if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
            continue;
        /* We need this in order to find out if file is directory */
        found = g_strconcat(dirname, "/", dirent->d_name, NULL);
        if (stat(found, &buf) != 0) {
            /* Error occurred */
            g_free(found);
            closedir(dir);
            return NULL;
        }
        if (!S_ISDIR(buf.st_mode)) {
            /* Normal file -- maybe just what we are looking for? */
            if (!strcasecmp(dirent->d_name, file)) {
                if (strlen(found) > FILENAME_MAX) {
                    /* No good! File + path too long */
                    g_free(found);
                    closedir(dir);
                    return NULL;
                } else {
                    closedir(dir);
                    return found;
                }
            }
        }
        g_free(found);
    }
    rewinddir(dir);
    while ((dirent = readdir(dir)) != NULL) {
        /* Don't scan "." and ".." */
        if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
            continue;
        /* We need this in order to find out if file is directory */
        found = g_strconcat(dirname, "/", dirent->d_name, NULL);
        if (stat(found, &buf) != 0) {
            /* Error occurred */
            g_free(found);
            closedir(dir);
            return NULL;
        }
        if (S_ISDIR(buf.st_mode)) {
            /* Found directory -- descend into it */
            result = find_file_recursively(found, file);
            if (result != NULL) { /* Found file there -- exit */
                g_free(found);
                closedir(dir);
                return result;
            }
        }
        g_free(found);
    }
    closedir(dir);
    return NULL;
}

void del_directory(const char *dirname)
{
#ifdef HAVE_FTS_H
    char *const argv[2] = {(char *)dirname, NULL};
    FTS *fts;
    FTSENT *p;

    fts = fts_open(argv, FTS_PHYSICAL, (int (*)())NULL);
    while ((p = fts_read(fts)) != NULL) {
        switch (p->fts_info) {
        case FTS_D:
            break;
        case FTS_DNR:
        case FTS_ERR:
            break;
        case FTS_DP:
            rmdir(p->fts_accpath);
            break;
        default:
            unlink(p->fts_accpath);
            break;
        }
    }
    fts_close(fts);
#else  /* !HAVE_FTS_H */
    DIR *dir;
    struct dirent *dirent;
    char *file;

    if ((dir = opendir(dirname)) != NULL) {
        while ((dirent = readdir(dir)) != NULL) {
            if (strcmp(dirent->d_name, ".") && strcmp(dirent->d_name, "..")) {
                file = g_strdup_printf("%s/%s", dirname, dirent->d_name);
                if (unlink(file) == -1)
                    if (errno == EISDIR)
                        del_directory(file);
                g_free(file);
            }
        }
        closedir(dir);
    }
    rmdir(dirname);
#endif /* !HAVE_FTS_H */
}

/* GTK3: pixel-doubling via cairo scaling - replaces GdkImage X11 approach */
cairo_surface_t *create_dblsize_surface(cairo_surface_t *src_surf)
{
    gint w = cairo_image_surface_get_width(src_surf);
    gint h = cairo_image_surface_get_height(src_surf);
    cairo_surface_t *dst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w * 2, h * 2);
    cairo_t *cr = cairo_create(dst);
    cairo_pattern_t *pat;

    cairo_scale(cr, 2.0, 2.0);
    cairo_set_source_surface(cr, src_surf, 0.0, 0.0);
    pat = cairo_get_source(cr);
    cairo_pattern_set_filter(pat, CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_destroy(cr);
    return dst;
}

static char *read_string(const char *filename, const char *section, const char *key,
                         gboolean comment)
{
    FILE *file;
    char *buffer, *ret_buffer = NULL;
    int found_section = 0, off = 0, len = 0;
    struct stat statbuf;

    if (!filename)
        return NULL;

    if ((file = fopen(filename, "r")) == NULL)
        return NULL;

    if (stat(filename, &statbuf) < 0) {
        fclose(file);
        return NULL;
    }

    buffer = g_malloc(statbuf.st_size);
    fread(buffer, 1, statbuf.st_size, file);
    while (!ret_buffer && off < statbuf.st_size) {
        while (off < statbuf.st_size && (buffer[off] == '\r' || buffer[off] == '\n' ||
                                         buffer[off] == ' ' || buffer[off] == '\t'))
            off++;
        if (off >= statbuf.st_size)
            break;
        if (buffer[off] == '[') {
            int slen = strlen(section);
            off++;
            found_section = 0;
            if (off + slen + 1 < statbuf.st_size && !strncasecmp(section, &buffer[off], slen)) {
                off += slen;
                if (buffer[off] == ']') {
                    off++;
                    found_section = 1;
                }
            }
        } else if (found_section && off + strlen(key) < statbuf.st_size &&
                   !strncasecmp(key, &buffer[off], strlen(key))) {
            off += strlen(key);
            while (off < statbuf.st_size && (buffer[off] == ' ' || buffer[off] == '\t'))
                off++;
            if (off >= statbuf.st_size)
                break;
            if (buffer[off] == '=') {
                off++;
                while (off < statbuf.st_size && (buffer[off] == ' ' || buffer[off] == '\t'))
                    off++;
                if (off >= statbuf.st_size)
                    break;
                len = 0;
                while (off + len < statbuf.st_size && buffer[off + len] != '\r' &&
                       buffer[off + len] != '\n' && (!comment || buffer[off + len] != ';'))
                    len++;
                ret_buffer = g_strndup(&buffer[off], len);
                off += len;
            }
        }
        while (off < statbuf.st_size && buffer[off] != '\r' && buffer[off] != '\n')
            off++;
    }

    g_free(buffer);
    fclose(file);
    return ret_buffer;
}

char *read_ini_string(const char *filename, const char *section, const char *key)
{
    return read_string(filename, section, key, TRUE);
}

char *read_ini_string_no_comment(const char *filename, const char *section, const char *key)
{
    return read_string(filename, section, key, FALSE);
}

GArray *string_to_garray(const gchar *str)
{
    GArray *array;
    gint temp;
    const gchar *ptr = str;
    gchar *endptr;

    array = g_array_new(FALSE, TRUE, sizeof(gint));
    for (;;) {
        temp = strtol(ptr, &endptr, 10);
        if (ptr == endptr)
            break;
        g_array_append_val(array, temp);
        ptr = endptr;
        while (!isdigit(*ptr) && (*ptr) != '\0')
            ptr++;
        if (*ptr == '\0')
            break;
    }
    return (array);
}

GArray *read_ini_array(const gchar *filename, const gchar *section, const gchar *key)
{
    gchar *temp;
    GArray *a;

    if ((temp = read_ini_string(filename, section, key)) == NULL)
        return NULL;
    a = string_to_garray(temp);
    g_free(temp);
    return a;
}

void glist_movedown(GList *list)
{
    gpointer temp;

    if (g_list_next(list)) {
        temp = list->data;
        list->data = list->next->data;
        list->next->data = temp;
    }
}

void glist_moveup(GList *list)
{
    gpointer temp;

    if (g_list_previous(list)) {
        temp = list->data;
        list->data = list->prev->data;
        list->prev->data = temp;
    }
}

/* GTK3: GtkObject removed; use GObject and g_object_steal_qdata */
static void util_menu_delete_popup_data(GObject *object, gpointer ifactory)
{
    g_signal_handlers_disconnect_by_func(object, util_menu_delete_popup_data, ifactory);
    g_object_steal_qdata(G_OBJECT(ifactory), quark_popup_data);
}


/*
 * util_item_factory_popup[_with_data]() is a replacement for
 * gtk_item_factory_popup[_with_data]().
 *
 * The difference is that the menu is always popped up within the
 * screen.  This means it does not necessarily pop up at (x,y).
 */

void util_item_factory_popup_with_data(GtkWidget *ifactory, gpointer data, GDestroyNotify destroy,
                                       guint x, guint y, guint mouse_button, guint32 time)
{
    (void)mouse_button;
    (void)time;

    if (!quark_popup_data)
        quark_popup_data = g_quark_from_static_string("GtkItemFactory-popup-data");

    if (data != NULL) {
        g_object_set_qdata_full(G_OBJECT(ifactory), quark_popup_data, data, destroy);
        g_signal_connect(G_OBJECT(ifactory), "selection-done",
                         G_CALLBACK(util_menu_delete_popup_data), ifactory);
    }

    /* GTK3 migration: gtk_menu_popup() + GtkMenuPositionFunc is deprecated in
     * GTK3.22 and broken in practice: with GDK_CURRENT_TIME (no real button
     * event) GTK3 cannot acquire a pointer grab, so the position function
     * result is ignored and the menu appears at (0,0) — top-left of screen.
     *
     * gtk_menu_popup_at_rect() (GTK3.22+) does not need a pointer grab: it
     * positions the menu relative to a GdkWindow + GdkRectangle in screen
     * coordinates and handles screen-edge clamping internally. */
    {
        GdkRectangle rect = { (gint)x, (gint)y, 1, 1 };
        gtk_menu_popup_at_rect(GTK_MENU(ifactory),
                               gdk_screen_get_root_window(gdk_screen_get_default()),
                               &rect,
                               GDK_GRAVITY_NORTH_WEST,
                               GDK_GRAVITY_NORTH_WEST,
                               NULL);
    }
}

void util_item_factory_popup(GtkWidget *ifactory, guint x, guint y, guint mouse_button,
                             guint32 time)
{
    util_item_factory_popup_with_data(ifactory, NULL, NULL, x, y, mouse_button, time);
}

/*
 * util_get_root_pointer() — GTK3 replacement for gdk_window_get_pointer(NULL, ...).
 * In GTK2, passing NULL as the window returned screen-root coordinates.
 * In GTK3, the root-window case is handled via the device API.
 */
void util_get_root_pointer(gint *x, gint *y)
{
    GdkDisplay *display = gdk_display_get_default();
    GdkSeat *seat = gdk_display_get_default_seat(display);
    GdkDevice *dev = gdk_seat_get_pointer(seat);
    gdk_device_get_position(dev, NULL, x, y);
}

static gint util_find_compare_func(gconstpointer a, gconstpointer b)
{
    return strcasecmp(a, b);
}

static void util_add_url_callback(GtkWidget *w, GtkWidget *entry)
{
    gchar *text;
    GList *node;

    text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!g_list_find_custom(cfg.url_history, text, util_find_compare_func)) {
        cfg.url_history = g_list_prepend(cfg.url_history, g_strdup(text));
        while (g_list_length(cfg.url_history) > 30) {
            node = g_list_last(cfg.url_history);
            g_free(node->data);
            cfg.url_history = g_list_remove_link(cfg.url_history, node);
        }
    }
}

GtkWidget *util_create_add_url_window(gchar *caption, GCallback ok_func, GCallback enqueue_func)
{
    /* GTK3: GtkCombo————— removed; use GtkComboBoxText with built-in entry */
    GtkWidget *win, *vbox, *bbox, *ok, *enqueue, *cancel, *combo, *centry;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), caption);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_MOUSE);
    gtk_window_set_default_size(GTK_WINDOW(win), 400, -1);
    gtk_container_set_border_width(GTK_CONTAINER(win), 10);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    combo = gtk_combo_box_text_new_with_entry();
    centry = gtk_bin_get_child(GTK_BIN(combo));
    if (cfg.url_history) {
        GList *node = cfg.url_history;
        while (node) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), (const gchar *)node->data);
            node = g_list_next(node);
        }
    }
    g_signal_connect(G_OBJECT(centry), "activate", G_CALLBACK(util_add_url_callback), centry);
    g_signal_connect(G_OBJECT(centry), "activate", ok_func, centry);
    gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, 0);
    gtk_window_set_focus(GTK_WINDOW(win), centry);
    gtk_entry_set_text(GTK_ENTRY(centry), "");
    gtk_widget_show(combo);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
    gtk_widget_show(bbox);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5); /* GTK3: gtk_button_box_set_spacing removed */

    ok = gtk_button_new_with_label(_("OK"));
    g_signal_connect(G_OBJECT(ok), "clicked", G_CALLBACK(util_add_url_callback), centry);
    g_signal_connect(G_OBJECT(ok), "clicked", ok_func, centry);
    gtk_widget_set_can_default(ok, TRUE);
    gtk_widget_grab_default(ok);
    gtk_box_pack_start(GTK_BOX(bbox), ok, FALSE, FALSE, 0);
    gtk_widget_show(ok);

    if (enqueue_func) {
        /* I18N: "Enqueue" here means "Add to playlist" */
        enqueue = gtk_button_new_with_label(_("Enqueue"));
        g_signal_connect(G_OBJECT(enqueue), "clicked", G_CALLBACK(util_add_url_callback), centry);
        g_signal_connect(G_OBJECT(enqueue), "clicked", enqueue_func, centry);
        gtk_widget_set_can_default(enqueue, TRUE);
        gtk_box_pack_start(GTK_BOX(bbox), enqueue, FALSE, FALSE, 0);
        gtk_widget_show(enqueue);
    }

    cancel = gtk_button_new_with_label(_("Cancel"));
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy),
                             GTK_WIDGET(win));
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
    gtk_widget_show(cancel);

    gtk_widget_show(vbox);
    return win;
}

/* GTK3: GtkFileSelection removed -- reimplemented with GtkFileChooserDialog.
 * The old GtkFileSelection + GtkCList helpers are replaced by a single
 * response callback.  TODO(#gtk3): restore CDDA directory hijack if needed.
 */

static void filebrowser_response_cb(GtkWidget *dialog, gint response, gpointer data)
{
    gboolean play_button = GPOINTER_TO_INT(data);

    if (response == GTK_RESPONSE_ACCEPT) {
        GSList *files, *node;
        gchar *folder;

        if (cfg.filesel_path)
            g_free(cfg.filesel_path);
        folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog));
        cfg.filesel_path = folder;

        files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        if (play_button)
            playlist_clear();
        for (node = files; node; node = g_slist_next(node)) {
            playlist_add((char *)node->data);
            g_free(node->data);
        }
        g_slist_free(files);

        if (play_button)
            playlist_play();
        playlistwin_update_list();
    }
    gtk_widget_destroy(dialog);
}

gboolean util_filebrowser_is_dir(GtkFileChooser *filesel)
{
    gchar *filename = gtk_file_chooser_get_filename(filesel);
    gboolean is_dir = FALSE;

    if (filename) {
        struct stat buf;
        is_dir = (stat(filename, &buf) == 0 && S_ISDIR(buf.st_mode));
        g_free(filename);
    }
    return is_dir;
}

GtkWidget *util_create_filebrowser(gboolean play_button)
{
    GtkWidget *dialog;
    const char *title = play_button ? _("Play files") : _("Load files");
    const char *ok_label = play_button ? _("Play") : _("Add");

    dialog = gtk_file_chooser_dialog_new(title, NULL, GTK_FILE_CHOOSER_ACTION_OPEN, _("Close"),
                                         GTK_RESPONSE_CANCEL, ok_label, GTK_RESPONSE_ACCEPT, NULL);

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

    if (cfg.filesel_path)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), cfg.filesel_path);

    g_signal_connect(dialog, "response", G_CALLBACK(filebrowser_response_cb),
                     GINT_TO_POINTER(play_button));

    gtk_widget_show(dialog);
    return dialog;
}

PangoFontDescription *util_font_load(char *name)
{
    PangoFontDescription *font;

    /* GTK3: use Pango font descriptions instead of GdkFont */
    font = pango_font_description_from_string(name);
    if (!font || pango_font_description_get_size(font) == 0) {
        if (font)
            pango_font_description_free(font);
        g_warning("Failed to parse font: \"%s\", using fallback.", name);
        font = pango_font_description_from_string("Sans 9");
    }
    return font;
}

#ifdef ENABLE_NLS
char *util_menu_translate(const char *path, void *func_data)
{
    char *translation = gettext(path);

    if (!translation || *translation != '/') {
        g_warning("Bad translation for menupath: %s", path);
        translation = (char *)path;
    }

    return translation;
}
#endif

void util_set_cursor(GtkWidget *window)
{
    static GdkCursor *cursor;

    if (!window) {
        if (cursor) {
            g_object_unref(cursor); /* GTK3: gdk_cursor_destroy -> g_object_unref */
            cursor = NULL;
        }
        return;
    }
    if (!cursor)
        cursor = gdk_cursor_new(GDK_LEFT_PTR);

    /* GTK3: window->window -> gtk_widget_get_window() */
    gdk_window_set_cursor(gtk_widget_get_window(window), cursor);
}

void util_dump_menu_rc(void)
{
    char *filename = g_strconcat(g_get_home_dir(), "/.xmms/menurc", NULL);
    /* gtk_item_factory_dump_rc(filename, NULL, FALSE); */ /* GTK3: removed */
    g_free(filename);
}

void util_read_menu_rc(void)
{
    char *filename = g_strconcat(g_get_home_dir(), "/.xmms/menurc", NULL);
    /* gtk_item_factory_parse_rc(filename); */ /* GTK3: removed */
    g_free(filename);
}

void util_dialog_keypress_cb(GtkWidget *w, GdkEventKey *event, gpointer data)
{
    if (event && event->keyval == GDK_KEY_Escape) /* GTK3: GDK_Escape -> GDK_KEY_Escape */
        gtk_widget_destroy(w);
}
