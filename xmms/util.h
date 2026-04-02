/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2001  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2991  Haavard Kvaalen
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
#ifndef UTIL_H
#define UTIL_H

#include "i18n.h"

/* System / library headers required by the declarations below */
#include <cairo/cairo.h>
#include <glib.h>
#include <gtk/gtk.h>

gchar *find_file_recursively(const char *dirname, const char *file);
void del_directory(const char *dirname);
/* GTK3: GdkImage removed — use cairo_surface_t for pixel surfaces */
cairo_surface_t *create_dblsize_surface(cairo_surface_t *src);
char *read_ini_string(const char *filename, const char *section, const char *key);
char *read_ini_string_no_comment(const char *filename, const char *section, const char *key);
GArray *read_ini_array(const gchar *filename, const gchar *section, const gchar *key);
GArray *string_to_garray(const gchar *str);
void glist_movedown(GList *list);
void glist_moveup(GList *list);
/* GTK3: popup helpers wrap native GtkMenu positioning */
void util_item_factory_popup(GtkWidget *menu, guint x, guint y, guint mouse_button, guint32 time);
void util_item_factory_popup_with_data(GtkWidget *menu, gpointer data, GDestroyNotify destroy,
                                       guint x, guint y, guint mouse_button, guint32 time);
/* GTK3: gdk_window_get_pointer(NULL) replacement */
void util_get_root_pointer(gint *x, gint *y);
/* GTK3: GtkSignalFunc → GCallback; GtkFileSelection → GtkFileChooser */
GtkWidget *util_create_add_url_window(gchar *caption, GCallback ok_func, GCallback enqueue_func);
GtkWidget *util_create_filebrowser(gboolean clear_pl_on_ok);
gboolean util_filebrowser_is_dir(GtkFileChooser *filesel);
/* GTK3: GdkFont removed — use PangoFontDescription */
PangoFontDescription *util_font_load(gchar *name);
void util_set_cursor(GtkWidget *window);
void util_dump_menu_rc(void);
void util_read_menu_rc(void);
void util_dialog_keypress_cb(GtkWidget *w, GdkEventKey *event, gpointer data);

#if ENABLE_NLS
gchar *util_menu_translate(const gchar *path, gpointer func_data);
#else
#    define util_menu_translate NULL
#endif


#endif
