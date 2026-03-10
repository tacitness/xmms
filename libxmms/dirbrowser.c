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

/* GTK3: GtkCTree replaced with GtkTreeView + GtkTreeStore (GTK2 widget removed) */

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <dirent.h>

#include "../xmms/i18n.h"
#include "config.h"

/* clang-format off */
/* XPM */
static const char *folder_xpm[] = {"16 16 16 1",       " 	c None",       ".	c #f4f7e4",
                                   "X	c #dee4b5",    "o	c #e1e7b9",    "O	c #c6cba4",
                                   "+	c #dce2b8",    "@	c #e9e9ec",    "#	c #d3d8ae",
                                   "$	c #d8daca",    "%	c #b2b2b5",    "&	c #767862",
                                   "*	c #e3e6c3",    "=	c #1b1b1a",    "-	c #939684",
                                   ";	c #555555",    ":	c #000000",    "                ",
                                   "                ", "  ::::          ", " :.@@O:         ",
                                   ":-&&&&&:::::    ", ":.@@@@@*$O#O=   ", ":@*+XXXX+##O:   ",
                                   ":.*#oooXXXXX:   ", ":@+XoXXXXXX#:   ", ":@*ooXXXXXX#:   ",
                                   ":@**XXXXXXX#:   ", ":@*XXXXXXXX%:   ", ":$.*OOOOOO%-:   ",
                                   " ;:::::::::::   ", "                ", "                "};

/* Icon by Jakub Steiner <jimmac@ximian.com> */

/* XPM */
static const char *ofolder_xpm[] = {"16 16 16 1",       " 	c None",        ".	c #a9ad93",
                                    "X	c #60634d",     "o	c #dee4b5",     "O	c #9ca085",
                                    "+	c #0c0d04",     "@	c #2f2f31",     "#	c #3b3d2c",
                                    "$	c #c8cda2",     "%	c #e6e6e9",     "&	c #b3b5a5",
                                    "*	c #80826d",     "=	c #292a1c",     "-	c #fefef6",
                                    ";	c #8f937b",     ":	c #000000",     "                ",
                                    "                ", "  ::::          ", " :-%%&:         ",
                                    ":-;;;OX:::::    ", ":-;;;;O;O;&.:   ", ":-*X##@@@@@=#:  ",
                                    ":%*+-%%ooooooO: ", ":%X;%ooooooo.*: ", ":.+-%oooooooO:  ",
                                    ":*O-oooooooo*:  ", ":O-oooooooo.:   ", ":*-%$$$$$$OX:   ",
                                    " :::::::::::    ", "                ", "                "};
/* clang-format on */

/* GTK3: GdkPixmap/GdkBitmap replaced with GdkPixbuf */
static GdkPixbuf *folder_pixbuf = NULL;
static GdkPixbuf *ofolder_pixbuf = NULL;

/* GtkTreeStore column indices */
enum {
    COL_PIXBUF,  /* GdkPixbuf * — folder icon */
    COL_NAME,    /* gchar *     — display name (basename) */
    COL_PATH,    /* gchar *     — full path with trailing '/' */
    COL_SCANNED, /* gboolean    — TRUE once children have been populated */
    N_COLS
};

/* Key used to attach the dir-change handler to the GtkTreeView */
#define HANDLER_KEY "xmms-dirbrowser-handler"

/* Sentinel path stored in dummy placeholder children */
#define PLACEHOLDER_PATH ""

static gboolean check_for_subdir(const char *path)
{
    DIR *dir;
    struct dirent *dirent;
    struct stat statbuf;
    char *npath;

    if ((dir = opendir(path)) != NULL) {
        while ((dirent = readdir(dir)) != NULL) {
            if (dirent->d_name[0] == '.')
                continue;

            npath = g_strconcat(path, dirent->d_name, NULL);
            if (stat(npath, &statbuf) != -1 && S_ISDIR(statbuf.st_mode)) {
                g_free(npath);
                closedir(dir);
                return TRUE;
            }
            g_free(npath);
        }
        closedir(dir);
    }
    return FALSE;
}

static void add_dir(GtkTreeStore *store, GtkTreeIter *parent_iter, const char *parent,
                    const char *dir)
{
    struct stat statbuf;
    char *path;

    /* Don't show hidden dirs, nor . and .. */
    if (dir[0] == '.')
        return;

    path = g_strconcat(parent, dir, NULL);
    if (stat(path, &statbuf) != -1 && S_ISDIR(statbuf.st_mode)) {
        gboolean has_subdir;
        char *full_path;
        GtkTreeIter node_iter;

        full_path = g_strconcat(path, "/", NULL);
        has_subdir = check_for_subdir(full_path);

        gtk_tree_store_append(store, &node_iter, parent_iter);
        gtk_tree_store_set(store, &node_iter, COL_PIXBUF, folder_pixbuf, COL_NAME, dir, COL_PATH,
                           full_path, COL_SCANNED, FALSE, -1);

        if (has_subdir) {
            /* Add a dummy child so the expander arrow is shown */
            GtkTreeIter dummy;
            gtk_tree_store_append(store, &dummy, &node_iter);
            gtk_tree_store_set(store, &dummy, COL_PIXBUF, NULL, COL_NAME, "", COL_PATH,
                               PLACEHOLDER_PATH, COL_SCANNED, FALSE, -1);
        }

        g_free(full_path);
    }
    g_free(path);
}

/*
 * GTK3: "row-expanded" signal replaces GtkCTree "tree_expand".
 * Signature: void cb(GtkTreeView *, GtkTreeIter *, GtkTreePath *, gpointer)
 */
static void expand_cb(GtkTreeView *view, GtkTreeIter *parent_iter, GtkTreePath *path, gpointer data)
{
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeStore *store = GTK_TREE_STORE(model);
    gboolean scanned = FALSE;
    gchar *dir_path = NULL;
    GtkTreeIter child_iter;

    (void)path;
    (void)data;

    gtk_tree_model_get(model, parent_iter, COL_PATH, &dir_path, COL_SCANNED, &scanned, -1);

    if (!scanned && dir_path) {
        DIR *dir;
        GList *names = NULL, *l;

        /* Remove the placeholder child */
        if (gtk_tree_model_iter_children(model, &child_iter, parent_iter))
            gtk_tree_store_remove(store, &child_iter);

        /* Collect subdirectory names for sorted insertion */
        if ((dir = opendir(dir_path)) != NULL) {
            struct dirent *dirent;
            while ((dirent = readdir(dir)) != NULL) {
                if (dirent->d_name[0] != '.')
                    names = g_list_prepend(names, g_strdup(dirent->d_name));
            }
            closedir(dir);
        }

        names = g_list_sort(names, (GCompareFunc)g_strcmp0);

        for (l = names; l != NULL; l = l->next)
            add_dir(store, parent_iter, dir_path, l->data);

        g_list_free_full(names, g_free);

        gtk_tree_store_set(store, parent_iter, COL_SCANNED, TRUE, -1);
    }

    /* GTK3: update icon to open-folder when expanded */
    gtk_tree_store_set(store, parent_iter, COL_PIXBUF, ofolder_pixbuf, -1);

    g_free(dir_path);
}

/*
 * GTK3: "row-collapsed" — restore closed-folder icon.
 */
static void collapse_cb(GtkTreeView *view, GtkTreeIter *iter, GtkTreePath *path, gpointer data)
{
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeStore *store = GTK_TREE_STORE(model);

    (void)path;
    (void)data;

    gtk_tree_store_set(store, iter, COL_PIXBUF, folder_pixbuf, -1);
}

/*
 * GTK3: "row-activated" replaces "select_row" double-click detection.
 * Fires on double-click or Enter.
 */
static void row_activated_cb(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col,
                             gpointer data)
{
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeIter iter;
    gchar *dir_path = NULL;
    void (*handler)(char *);

    (void)col;
    (void)data;

    if (!gtk_tree_model_get_iter(model, &iter, path))
        return;

    gtk_tree_model_get(model, &iter, COL_PATH, &dir_path, -1);
    handler = g_object_get_data(G_OBJECT(view), HANDLER_KEY);
    if (handler && dir_path && dir_path[0] != '\0')
        handler(dir_path);

    g_free(dir_path);
}

/*
 * GTK3: on show, scroll the preselected row into view.
 */
static void show_cb(GtkWidget *widget, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(data);
    GtkTreePath *sel_path = g_object_get_data(G_OBJECT(view), "selected_path");

    (void)widget;

    if (sel_path)
        gtk_tree_view_scroll_to_cell(view, sel_path, NULL, TRUE, 0.6, 0.0);
}

static void ok_clicked(GtkWidget *widget, GtkWidget *tree_widget)
{
    GtkTreeView *view = GTK_TREE_VIEW(tree_widget);
    GtkTreeSelection *sel = gtk_tree_view_get_selection(view);
    GtkTreeModel *model = NULL;
    GList *rows, *l;
    void (*handler)(char *);
    GtkWidget *window;

    window = g_object_get_data(G_OBJECT(widget), "window");
    gtk_widget_hide(window);

    handler = g_object_get_data(G_OBJECT(view), HANDLER_KEY);
    rows = gtk_tree_selection_get_selected_rows(sel, &model);

    for (l = rows; l != NULL; l = l->next) {
        GtkTreeIter iter;
        gchar *dir_path = NULL;

        if (!gtk_tree_model_get_iter(model, &iter, l->data))
            continue;

        gtk_tree_model_get(model, &iter, COL_PATH, &dir_path, -1);
        if (handler && dir_path && dir_path[0] != '\0')
            handler(dir_path);
        g_free(dir_path);
    }

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
    gtk_widget_destroy(window);
}

GtkWidget *xmms_create_dir_browser(char *title, char *current_path, GtkSelectionMode mode,
                                   void (*handler)(char *))
{
    GtkWidget *window, *scroll_win, *tree, *vbox, *bbox, *ok, *cancel, *sep;
    GtkTreeStore *store;
    GtkTreeView *view;
    GtkTreeViewColumn *col;
    GtkCellRenderer *pixbuf_renderer, *text_renderer;
    GtkTreeSelection *selection;
    GtkTreeIter root_iter, dummy_iter;
    GtkTreeIter selected_iter;
    gboolean have_selected = FALSE;

    /* GTK3: GTK_WINDOW_DIALOG removed — use GTK_WINDOW_TOPLEVEL */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 400);
    gtk_window_set_title(GTK_WINDOW(window), title);
    /* GTK3: gtk_container_border_width() -> gtk_container_set_border_width() */
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    /* GTK3: gtk_vbox_new() -> gtk_box_new(GTK_ORIENTATION_VERTICAL, ...) */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_win, TRUE, TRUE, 0);
    gtk_widget_show(scroll_win);

    /* GTK3: GdkPixmap -> GdkPixbuf. No window handle required. */
    if (!folder_pixbuf) {
        folder_pixbuf = gdk_pixbuf_new_from_xpm_data(folder_xpm);
        ofolder_pixbuf = gdk_pixbuf_new_from_xpm_data(ofolder_xpm);
    }

    /* Build the tree model */
    store = gtk_tree_store_new(N_COLS, GDK_TYPE_PIXBUF, /* COL_PIXBUF  */
                               G_TYPE_STRING,           /* COL_NAME    */
                               G_TYPE_STRING,           /* COL_PATH    */
                               G_TYPE_BOOLEAN);         /* COL_SCANNED */

    /* Root node "/" */
    gtk_tree_store_append(store, &root_iter, NULL);
    gtk_tree_store_set(store, &root_iter, COL_PIXBUF, folder_pixbuf, COL_NAME, "/", COL_PATH, "/",
                       COL_SCANNED, FALSE, -1);

    /* Placeholder child so root shows an expander arrow */
    gtk_tree_store_append(store, &dummy_iter, &root_iter);
    gtk_tree_store_set(store, &dummy_iter, COL_PIXBUF, NULL, COL_NAME, "", COL_PATH,
                       PLACEHOLDER_PATH, COL_SCANNED, FALSE, -1);

    /* Build the GtkTreeView */
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store); /* view holds the reference */
    view = GTK_TREE_VIEW(tree);

    gtk_tree_view_set_headers_visible(view, FALSE);

    /* Combined icon + text column */
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Directory"));

    pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, pixbuf_renderer, FALSE);
    gtk_tree_view_column_add_attribute(col, pixbuf_renderer, "pixbuf", COL_PIXBUF);

    text_renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, text_renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, text_renderer, "text", COL_NAME);

    gtk_tree_view_append_column(view, col);

    /* Selection mode */
    selection = gtk_tree_view_get_selection(view);
    gtk_tree_selection_set_mode(selection, mode);

    /* GTK3: g_signal_connect replaces gtk_signal_connect;
     *       "row-expanded" replaces "tree_expand" */
    g_signal_connect(tree, "row-expanded", G_CALLBACK(expand_cb), NULL);
    g_signal_connect(tree, "row-collapsed", G_CALLBACK(collapse_cb), NULL);

    /* GTK3: "row-activated" replaces the "select_row" double-click path */
    g_signal_connect(tree, "row-activated", G_CALLBACK(row_activated_cb), NULL);

    g_signal_connect(window, "show", G_CALLBACK(show_cb), tree);

    gtk_container_add(GTK_CONTAINER(scroll_win), tree);

    /* Store handler callback on the view widget */
    g_object_set_data(G_OBJECT(tree), HANDLER_KEY, handler);

    /* Expand root immediately (populates first level) */
    {
        GtkTreePath *root_path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &root_iter);
        gtk_tree_view_expand_row(view, root_path, FALSE);
        gtk_tree_path_free(root_path);
    }

    gtk_widget_show(tree);

    sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
    gtk_widget_show(sep);

    /* GTK3: gtk_hbutton_box_new() -> gtk_button_box_new(HORIZONTAL) */
    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);

    ok = gtk_button_new_with_label(_("OK"));
    /* GTK3: GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT) -> gtk_widget_set_can_default() */
    gtk_widget_set_can_default(ok, TRUE);
    gtk_window_set_default(GTK_WINDOW(window), ok);
    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(ok), "window", window);
    g_signal_connect(ok, "clicked", G_CALLBACK(ok_clicked), tree);
    gtk_widget_show(ok);

    cancel = gtk_button_new_with_label(_("Cancel"));
    /* GTK3: GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT) */
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
    /* GTK3: gtk_signal_connect_object -> g_signal_connect_swapped */
    g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_widget_destroy), window);
    gtk_widget_show(cancel);

    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
    gtk_widget_show(bbox);
    gtk_widget_show(vbox);

    /*
     * Navigate to current_path by walking the already-expanded tree.
     * Each component must already be present (root was just expanded).
     */
    if (current_path && *current_path) {
        char **dir;
        int i;
        GtkTreeIter iter = root_iter;
        GtkTreeModel *model = GTK_TREE_MODEL(store);

        dir = g_strsplit(current_path, "/", 0);
        for (i = 0; dir[i] != NULL; i++) {
            GtkTreeIter child_iter;
            gboolean found = FALSE;

            if (dir[i][0] == '\0')
                continue;

            /* GTK3: iterate children instead of GTK_CTREE_ROW(node)->sibling */
            if (!gtk_tree_model_iter_children(model, &child_iter, &iter))
                break;

            do {
                gchar *name = NULL;
                gtk_tree_model_get(model, &child_iter, COL_NAME, &name, -1);
                if (name && strcmp(dir[i], name) == 0) {
                    g_free(name);
                    iter = child_iter;
                    found = TRUE;
                    break;
                }
                g_free(name);
            } while (gtk_tree_model_iter_next(model, &child_iter));

            if (!found)
                break;

            /* GTK3: !GTK_CTREE_ROW(node)->is_leaf -> iter_has_child() */
            if (gtk_tree_model_iter_has_child(model, &iter) && dir[i + 1] != NULL) {
                GtkTreePath *p = gtk_tree_model_get_path(model, &iter);
                gtk_tree_view_expand_row(view, p, FALSE);
                gtk_tree_path_free(p);
            } else {
                selected_iter = iter;
                have_selected = TRUE;
                break;
            }
        }
        g_strfreev(dir);
    }

    if (!have_selected)
        selected_iter = root_iter;

    /* GTK3: gtk_ctree_select() -> gtk_tree_selection_select_iter() */
    gtk_tree_selection_select_iter(selection, &selected_iter);

    /* Store path for show_cb scroll-into-view */
    {
        GtkTreePath *sel_path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &selected_iter);
        g_object_set_data_full(G_OBJECT(tree), "selected_path", sel_path,
                               (GDestroyNotify)gtk_tree_path_free);
    }

    return window;
}
