/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2001  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front
 * Technologies Copyright (C) 1999-2001  Haavard Kvaalen
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
#include "xmms.h"

GtkWidget *skinwin, *skinwin_list, *skinwin_close;
GList *skinlist = NULL;
static gint skinwin_selected_row = 0; /* row index of the currently active skin */

gint skinwin_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_widget_hide(skinwin);
    return (TRUE);
}

void change_skin_event(GtkTreeView *widget, GtkTreePath *path, GtkTreeViewColumn *col,
                       gpointer data)
{
    gint row = gtk_tree_path_get_indices(path)[0];
    if (row == 0)
        load_skin(NULL);
    else
        load_skin(((struct SkinNode *)g_list_nth(skinlist, row - 1)->data)->path);
}

static void enable_random_skin_event(GtkWidget *widget, gpointer data)
{
    cfg.random_skin_on_play = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

void create_skin_window(void)
{
    GtkWidget *vbox, *hbox, *main_hbox, *separator, *scrolled_win, *checkbox;

    skinwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(skinwin), _("Skin selector"));
    gtk_window_set_transient_for(GTK_WINDOW(skinwin), GTK_WINDOW(mainwin));
    g_signal_connect(G_OBJECT(skinwin), "delete_event", G_CALLBACK(skinwin_delete_event), NULL);
    gtk_container_set_border_width(GTK_CONTAINER(skinwin), 10);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(skinwin), vbox);

    {
        /* GTK3: GtkCList replaced by GtkTreeView + GtkListStore (single text column) */
        GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *col;
        GtkTreeSelection *sel;

        skinwin_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
        g_object_unref(store); /* tree view holds the reference now */

        col = gtk_tree_view_column_new_with_attributes(_("Skins"), renderer, "text", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(skinwin_list), col);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(skinwin_list), FALSE);

        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(skinwin_list));
        gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
    }
    g_signal_connect(G_OBJECT(skinwin_list), "row-activated", G_CALLBACK(change_skin_event), NULL);
    gtk_widget_set_size_request(skinwin_list, 250, 200);
    scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_win), skinwin_list);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_win), 5);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_win, TRUE, TRUE, 0);

    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, TRUE, 0);

    main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_spacing(GTK_BOX(main_hbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), main_hbox, FALSE, FALSE, 0);

    checkbox = gtk_check_button_new_with_label(_("Select random skin on play"));
    gtk_box_pack_start(GTK_BOX(main_hbox), checkbox, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), cfg.random_skin_on_play);
    g_signal_connect(G_OBJECT(checkbox), "toggled", G_CALLBACK(enable_random_skin_event), NULL);

    hbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbox), 5);
    gtk_box_pack_start(GTK_BOX(main_hbox), hbox, TRUE, TRUE, 0);
    skinwin_close = gtk_button_new_with_label(_("Close"));
    gtk_widget_set_can_default(skinwin_close, TRUE);
    g_signal_connect(G_OBJECT(skinwin_close), "clicked", G_CALLBACK(skinwin_delete_event), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), skinwin_close, FALSE, FALSE, 0);
    gtk_widget_grab_default(skinwin_close);
}

static void add_skin(gchar *skinpath)
{
    struct SkinNode *node = (struct SkinNode *)g_malloc(sizeof(struct SkinNode));
    gchar *tmp;

    node->path = skinpath;

    tmp = strrchr(skinpath, '/');
    if (!tmp)
        tmp = skinpath;
    node->name = (char *)g_malloc(strlen(tmp + 1) + 1);
    strcpy(node->name, tmp + 1);
    tmp = strrchr(node->name, '.');
    if (tmp) {
        if (!strcasecmp(tmp, ".zip"))
            *tmp = '\0';
        else if (!strcasecmp(tmp, ".wsz"))
            *tmp = '\0';
        else if (!strcasecmp(tmp, ".tgz"))
            *tmp = '\0';
        else if (!strcasecmp(tmp, ".gz"))
            *tmp = '\0';
        else if (!strcasecmp(tmp, ".bz2"))
            *tmp = '\0';
        else if (!strcasecmp(tmp, ".tar"))
            *tmp = '\0';
        tmp = strrchr(node->name, '.');
        if (tmp) { /* maybe we still have to remove a leftover ".tar" */
            if (!strcasecmp(tmp, ".tar"))
                *tmp = '\0';
        }
    }
    skinlist = g_list_prepend(skinlist, node);
}

static void scan_skindir(char *dirpath)
{
    DIR *dir;
    struct dirent *dirent;
    struct stat statbuf;
    char *file, *tmp;

    if ((dir = opendir(dirpath)) != NULL) {
        while ((dirent = readdir(dir)) != NULL) {
            if (strcmp(dirent->d_name, ".") && strcmp(dirent->d_name, "..")) {
                file = g_strdup_printf("%s/%s", dirpath, dirent->d_name);
                if (!stat(file, &statbuf)) {
                    if (S_ISDIR(statbuf.st_mode))
                        add_skin(file);
                    else if (S_ISREG(statbuf.st_mode)) {
                        tmp = strrchr(file, '.');
                        if (tmp) {
                            if (!strcasecmp(tmp, ".zip") || !strcasecmp(tmp, ".wsz") ||
                                !strcasecmp(tmp, ".tgz") || !strcasecmp(tmp, ".gz") ||
                                !strcasecmp(tmp, ".bz2"))
                                add_skin(file);
                            else
                                g_free(file);
                        } else
                            g_free(file);
                    } else
                        g_free(file);
                } else
                    g_free(file);
            }
        }
        closedir(dir);
    }
}

static gint skinlist_compare_func(gconstpointer a, gconstpointer b)
{
    return strcasecmp(((struct SkinNode *)a)->name, ((struct SkinNode *)b)->name);
}

static void skin_free_func(gpointer data, gpointer user_data)
{
    g_free(((struct SkinNode *)data)->name);
    g_free(((struct SkinNode *)data)->path);
    g_free(data);
}

void scan_skins(void)
{
    int i;
    GList *entry;
    char *str, *skinsdir;
    gchar **list;

    if (skinlist) {
        g_list_foreach(skinlist, skin_free_func, NULL);
        g_list_free(skinlist);
    }
    skinlist = NULL;
    str = g_strconcat(g_get_home_dir(), "/.xmms/Skins", NULL);
    scan_skindir(str);
    g_free(str);
    str = g_strconcat(DATA_DIR, "/Skins", NULL);
    scan_skindir(str);
    g_free(str);
    skinlist = g_list_sort(skinlist, skinlist_compare_func);
    skinsdir = getenv("SKINSDIR");
    if (skinsdir) {
        list = g_strsplit(skinsdir, ":", 0);
        i = 0;
        while (list[i])
            scan_skindir(list[i++]);
    }

    {
        /* GTK3: populate GtkListStore — first row is "(none)", then sorted skinlist */
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(skinwin_list)));
        GtkTreeIter iter;

        gtk_list_store_clear(store);
        skinwin_selected_row = 0; /* default: (none) */

        /* Row 0: built-in (no skin) */
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, _("(none)"), -1);

        for (i = 0; i < (gint)g_list_length(skinlist); i++) {
            entry = g_list_nth(skinlist, i);
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter, 0, ((struct SkinNode *)entry->data)->name, -1);
            if (skin->path && !strcmp(((struct SkinNode *)entry->data)->path, skin->path))
                skinwin_selected_row = i + 1; /* +1 to account for the (none) row */
        }
    }
}

void show_skin_window(void)
{
    scan_skins();
    gtk_window_set_position(GTK_WINDOW(skinwin), GTK_WIN_POS_MOUSE);
    gtk_widget_show_all(skinwin);
    gtk_widget_grab_focus(skinwin_list);
    {
        /* GTK3: select the current skin row and scroll it into view */
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(skinwin_list));
        GtkTreePath *path = gtk_tree_path_new_from_indices(skinwin_selected_row, -1);
        GtkTreeIter iter;
        GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(skinwin_list));

        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gtk_tree_selection_select_iter(sel, &iter);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(skinwin_list), path, NULL, FALSE, 0, 0);
        }
        gtk_tree_path_free(path);
    }
}
