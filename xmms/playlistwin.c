/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2002  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2006  Haavard Kvaalen
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
#include "libxmms/util.h"
#include "xmms.h"


/* GTK3: wrapper to silence incompatible-pointer-types warning */
static void on_widget_destroyed(GtkWidget *w, gpointer data)
{
    gtk_widget_destroyed(w, (GtkWidget **)data);
}
GtkWidget *playlistwin;
static GtkWidget *playlistwin_url_window = NULL;
static GtkWidget *playlistwin_sort_menu, *playlistwin_sub_menu;
static GtkWidget *playlistwin_popup_menu, *playlistwin_save_menu;
static GtkAccelGroup *playlistwin_accel;

static cairo_surface_t *playlistwin_bg;
static cairo_t *playlistwin_gc;
static int playlistwin_resizing, playlistwin_resize_x, playlistwin_resize_y;
static int playlistwin_save_type;

gboolean playlistwin_focus = FALSE;

PlayList_List *playlistwin_list = NULL;
PButton *playlistwin_shade, *playlistwin_close;
static PlaylistSlider *playlistwin_slider = NULL;
static TextBox *playlistwin_time_min, *playlistwin_time_sec;
static TextBox *playlistwin_info, *playlistwin_sinfo;
static SButton *playlistwin_srew, *playlistwin_splay;
static SButton *playlistwin_spause, *playlistwin_sstop;
static SButton *playlistwin_sfwd, *playlistwin_seject;
static SButton *playlistwin_sscroll_up, *playlistwin_sscroll_down;
Vis *playlistwin_vis;

static GList *playlistwin_wlist = NULL;
static gboolean playlistwin_vis_enabled = FALSE;

static struct {
    int w, h;
} playlistwin_resizeq = {-1, -1};

struct overwrite_data {
    char *filename;
    int pls;
    GtkWidget *fsel;
};

extern TButton *mainwin_pl;

enum {
    ADD_URL,
    ADD_DIR,
    ADD_FILE,
    SUB_MISC,
    SUB_ALL,
    SUB_CROP,
    SUB_SELECTED,
    SEL_INV,
    SEL_ZERO,
    SEL_ALL,
    MISC_SORT,
    MISC_FILEINFO,
    MISC_MISCOPTS,
    MISC_QUEUE,
    PLIST_NEW,
    PLIST_SAVE,
    PLIST_LOAD,
    SEL_LOOKUP,
    MISC_QUEUE_MANAGER
};

static void playlistwin_sort_menu_callback(gpointer cb_data, guint action, GtkWidget *w);
static void playlistwin_sub_menu_callback(gpointer cb_data, guint action, GtkWidget *w);
static void playlistwin_save_type_cb(gpointer cb_data, guint action, GtkWidget *w);
static void playlistwin_set_hints(void);
static void playlistwin_set_shade(gboolean shaded);
static void playlistwin_popup_menu_callback(gpointer cb_data, guint action, GtkWidget *w);
static void playlistwin_draw_frame(void);


enum {
    PLAYLISTWIN_SORT_BYTITLE,
    PLAYLISTWIN_SORT_BYFILENAME,
    PLAYLISTWIN_SORT_BYPATH,
    PLAYLISTWIN_SORT_BYDATE,
    PLAYLISTWIN_SORT_SEL_BYTITLE,
    PLAYLISTWIN_SORT_SEL_BYFILENAME,
    PLAYLISTWIN_SORT_SEL_BYPATH,
    PLAYLISTWIN_SORT_SEL_BYDATE,
    PLAYLISTWIN_SORT_RANDOMIZE,
    PLAYLISTWIN_SORT_REVERSE,
    PLAYLISTWIN_SORT_SEL_RANDOMIZE,
};

#if 0 /* GTK3: GtkItemFactory removed */
GtkItemFactoryEntry playlistwin_sort_menu_entries[] = {
    {N_("/Sort List"), NULL, NULL, 0, "<Branch>"},
    {N_("/Sort List/By Title"), NULL, playlistwin_sort_menu_callback, PLAYLISTWIN_SORT_BYTITLE,
     "<Item>"},
    {N_("/Sort List/By Filename"), NULL, playlistwin_sort_menu_callback,
     PLAYLISTWIN_SORT_BYFILENAME, "<Item>"},
    {N_("/Sort List/By Path + Filename"), NULL, playlistwin_sort_menu_callback,
     PLAYLISTWIN_SORT_BYPATH, "<Item>"},
    {N_("/Sort List/By Date"), NULL, playlistwin_sort_menu_callback, PLAYLISTWIN_SORT_BYDATE,
     "<Item>"},
    {N_("/Selection"), NULL, NULL, 0, "<Branch>"},
    {N_("/Selection/Sort By Title"), NULL, playlistwin_sort_menu_callback,
     PLAYLISTWIN_SORT_SEL_BYTITLE, "<Item>"},
    {N_("/Selection/Sort By Filename"), NULL, playlistwin_sort_menu_callback,
     PLAYLISTWIN_SORT_SEL_BYFILENAME, "<Item>"},
    {N_("/Selection/Sort By Path + Filename"), NULL, playlistwin_sort_menu_callback,
     PLAYLISTWIN_SORT_SEL_BYPATH, "<Item>"},
    {N_("/Selection/Sort By Date"), NULL, playlistwin_sort_menu_callback,
     PLAYLISTWIN_SORT_SEL_BYDATE, "<Item>"},
    {N_("/Selection/Randomize"), NULL, playlistwin_sort_menu_callback,
     PLAYLISTWIN_SORT_SEL_RANDOMIZE, "<Item>"},
    {"/-", NULL, NULL, 0, "<Separator>"},
    {N_("/Randomize List"), NULL, playlistwin_sort_menu_callback, PLAYLISTWIN_SORT_RANDOMIZE,
     "<Item>"},
    {N_("/Reverse List"), NULL, playlistwin_sort_menu_callback, PLAYLISTWIN_SORT_REVERSE, "<Item>"},
};
#endif

static const int playlistwin_sort_menu_entries_num = 0; /* GTK3: entries disabled */

enum { PLAYLISTWIN_SAVE_EXTENSION = 0, PLAYLISTWIN_SAVE_M3U, PLAYLISTWIN_SAVE_PLS };

#if 0 /* GTK3: GtkItemFactory removed */
static GtkItemFactoryEntry playlistwin_playlist_filetypes[] = {
    {N_("/By extension"), NULL, playlistwin_save_type_cb, PLAYLISTWIN_SAVE_EXTENSION, NULL},
    {"/-", NULL, NULL, 0, "<Separator>"},
    {"/m3u", NULL, playlistwin_save_type_cb, PLAYLISTWIN_SAVE_M3U, NULL},
    {"/pls", NULL, playlistwin_save_type_cb, PLAYLISTWIN_SAVE_PLS, NULL},
};
#endif

static const int playlistwin_playlist_filetypes_num = 0; /* GTK3: entries disabled */

enum { PLAYLISTWIN_REMOVE_DEAD_FILES, PLAYLISTWIN_PHYSICALLY_DELETE };

#if 0 /* GTK3: GtkItemFactory removed */
GtkItemFactoryEntry playlistwin_sub_menu_entries[] = {
    {N_("/Remove Dead Files"), NULL, playlistwin_sub_menu_callback, PLAYLISTWIN_REMOVE_DEAD_FILES,
     "<Item>"},
    {N_("/Physically Delete Files"), NULL, playlistwin_sub_menu_callback,
     PLAYLISTWIN_PHYSICALLY_DELETE, "<Item>"},
};
#endif

static const int playlistwin_sub_menu_entries_num = 0; /* GTK3: entries disabled */

#if 0                                                  /* GTK3: GtkItemFactory removed */
GtkItemFactoryEntry playlistwin_popup_menu_entries[] = {
    {N_("/View File Info"), "<control>3", playlistwin_popup_menu_callback, MISC_FILEINFO, "<Item>"},
    {N_("/Queue - Unqueue"), "Q", playlistwin_popup_menu_callback, MISC_QUEUE, "<Item>"},
    {N_("/Queue manager"), "<alt>Q", playlistwin_popup_menu_callback, MISC_QUEUE_MANAGER, "<Item>"},
    {N_("/-"), NULL, NULL, 0, "<Separator>"},
    {N_("/Add"), NULL, NULL, 0, "<Branch>"},
    {N_("/Add/File"), NULL, playlistwin_popup_menu_callback, ADD_FILE, "<Item>"},
    {N_("/Add/Directory"), NULL, playlistwin_popup_menu_callback, ADD_DIR, "<Item>"},
    {N_("/Add/Url"), NULL, playlistwin_popup_menu_callback, ADD_URL, "<Item>"},

    {N_("/Remove"), NULL, NULL, 0, "<Branch>"},
    {N_("/Remove/Selected"), NULL, playlistwin_popup_menu_callback, SUB_SELECTED, "<Item>"},
    {N_("/Remove/Crop"), NULL, playlistwin_popup_menu_callback, SUB_CROP, "<Item>"},
    {N_("/Remove/All"), NULL, playlistwin_popup_menu_callback, SUB_ALL, "<Item>"},
    {N_("/Remove/Misc"), NULL, NULL, 0, "<Item>"},

    {N_("/Selection"), NULL, NULL, 0, "<Branch>"},
    {N_("/Selection/Select All"), NULL, playlistwin_popup_menu_callback, SEL_ALL, "<Item>"},
    {N_("/Selection/Select None"), NULL, playlistwin_popup_menu_callback, SEL_ZERO, "<Item>"},
    {N_("/Selection/Invert Selection"), NULL, playlistwin_popup_menu_callback, SEL_INV, "<Item>"},
    {N_("/Selection/-"), NULL, NULL, 0, "<Separator>"},
    {N_("/Selection/Read Extended Info"), NULL, playlistwin_popup_menu_callback, SEL_LOOKUP,
     "<Item>"},

    {N_("/Sort"), NULL, NULL, 0, "<Item>"},

    {N_("/Playlist"), NULL, NULL, 0, "<Branch>"},
    {N_("/Playlist/Load List"), NULL, playlistwin_popup_menu_callback, PLIST_LOAD, "<Item>"},
    {N_("/Playlist/Save List"), NULL, playlistwin_popup_menu_callback, PLIST_SAVE, "<Item>"},
    {N_("/Playlist/New List"), NULL, playlistwin_popup_menu_callback, PLIST_NEW, "<Item>"},
};
#endif

static const int playlistwin_popup_menu_entries_num = 0; /* GTK3: entries disabled */

static void playlistwin_update_info(void)
{
    char *text, *sel_text, *tot_text;
    gulong selection, total;
    gboolean selection_more, total_more;

    playlist_get_total_time(&total, &selection, &total_more, &selection_more);

    if (selection > 0 || (selection == 0 && !selection_more)) {
        if (selection > 3600)
            sel_text =
                g_strdup_printf("%lu:%-2.2lu:%-2.2lu%s", selection / 3600, (selection / 60) % 60,
                                selection % 60, (selection_more ? "+" : ""));
        else
            sel_text = g_strdup_printf("%lu:%-2.2lu%s", selection / 60, selection % 60,
                                       (selection_more ? "+" : ""));
    } else
        sel_text = g_strdup("?");
    if (total > 0 || (total == 0 && !total_more)) {
        if (total > 3600)
            tot_text = g_strdup_printf("%lu:%-2.2lu:%-2.2lu%s", total / 3600, (total / 60) % 60,
                                       total % 60, total_more ? "+" : "");
        else
            tot_text =
                g_strdup_printf("%lu:%-2.2lu%s", total / 60, total % 60, total_more ? "+" : "");
    } else
        tot_text = g_strdup("?");
    text = g_strconcat(sel_text, "/", tot_text, NULL);
    textbox_set_text(playlistwin_info, text);
    g_free(text);
    g_free(tot_text);
    g_free(sel_text);
}

static void playlistwin_update_sinfo(void)
{
    char *posstr, *timestr, *title, *info, *dots;
    int pos, time, max_len;

    pos = get_playlist_position();
    title = playlist_get_songtitle(pos);
    time = playlist_get_songtime(pos);

    if (!title) {
        textbox_set_text(playlistwin_sinfo, "");
        return;
    }

    if (cfg.show_numbers_in_pl)
        posstr = g_strdup_printf("%d. ", pos + 1);
    else
        posstr = g_strdup("");

    max_len = (cfg.playlist_width - 35) / 5 - strlen(posstr);

    if (time != -1) {
        timestr = g_strdup_printf(" %d:%-2.2d", time / 60000, (time / 1000) % 60);
        max_len -= strlen(timestr);
    } else
        timestr = g_strdup("");

    if (cfg.convert_underscore) {
        char *tmp;
        while ((tmp = strchr(title, '_')) != NULL)
            *tmp = ' ';
    }
    if (cfg.convert_twenty) {
        char *tmp, *tmp2;
        while ((tmp = strstr(title, "%20")) != NULL) {
            tmp2 = tmp + 3;
            *(tmp++) = ' ';
            while (*tmp2)
                *(tmp++) = *(tmp2++);
            *tmp = '\0';
        }
    }

    if (strlen(title) > max_len) {
        max_len -= 3;
        dots = "...";
    } else
        dots = "";

    info = g_strdup_printf("%s%-*.*s%s%s", posstr, max_len, max_len, title, dots, timestr);
    g_free(posstr);
    g_free(title);
    g_free(timestr);

    textbox_set_text(playlistwin_sinfo, info);
    g_free(info);
}

gboolean playlistwin_item_visible(int index)
{
    if (index >= playlistwin_list->pl_first &&
        index < (playlistwin_list->pl_first + playlistwin_list->pl_num_visible))
        return TRUE;
    return FALSE;
}

int playlistwin_get_toprow(void)
{
    if (playlistwin_list)
        return (playlistwin_list->pl_first);
    return (-1);
}

void playlistwin_set_toprow(int toprow)
{
    if (playlistwin_list)
        playlistwin_list->pl_first = toprow;
    playlistwin_update_list();
}

void playlistwin_update_list(void)
{
    g_return_if_fail(playlistwin_list != NULL);

    draw_widget(playlistwin_list);
    draw_widget(playlistwin_slider);
    playlistwin_update_info();
    playlistwin_update_sinfo();
}

static void playlistwin_create_mask(void)
{
    /* GTK3: shape mask via cairo_region_t + gtk_widget_shape_combine_region */
    cairo_region_t *region;

    if (cfg.show_wm_decorations)
        return;

    region = cairo_region_create_rectangle(
        &(cairo_rectangle_int_t){0, 0, cfg.playlist_width, PLAYLIST_HEIGHT});
    gtk_widget_shape_combine_region(playlistwin, region);
    cairo_region_destroy(region);
}

void playlistwin_set_shade_menu_cb(gboolean shaded)
{
    cfg.playlist_shaded = shaded;
    if (!cfg.show_wm_decorations)
        dock_shade(dock_window_list, playlistwin, PLAYLIST_HEIGHT);
    else
        gtk_window_set_default_size(GTK_WINDOW(playlistwin), cfg.playlist_width, PLAYLIST_HEIGHT);

    if (shaded) {
        show_widget(playlistwin_sinfo);
        playlistwin_shade->pb_nx = 128;
        playlistwin_shade->pb_ny = 45;
        playlistwin_shade->pb_px = 150;
        playlistwin_shade->pb_py = 42;
        playlistwin_close->pb_nx = 138;
        playlistwin_close->pb_ny = 45;
    } else {
        hide_widget(playlistwin_sinfo);
        playlistwin_shade->pb_nx = 157;
        playlistwin_shade->pb_ny = 3;
        playlistwin_shade->pb_px = 62;
        playlistwin_shade->pb_py = 42;
        playlistwin_close->pb_nx = 167;
        playlistwin_close->pb_ny = 3;
    }

    playlistwin_create_mask();
    playlistwin_set_hints();
    draw_playlist_window(TRUE);
}

static void playlistwin_shade_toggle(void)
{
    playlistwin_set_shade(!cfg.playlist_shaded);
}

static void playlistwin_set_shade(gboolean shaded)
{
    /* GTK3: call implementation directly (gtk_item_factory_get_widget removed) */
    playlistwin_set_shade_menu_cb(shaded);
}

void playlistwin_raise(void)
{
    if (cfg.playlist_visible)
        gdk_window_raise(gtk_widget_get_window(playlistwin));
}

static void playlistwin_release(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
    if (event->button == 3)
        return;

    /* GTK3: ungrab pointer on button release */
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    /* gdk_flush() no-op in GTK3 */
    if (playlistwin_resizing)
        playlistwin_resizing = FALSE;
    else if (dock_is_moving(playlistwin))
        dock_move_release(playlistwin);
    else {
        handle_release_cb(playlistwin_wlist, widget, event);
        playlist_popup_destroy();
        draw_playlist_window(FALSE);
    }
}

void playlistwin_scroll(int num)
{
    playlistwin_list->pl_first += num;
    playlistwin_update_list();
}

static void playlistwin_scroll_up_pushed(void)
{
    playlistwin_list->pl_first -= 3;
    playlistwin_update_list();
}

static void playlistwin_scroll_down_pushed(void)
{
    playlistwin_list->pl_first += 3;
    playlistwin_update_list();
}

static void playlistwin_select_all(void)
{
    playlist_select_all(TRUE);
    playlistwin_list->pl_prev_selected = 0;
    playlistwin_list->pl_prev_min = 0;
    playlistwin_list->pl_prev_max = get_playlist_length() - 1;
    playlistwin_update_list();
}

static void playlistwin_select_none(void)
{
    playlist_select_all(FALSE);
    playlistwin_list->pl_prev_selected = -1;
    playlistwin_list->pl_prev_min = -1;
    playlistwin_update_list();
}

static void playlistwin_inverse_selection(void)
{
    playlist_select_invert_all();
    playlistwin_list->pl_prev_selected = -1;
    playlistwin_list->pl_prev_min = -1;
    playlistwin_update_list();
}

static void playlistwin_resize(int width, int height)
{
    int bx, by, nw, nh;
    cairo_surface_t *oldbg;
    gboolean dummy;

    bx = (width - 275) / 25;
    nw = (bx * 25) + 275;
    if (nw < 275)
        nw = 275;

    if (!cfg.playlist_shaded) {
        by = (height - 58) / 29;
        nh = (by * 29) + 58;
        if (nh < 116)
            nh = 116;
    } else
        nh = cfg.playlist_height;

    if (nw == cfg.playlist_width && nh == cfg.playlist_height)
        return;

    cfg.playlist_width = nw;
    cfg.playlist_height = nh;

    resize_widget(playlistwin_list, cfg.playlist_width - 31, cfg.playlist_height - 58);
    move_widget(playlistwin_slider, cfg.playlist_width - 15, 20);
    resize_widget(playlistwin_sinfo, cfg.playlist_width - 35, 14);
    playlistwin_update_sinfo();
    move_widget(playlistwin_shade, cfg.playlist_width - 21, 3);
    move_widget(playlistwin_close, cfg.playlist_width - 11, 3);
    move_widget(playlistwin_time_min, cfg.playlist_width - 82, cfg.playlist_height - 15);
    move_widget(playlistwin_time_sec, cfg.playlist_width - 64, cfg.playlist_height - 15);
    move_widget(playlistwin_info, cfg.playlist_width - 143, cfg.playlist_height - 28);
    move_widget(playlistwin_srew, cfg.playlist_width - 144, cfg.playlist_height - 16);
    move_widget(playlistwin_splay, cfg.playlist_width - 138, cfg.playlist_height - 16);
    move_widget(playlistwin_spause, cfg.playlist_width - 128, cfg.playlist_height - 16);
    move_widget(playlistwin_sstop, cfg.playlist_width - 118, cfg.playlist_height - 16);
    move_widget(playlistwin_sfwd, cfg.playlist_width - 109, cfg.playlist_height - 16);
    move_widget(playlistwin_seject, cfg.playlist_width - 100, cfg.playlist_height - 16);
    move_widget(playlistwin_sscroll_up, cfg.playlist_width - 14, cfg.playlist_height - 35);
    move_widget(playlistwin_sscroll_down, cfg.playlist_width - 14, cfg.playlist_height - 30);
    resize_widget(playlistwin_slider, 8, cfg.playlist_height - 58);
    if (cfg.playlist_width >= 350) {
        move_widget(playlistwin_vis, cfg.playlist_width - 223, cfg.playlist_height - 26);
        if (playlistwin_vis_enabled)
            show_widget(playlistwin_vis);
    } else
        hide_widget(playlistwin_vis);

    oldbg = playlistwin_bg;
    playlistwin_bg =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, cfg.playlist_width, cfg.playlist_height);
    /* GTK3 fix: recreate cairo context for new surface; old context still pointed to oldbg
     * causing all subsequent draws to go to the old (about-to-be-destroyed) surface → black */
    cairo_destroy(playlistwin_gc);
    playlistwin_gc = cairo_create(playlistwin_bg);
    widget_list_change_surface(playlistwin_wlist, playlistwin_bg);
    widget_list_change_cr(playlistwin_wlist, playlistwin_gc);
    playlistwin_create_mask();

    playlistwin_draw_frame();
    draw_widget_list(playlistwin_wlist, &dummy, TRUE);
    clear_widget_list_redraw(playlistwin_wlist);
    gtk_widget_queue_draw(playlistwin);
    cairo_surface_destroy(oldbg);
}

static gboolean playlistwin_resize_handler(gpointer data)
{
    GDK_THREADS_ENTER();
    playlistwin_resize(playlistwin_resizeq.w, playlistwin_resizeq.h);

    playlistwin_resizeq.w = -1;
    playlistwin_resizeq.h = -1;

    GDK_THREADS_LEAVE();
    return FALSE;
}

static void playlistwin_queue_resize(int width, int height)
{
    if (playlistwin_resizeq.w == -1)
        g_idle_add_full(G_PRIORITY_HIGH_IDLE, playlistwin_resize_handler, NULL, NULL);

    playlistwin_resizeq.w = width;
    playlistwin_resizeq.h = height;
}

static void playlistwin_motion(GtkWidget *widget, GdkEventMotion *event, gpointer callback_data)
{
    XEvent ev;

    if (playlistwin_resizing) {
        playlistwin_resize(event->x + playlistwin_resize_x, event->y + playlistwin_resize_y);
        /* GTK3: gdk_window_set_hints removed; resize enforced directly below */
        gdk_window_resize(gtk_widget_get_window(playlistwin), cfg.playlist_width, PLAYLIST_HEIGHT);
        gtk_widget_set_size_request(playlistwin, cfg.playlist_width, PLAYLIST_HEIGHT);
    } else if (dock_is_moving(playlistwin)) {
        dock_move_motion(playlistwin, event);
    } else {
        handle_motion_cb(playlistwin_wlist, widget, event);
        draw_playlist_window(FALSE);
    }
    /* gdk_flush() no-op in GTK3 */
    while (XCheckMaskEvent(gdk_x11_get_default_xdisplay(), ButtonMotionMask, &ev))
        ;
}

static void playlistwin_show_filebrowser(void)
{
    static GtkWidget *filebrowser;
    if (filebrowser != NULL)
        return;

    filebrowser = util_create_filebrowser(FALSE);
    g_signal_connect(G_OBJECT(filebrowser), "destroy", G_CALLBACK(on_widget_destroyed),
                     &filebrowser);
}


static void playlistwin_url_ok_clicked(GtkWidget *w, GtkWidget *entry)
{
    char *text, *temp;

    text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (text && *text) {
        g_strstrip(text);
        if (strstr(text, ":/") == NULL && text[0] != '/')
            temp = g_strconcat("http://", text, NULL);
        else
            temp = g_strdup(text);
        playlist_add_url_string(temp);
        g_free(temp);
    }
    gtk_widget_destroy(playlistwin_url_window);
}

void playlistwin_show_add_url_window(void)
{
    if (!playlistwin_url_window) {
        playlistwin_url_window =
            util_create_add_url_window(_("Enter URL to add:"),
                                       G_CALLBACK(playlistwin_url_ok_clicked), NULL);
        gtk_window_set_transient_for(GTK_WINDOW(playlistwin_url_window), GTK_WINDOW(playlistwin));
        g_signal_connect(G_OBJECT(playlistwin_url_window), "destroy",
                         G_CALLBACK(on_widget_destroyed), &playlistwin_url_window);
        gtk_widget_show(playlistwin_url_window);
    }
}

static void playlistwin_add_dir_handler(char *dir)
{
    g_free(cfg.filesel_path);
    cfg.filesel_path = g_strdup(dir);
    playlist_add_dir(dir);
}

static void playlistwin_show_dirbrowser(void)
{
    GtkWidget *dialog;
    gint result;

    /* GTK3: replaced bare libxmms dirbrowser with GtkFileChooserDialog so the
     * standard GTK folder picker appears with bookmarks/favourites sidebar
     * (fixes issue #20). */
    dialog = gtk_file_chooser_dialog_new(_("Select directory to add"), GTK_WINDOW(playlistwin),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _("_Cancel"),
                                         GTK_RESPONSE_CANCEL, _("_Add"), GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    if (cfg.filesel_path)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), cfg.filesel_path);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        GSList *folders = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        GSList *it;
        for (it = folders; it != NULL; it = g_slist_next(it)) {
            playlistwin_add_dir_handler((char *)it->data);
            g_free(it->data);
        }
        g_slist_free(folders);
    }
    gtk_widget_destroy(dialog);
}

static void playlistwin_fileinfo(void)
{
    /*
     * Show the first selected file, or the
     * current file if nothing is selected
     */
    GList *list = playlist_get_selected();
    if (list != NULL) {
        playlist_fileinfo(GPOINTER_TO_INT(list->data));
        g_list_free(list);
    } else
        playlist_fileinfo_current();
}

static void playlistwin_set_sensitive_popupmenu(void)
{
    GtkWidget *w;
    gboolean set;

    set = playlist_get_num_selected() > 0;

    w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(w, set);

    w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(w, set);

    w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(w, set);
}

static void playlistwin_set_sensitive_sortmenu(void)
{
    GtkWidget *w;
    gboolean set;

    set = playlist_get_num_selected() > 1;

    w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(w, set);
    w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(w, set);
    w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(w, set);
    w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(w, set);
    w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(w, set);
}

static void playlistwin_save_playlist_error(char *path, GtkWidget *filesel)
{
    GtkWidget *dialog, *label, *bbox, *close;
    char *text;

    dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), _("Unable to write playlist!"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(filesel));

    text = g_strdup_printf(_("Error writing playlist \"%s\": %s"), path, strerror(errno));
    label = gtk_label_new(text);
    g_free(text);
    /* TODO(#gtk3): gtk_misc_set_padding deprecated */
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label, TRUE, TRUE,
                       0);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(dialog))), bbox, FALSE, FALSE,
                       0);

    close = gtk_button_new_with_label(_("OK"));
    g_signal_connect_swapped(G_OBJECT(close), "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(dialog));
    gtk_widget_set_can_default(close, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), close, FALSE, FALSE, 0);
    gtk_widget_grab_default(close);

    gtk_widget_show_all(dialog);
}

static void playlistwin_check_overwrite_cb(GtkButton *w, gpointer user_data)
{
    struct overwrite_data *odata = user_data;
    if (!playlist_save(odata->filename, odata->pls))
        playlistwin_save_playlist_error(odata->filename, odata->fsel);
    else {
        gtk_widget_hide(odata->fsel);
        g_free(user_data);
    }
}

static void playlistwin_check_overwrite(GtkWidget *filesel, char *filename, int pls)
{
    GtkWidget *dialog, *label, *bbox, *overwrite, *cancel;
    char *text;
    struct overwrite_data *data;

    data = g_malloc0(sizeof(*data));
    data->filename = filename;
    data->pls = pls;
    data->fsel = filesel;

    dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), _("File exists!"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(filesel));

    text = g_strdup_printf(_("%s already exists."), filename);
    label = gtk_label_new(text);
    /* TODO(#gtk3): gtk_misc_set_padding deprecated */
    g_free(text);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label, TRUE, TRUE,
                       0);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(dialog))), bbox, FALSE, FALSE,
                       0);

    overwrite = gtk_button_new_with_label(_("Overwrite"));
    g_signal_connect(G_OBJECT(overwrite), "clicked", G_CALLBACK(playlistwin_check_overwrite_cb),
                     data);
    g_signal_connect_swapped(G_OBJECT(overwrite), "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(dialog));
    gtk_widget_set_can_default(overwrite, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), overwrite, FALSE, FALSE, 0);
    cancel = gtk_button_new_with_label(_("Cancel"));
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(g_free), (GObject *)data);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(dialog));
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
    gtk_widget_grab_default(overwrite);

    gtk_widget_show_all(dialog);
}

static void playlistwin_save_filesel_ok(
    GtkWidget *w, GtkFileChooser /* TODO(#gtk3): was GtkFileSelection */ *filesel)
{
    char *filename, *slash;
    struct stat statd;
    int len;
    gboolean pls = FALSE;

    if (util_filebrowser_is_dir(filesel))
        return;

    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel)) /* TODO(#gtk3) */;

    if ((slash = strrchr(filename, '/')) != NULL)
        len = slash - filename + 1;
    else
        len = strlen(filename);

    if (playlistwin_save_type == PLAYLISTWIN_SAVE_EXTENSION) {
        char *ext;
        int error = FALSE;
        if ((ext = strrchr(filename, '.')) != NULL && ext > slash) {
            if (!strcmp(ext, ".pls"))
                pls = TRUE;
            else if (strcmp(ext, ".m3u"))
                error = TRUE;
        } else
            error = TRUE;
        if (error) {
            char *m;
            GtkWidget *w;
            m = g_strdup_printf(_("Unknown file type for %s.\n"
                                  "The filename of the playlist should end "
                                  "in either \".m3u\" or \".pls\"."),
                                filename);
            w = xmms_show_message(_("Unable to save playlist"), m, _("OK"), TRUE, NULL, NULL);
            gtk_window_set_transient_for(GTK_WINDOW(w), GTK_WINDOW(filesel));

            g_free(m);
            return;
        }
    } else if (playlistwin_save_type == PLAYLISTWIN_SAVE_PLS)
        pls = TRUE;

    g_free(cfg.playlist_path);
    cfg.playlist_path = g_strndup(filename, len);

    if (stat(filename, &statd) == 0)
        playlistwin_check_overwrite(GTK_WIDGET(filesel), filename, pls);
    else {
        if (!playlist_save(filename, pls))
            playlistwin_save_playlist_error(filename, GTK_WIDGET(filesel));
        else
            gtk_widget_hide(GTK_WIDGET(filesel));
    }
}

static void playlistwin_load_filesel_ok(GtkWidget *w, GtkWidget *filesel)
{
    char *filename, *text, *tmp;

    if (util_filebrowser_is_dir(GTK_FILE_CHOOSER(filesel)))
        return;

    filename = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel) /* TODO(#gtk3) */));
    text = g_strdup(filename);

    if ((tmp = strrchr(text, '/')) != NULL)
        *(tmp + 1) = '\0';
    g_free(cfg.playlist_path);
    cfg.playlist_path = g_strdup(text);
    g_free(text);

    if (filename && *filename) {
        playlist_clear();
        mainwin_clear_song_info();
        mainwin_set_info_text();

        playlist_load(filename);
    }
    gtk_widget_destroy(GTK_WIDGET(filesel));
}

static void playlistwin_show_sub_misc_menu(void)
{
    int x, y;
    GtkWidget *widget;

    widget = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    gtk_widget_set_sensitive(widget, playlist_get_num_selected() > 0);
    util_get_root_pointer(&x, &y);
    util_item_factory_popup(GTK_WIDGET(playlistwin_sub_menu), x, y, 1, GDK_CURRENT_TIME);
}

static void playlistwin_show_load_filesel(void)
{
    static GtkWidget *load_filesel;
    GObject *object;

    if (load_filesel != NULL)
        return;
    load_filesel = gtk_file_chooser_dialog_new("Select file", NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
                                               NULL) /* TODO(#gtk3) */;

    if (cfg.playlist_path)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(load_filesel),
                                      cfg.playlist_path) /* TODO(#gtk3) */;
    object =
        G_OBJECT(gtk_dialog_get_widget_for_response(GTK_DIALOG(load_filesel), GTK_RESPONSE_ACCEPT));
    g_signal_connect(object, "clicked", G_CALLBACK(playlistwin_load_filesel_ok), load_filesel);
    object =
        G_OBJECT(gtk_dialog_get_widget_for_response(GTK_DIALOG(load_filesel), GTK_RESPONSE_CANCEL));
    g_signal_connect_swapped(object, "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(load_filesel));
    g_signal_connect(G_OBJECT(load_filesel), "destroy", G_CALLBACK(on_widget_destroyed),
                     &load_filesel);
    gtk_widget_show(load_filesel);
}

static void playlistwin_show_save_filesel(void)
{
    static GtkWidget *filesel;
    GtkWidget *frame, *hbox, *label, *menu, *om;
    GObject *object;

    if (filesel != NULL) {
        if (!gtk_widget_is_visible(filesel)) {
            if (cfg.playlist_path)
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(filesel),
                                              cfg.playlist_path) /* TODO(#gtk3) */;
            gtk_widget_show(filesel);
        }
        return;
    }

    filesel = gtk_file_chooser_dialog_new("Select file", NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
                                          NULL) /* TODO(#gtk3) */;
    if (cfg.playlist_path)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(filesel),
                                      cfg.playlist_path) /* TODO(#gtk3) */;
    object = G_OBJECT(gtk_dialog_get_widget_for_response(GTK_DIALOG(filesel), GTK_RESPONSE_ACCEPT));
    g_signal_connect(object, "clicked", G_CALLBACK(playlistwin_save_filesel_ok), filesel);
    object = G_OBJECT(gtk_dialog_get_widget_for_response(GTK_DIALOG(filesel), GTK_RESPONSE_CANCEL));
    g_signal_connect_swapped(object, "clicked", G_CALLBACK(gtk_widget_hide), G_OBJECT(filesel));
    g_signal_connect(G_OBJECT(filesel), "destroy", G_CALLBACK(on_widget_destroyed), &filesel);
    /*
     * I18N: "Save options" here is "options for saving, not "save
     * the options"
     */
    frame = gtk_frame_new(_("Save options"));
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    gtk_container_add(GTK_CONTAINER(frame), hbox);
    label = gtk_label_new(_("Determine file type:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    om = gtk_combo_box_new() /* TODO(#gtk3): was gtk_option_menu_new */;
    gtk_box_pack_start(GTK_BOX(hbox), om, TRUE, TRUE, 0);
    menu = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
    /* TODO(#gtk3): gtk_option_menu_set_menu removed */
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(filesel))), frame, TRUE, TRUE,
                       0);

    gtk_widget_show_all(filesel);
}

static void playlistwin_save_type_cb(gpointer cb_data, guint action, GtkWidget *w)
{
    playlistwin_save_type = action;
}

static void playlistwin_popup_handler(int item)
{
    switch (item) {
    /* Add button */
    case ADD_URL:
        playlistwin_show_add_url_window();
        break;
    case ADD_DIR:
        playlistwin_show_dirbrowser();
        break;
    case ADD_FILE:
        playlistwin_show_filebrowser();
        break;

    /* Sub Button */
    case SUB_MISC:
        playlistwin_show_sub_misc_menu();
        break;
    case SUB_ALL:
        playlist_clear();
        mainwin_clear_song_info();
        mainwin_set_info_text();
        break;
    case SUB_CROP:
        playlist_delete(TRUE);
        break;
    case SUB_SELECTED:
        playlist_delete(FALSE);
        break;

    /* Select button */
    case SEL_INV:
        playlistwin_inverse_selection();
        break;
    case SEL_ZERO:
        playlistwin_select_none();
        break;
    case SEL_ALL:
        playlistwin_select_all();
        break;

    /* Misc button */
    case MISC_SORT: {
        int x, y;
        GtkWidget *f;
        playlistwin_set_sensitive_sortmenu();
        util_get_root_pointer(&x, &y);
        f = GTK_WIDGET(playlistwin_sort_menu);
        util_item_factory_popup(f, x, y, 1, GDK_CURRENT_TIME);
        break;
    }
    case MISC_FILEINFO:
        playlistwin_fileinfo();
        break;
    case MISC_MISCOPTS:
        break;

    /* Playlist button */
    case PLIST_NEW:
        playlist_clear();
        mainwin_clear_song_info();
        mainwin_set_info_text();
        break;
    case PLIST_SAVE:
        playlistwin_show_save_filesel();
        break;
    case PLIST_LOAD:
        playlistwin_show_load_filesel();
        break;
    }
}

static gboolean inside_sensitive_widgets(int x, int y)
{
    return (inside_widget(x, y, playlistwin_list) || inside_widget(x, y, playlistwin_slider) ||
            inside_widget(x, y, playlistwin_close) || inside_widget(x, y, playlistwin_shade) ||
            inside_widget(x, y, playlistwin_time_min) ||
            inside_widget(x, y, playlistwin_time_sec) || inside_widget(x, y, playlistwin_info) ||
            inside_widget(x, y, playlistwin_vis) || inside_widget(x, y, playlistwin_srew) ||
            inside_widget(x, y, playlistwin_splay) || inside_widget(x, y, playlistwin_spause) ||
            inside_widget(x, y, playlistwin_sstop) || inside_widget(x, y, playlistwin_sfwd) ||
            inside_widget(x, y, playlistwin_seject) ||
            inside_widget(x, y, playlistwin_sscroll_up) ||
            inside_widget(x, y, playlistwin_sscroll_down));
}

#define REGION_L(x1, x2, y1, y2)                                                      \
    (event->x >= (x1) && event->x < (x2) && event->y >= cfg.playlist_height - (y1) && \
     event->y < cfg.playlist_height - (y2))
#define REGION_R(x1, x2, y1, y2)                                                      \
    (event->x >= cfg.playlist_width - (x1) && event->x < cfg.playlist_width - (x2) && \
     event->y >= cfg.playlist_height - (y1) && event->y < cfg.playlist_height - (y2))

/* GTK3: scroll-event delivers mouse-wheel input; GDK no longer synthesises button
 * 4/5 press events for scroll wheels so we need this dedicated handler. */
static gboolean playlistwin_scroll_event_cb(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
    if (event->direction == GDK_SCROLL_UP ||
        (event->direction == GDK_SCROLL_SMOOTH && event->delta_y < 0.0))
        playlistwin_scroll(-3);
    else if (event->direction == GDK_SCROLL_DOWN ||
             (event->direction == GDK_SCROLL_SMOOTH && event->delta_y > 0.0))
        playlistwin_scroll(3);
    else
        return FALSE;

    return TRUE;
}

static void playlistwin_press(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
    gboolean grab = TRUE;
    int xpos, ypos;

    dock_get_widget_pos(playlistwin, &xpos, &ypos);
    if (event->button == 1 && !cfg.show_wm_decorations &&
        ((!cfg.playlist_shaded && event->x > cfg.playlist_width - 20 &&
          event->y > cfg.playlist_height - 20) ||
         (cfg.playlist_shaded && event->x >= cfg.playlist_width - 31 &&
          event->x < cfg.playlist_width - 22))) {
        if (hint_move_resize_available()) {
            hint_move_resize(playlistwin, event->x_root, event->y_root, FALSE);
            grab = FALSE;
        } else {
            playlistwin_resizing = TRUE;
            playlistwin_resize_x = cfg.playlist_width - event->x;
            playlistwin_resize_y = cfg.playlist_height - event->y;
            playlistwin_raise();
        }
    } else if (event->button == 1 && REGION_L(12, 37, 29, 11)) {
        int nx[] = {0, 0, 0}, ny[] = {111, 130, 149};
        int sx[] = {23, 23, 23}, sy[] = {111, 130, 149};
        int barx = 48, bary = 111;

        playlist_popup(xpos + 12, ypos + cfg.playlist_height - (3 * 18) - 11, 3, nx, ny, sx, sy,
                       barx, bary, ADD_URL, playlistwin_popup_handler);
        grab = FALSE;
    } else if (event->button == 1 && REGION_L(41, 66, 29, 11)) {
        int nx[] = {54, 54, 54, 54}, ny[] = {168, 111, 130, 149};
        int sx[] = {77, 77, 77, 77}, sy[] = {168, 111, 130, 149};
        int barx = 100, bary = 111;

        playlist_popup(xpos + 41, ypos + cfg.playlist_height - (4 * 18) - 11, 4, nx, ny, sx, sy,
                       barx, bary, SUB_MISC, playlistwin_popup_handler);
        grab = FALSE;
    } else if (event->button == 1 && REGION_L(70, 95, 29, 11)) {
        int nx[] = {104, 104, 104}, ny[] = {111, 130, 149};
        int sx[] = {127, 127, 127}, sy[] = {111, 130, 149};
        int barx = 150, bary = 111;

        playlist_popup(xpos + 70, ypos + cfg.playlist_height - (3 * 18) - 11, 3, nx, ny, sx, sy,
                       barx, bary, SEL_INV, playlistwin_popup_handler);
        grab = FALSE;
    } else if (event->button == 1 && REGION_L(99, 124, 29, 11)) {
        int nx[] = {154, 154, 154}, ny[] = {111, 130, 149};
        int sx[] = {177, 177, 177}, sy[] = {111, 130, 149};
        int barx = 200, bary = 111;

        playlist_popup(xpos + 99, ypos + cfg.playlist_height - (3 * 18) - 11, 3, nx, ny, sx, sy,
                       barx, bary, MISC_SORT, playlistwin_popup_handler);
        grab = FALSE;
    } else if (event->button == 1 && REGION_R(46, 23, 29, 11)) {
        int nx[] = {204, 204, 204}, ny[] = {111, 130, 149};
        int sx[] = {227, 227, 227}, sy[] = {111, 130, 149};
        int barx = 250, bary = 111;

        playlist_popup(xpos + cfg.playlist_width - 46, ypos + cfg.playlist_height - (3 * 18) - 11,
                       3, nx, ny, sx, sy, barx, bary, PLIST_NEW, playlistwin_popup_handler);
        grab = FALSE;
    } else if (event->button == 1 && REGION_R(82, 54, 15, 9)) {
        if (cfg.timer_mode == TIMER_ELAPSED)
            cfg.timer_mode = TIMER_REMAINING;
        else
            cfg.timer_mode = TIMER_ELAPSED;
    } else if (event->button == 2 && (event->type == GDK_BUTTON_PRESS) &&
               inside_widget(event->x, event->y, playlistwin_list)) {
        gtk_selection_convert(widget, GDK_SELECTION_PRIMARY, GDK_TARGET_STRING, event->time);
    } else if (cfg.playlist_width >= 350 && REGION_R(223, 151, 26, 10)) {
        if (event->button == 1) {
            cfg.vis_type++;
            if (cfg.vis_type > VIS_OFF)
                cfg.vis_type = VIS_ANALYZER;
            mainwin_vis_set_type(cfg.vis_type);
        } else if (event->button == 3) {
            int mx, my;

            util_get_root_pointer(&mx, &my);
            util_item_factory_popup(mainwin_vis_menu, mx, my, 3, event->time);
            grab = FALSE;
        }
    } else if (event->button == 1 && event->type == GDK_BUTTON_PRESS &&
               !inside_sensitive_widgets(event->x, event->y) && (cfg.easy_move || event->y < 14)) {
        if (hint_move_resize_available()) {
            hint_move_resize(playlistwin, event->x_root, event->y_root, TRUE);
            grab = FALSE;
        } else {
            gdk_window_raise(gtk_widget_get_window(playlistwin));
            dock_move_press(dock_window_list, playlistwin, event, FALSE);
        }
    } else if (event->button == 1 && event->type == GDK_2BUTTON_PRESS &&
               !inside_sensitive_widgets(event->x, event->y) && event->y < 14) {
        playlistwin_set_shade(!cfg.playlist_shaded);
        if (dock_is_moving(playlistwin))
            dock_move_release(playlistwin);
    } else if (event->button == 3) {
        if (!inside_widget(event->x, event->y, playlistwin_list))
            /*
             * Pop up the main menu a few pixels down to avoid
             * anything to be selected initially.
             */
            util_item_factory_popup(mainwin_general_menu, event->x_root, event->y_root + 2, 3,
                                    event->time);
        else {
            int pos, sensitive;
            GtkWidget *w;
            pos = playlist_list_get_playlist_position(playlistwin_list, event->x, event->y);
            if (pos != -1)
                pos++;
            sensitive = pos != -1;
            w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
            gtk_widget_set_sensitive(w, sensitive);

            w = NULL /* TODO(#gtk3): gtk_item_factory_get_widget removed */;
            gtk_widget_set_sensitive(w, sensitive);

            playlistwin_set_sensitive_sortmenu();
            playlistwin_set_sensitive_popupmenu();

            util_item_factory_popup_with_data(playlistwin_popup_menu, GINT_TO_POINTER(pos), NULL,
                                              event->x_root, event->y_root + 5, 3, event->time);
        }
        grab = FALSE;
    } else if (event->button == 4) /* GTK2 fallback: scrollwheel up (GTK3: see scroll-event) */
        playlistwin_scroll(-3);
    else if (event->button == 5)   /* GTK2 fallback: scrollwheel down */
        playlistwin_scroll(3);
    else {
        handle_press_cb(playlistwin_wlist, widget, event);
        draw_playlist_window(FALSE);
    }
    if (grab)
        gdk_pointer_grab(gtk_widget_get_window(playlistwin), FALSE,
                         GDK_BUTTON_MOTION_MASK | GDK_BUTTON_RELEASE_MASK, GDK_NONE, GDK_NONE,
                         GDK_CURRENT_TIME);
}

static void playlistwin_focus_in(GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
    playlistwin_close->pb_allow_draw = TRUE;
    playlistwin_shade->pb_allow_draw = TRUE;
    playlistwin_focus = TRUE;
    draw_playlist_window(TRUE);
}

static void playlistwin_focus_out(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
    playlistwin_close->pb_allow_draw = FALSE;
    playlistwin_shade->pb_allow_draw = FALSE;
    playlistwin_focus = FALSE;
    draw_playlist_window(TRUE);
}

static gboolean playlistwin_configure(GtkWidget *window, GdkEventConfigure *event, gpointer data)
{
    if (!gtk_widget_is_visible(window))
        return FALSE;

    if (cfg.show_wm_decorations || hint_move_resize_available()) {
        if (event->width != cfg.playlist_width ||
            (event->height != cfg.playlist_height && event->height != 14))
            playlistwin_queue_resize(event->width, event->height);
    }

    if (cfg.show_wm_decorations)
        gdk_window_get_root_origin(gtk_widget_get_window(window), &cfg.playlist_x, &cfg.playlist_y);
    else
        gdk_window_get_root_origin(gtk_widget_get_window(window), &cfg.playlist_x, &cfg.playlist_y);

    return FALSE;
}

void playlistwin_set_back_pixmap()
{
    /* GTK3: gdk_window_set_back_pixmap removed; drawing handled by cairo + queue_draw */
    gtk_widget_queue_draw(playlistwin);
}

static int playlistwin_client_event(GtkWidget *w, GdkEventAny *event, gpointer data)
{
    /* TODO(#gtk3): GdkEventClient removed; stub */
    (void)event;
    return FALSE;
}

static int playlistwin_delete(GtkWidget *w, gpointer data)
{
    playlistwin_show(FALSE);
    return TRUE;
}

static void playlistwin_set_hints(void)
{
    GdkGeometry geometry;
    GdkWindowHints mask;

    if (!cfg.show_wm_decorations && !hint_move_resize_available())
        return;

    geometry.min_width = 275;
    geometry.base_width = 275;
    geometry.width_inc = 25;
    geometry.height_inc = 29;
    geometry.max_width = (1 << 16) - 1;
    geometry.max_height = (1 << 16) - 1;
    mask = GDK_HINT_MIN_SIZE | GDK_HINT_RESIZE_INC | GDK_HINT_MAX_SIZE | GDK_HINT_BASE_SIZE;
    if (cfg.playlist_shaded) {
        geometry.min_height = 14;
        geometry.max_height = 14;
        geometry.base_height = 14;
    } else {
        geometry.min_height = 116;
        geometry.base_height = 116;
    }

    gtk_window_set_geometry_hints(GTK_WINDOW(playlistwin), playlistwin, &geometry, mask);
}

static void playlistwin_physically_delete_cb(GtkWidget *widget, gpointer data)
{
    GList *node, *selected_list = data;
    int deleted = 0, length;
    char *message = NULL;

    length = g_list_length(selected_list);
    node = selected_list;
    while (node) {
        GList *next = g_list_next(node);
        if (unlink(node->data) == 0)
            deleted++;
        else {
            if (length == 1)
                message = g_strdup_printf(_("Failed to delete \"%s\": %s."),
                                          (char *)selected_list->data, strerror(errno));

            /* The unlink failed, we dont want to remove
               the file from the playlist either */
            selected_list = g_list_remove_link(selected_list, node);
            g_free(node->data);
            g_list_free_1(node);
        }
        node = next;
    }

    playlist_delete_filenames(selected_list);
    g_list_foreach(selected_list, (GFunc)g_free, NULL);
    g_list_free(selected_list);

    if (length > 1 && deleted < length)
        message = g_strdup_printf(_("%d of %d files successfully deleted."), deleted, length);

    if (message) {
        xmms_show_message(_("XMMS: Files deleted"), message, _("OK"), FALSE, NULL, NULL);
        g_free(message);
    }
}

static void playlistwin_physically_delete(void)
{
    GtkWidget *dialog, *vbox, *label, *bbox, *ok, *cancel;
    GList *node, *selected_list = NULL;
    char *text;
    int length;

    PL_LOCK();
    for (node = get_playlist(); node != NULL; node = g_list_next(node)) {
        PlaylistEntry *entry = node->data;
        /*
         * We do not want to try to unlink url's
         */
        if (entry->selected && !strstr(entry->filename, ":/"))
            selected_list = g_list_prepend(selected_list, g_strdup(entry->filename));
    }
    PL_UNLOCK();
    selected_list = g_list_reverse(selected_list);


    if (!selected_list)
        return;

    dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), _("XMMS: Delete files?"));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), vbox, TRUE, TRUE,
                       0);

    length = g_list_length(selected_list);
    if (length > 1)
        text = g_strdup_printf(_("Really delete %d files?"), length);
    else
        text = g_strdup_printf(_("Really delete: \"%s\"?"), (char *)selected_list->data);

    label = gtk_label_new(text);
    g_free(text);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(dialog))), bbox, FALSE, FALSE,
                       0);

    ok = gtk_button_new_with_label(_("OK"));
    cancel = gtk_button_new_with_label(_("Cancel"));
    g_signal_connect(G_OBJECT(ok), "clicked", G_CALLBACK(playlistwin_physically_delete_cb),
                     selected_list);
    g_signal_connect_swapped(G_OBJECT(ok), "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(dialog));
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(dialog));
    gtk_box_pack_start(GTK_BOX(bbox), ok, FALSE, FALSE, 0);
    gtk_widget_set_can_default(ok, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_widget_grab_default(ok);

    gtk_widget_show_all(dialog);
}

static void playlistwin_keypress_up_down_handler(PlayList_List *pl, gboolean up, guint state)
{
    if ((state & GDK_MOD1_MASK) && (state & GDK_SHIFT_MASK))
        return;
    if (!(state & GDK_MOD1_MASK))
        playlist_select_all(FALSE);

    if (pl->pl_prev_selected == -1 ||
        (!playlistwin_item_visible(pl->pl_prev_selected) &&
         !(state & (GDK_SHIFT_MASK | GDK_MOD1_MASK) && pl->pl_prev_min != -1))) {
        pl->pl_prev_selected = pl->pl_first;
    } else if (state & GDK_SHIFT_MASK) {
        if (pl->pl_prev_min == -1) {
            pl->pl_prev_max = pl->pl_prev_selected;
            pl->pl_prev_min = pl->pl_prev_selected;
        }
        pl->pl_prev_max += (up ? -1 : 1);
        pl->pl_prev_max = CLAMP(pl->pl_prev_max, 0, get_playlist_length() - 1);

        pl->pl_first = MIN(pl->pl_first, pl->pl_prev_max);
        pl->pl_first = MAX(pl->pl_first, pl->pl_prev_max - pl->pl_num_visible + 1);
        playlist_select_range(pl->pl_prev_min, pl->pl_prev_max, TRUE);
        return;
    } else if (state & GDK_MOD1_MASK) {
        if (up)
            playlist_list_move_up(pl);
        else
            playlist_list_move_down(pl);

        if (pl->pl_prev_min != -1) {
            if (pl->pl_prev_min < pl->pl_first)
                pl->pl_first = pl->pl_prev_min;
            else if (pl->pl_prev_max >= (pl->pl_first + pl->pl_num_visible))
                pl->pl_first = pl->pl_prev_max - pl->pl_num_visible + 1;
        }
        return;
    } else if (up)
        pl->pl_prev_selected--;
    else
        pl->pl_prev_selected++;

    pl->pl_prev_selected = CLAMP(pl->pl_prev_selected, 0, get_playlist_length() - 1);

    if (pl->pl_prev_selected < pl->pl_first)
        pl->pl_first--;
    else if (pl->pl_prev_selected >= (pl->pl_first + pl->pl_num_visible))
        pl->pl_first++;

    playlist_select_range(pl->pl_prev_selected, pl->pl_prev_selected, TRUE);
    pl->pl_prev_min = -1;
}

static gboolean playlistwin_keypress(GtkWidget *w, GdkEventKey *event, gpointer data)
{
    guint keyval;
    gboolean refresh = TRUE;

    if (cfg.playlist_shaded) {
        gtk_widget_event(mainwin, (GdkEvent *)event);
        return TRUE;
    }

    switch (keyval = event->keyval) {
    case GDK_KEY_KP_Up:
    case GDK_KEY_KP_Down:
    case GDK_KEY_Up:
    case GDK_KEY_Down:
        playlistwin_keypress_up_down_handler(playlistwin_list,
                                             keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up,
                                             event->state);
        break;
    case GDK_KEY_Page_Up:
        playlistwin_scroll(-playlistwin_list->pl_num_visible);
        break;
    case GDK_KEY_Page_Down:
        playlistwin_scroll(playlistwin_list->pl_num_visible);
        break;
    case GDK_KEY_Home:
        playlistwin_list->pl_first = 0;
        break;
    case GDK_KEY_End:
        playlistwin_list->pl_first = get_playlist_length() - playlistwin_list->pl_num_visible;
        break;
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
        if (playlistwin_list->pl_prev_selected > -1) {
            playlist_set_position(playlistwin_list->pl_prev_selected);
            if (!get_input_playing())
                playlist_play();
        }
        refresh = FALSE;
        break;
    case GDK_KEY_Delete:
        if (event->state & GDK_CONTROL_MASK)
            playlist_delete(TRUE);
        else
            playlist_delete(FALSE);
        refresh = FALSE;
        break;
    case GDK_KEY_Insert:
        if (event->state & GDK_SHIFT_MASK)
            playlistwin_show_dirbrowser();
        else if (event->state & GDK_MOD1_MASK)
            playlistwin_show_add_url_window();
        else
            playlistwin_show_filebrowser();
        refresh = FALSE;
        break;
    default:
        if (!gtk_accel_groups_activate(G_OBJECT(playlistwin), event->keyval, event->state))
            gtk_widget_event(mainwin, (GdkEvent *)event);
        refresh = FALSE;
        break;
    }
    if (refresh)
        playlistwin_update_list();

    return TRUE;
}

static void playlistwin_draw_frame(void)
{
    int w, h, y, i, c;
    SkinIndex src;

    w = cfg.playlist_width;
    h = cfg.playlist_height;
    src = SKIN_PLEDIT;

    if (cfg.playlist_shaded) {
        skin_draw_pixmap(playlistwin_gc, src, 72, 42, 0, 0, 25, 14);
        c = (w - 75) / 25;
        for (i = 0; i < c; i++)
            skin_draw_pixmap(playlistwin_gc, src, 72, 57, (i * 25) + 25, 0, 25, 14);
        skin_draw_pixmap(playlistwin_gc, src, 99,
                         (!playlistwin_focus && cfg.dim_titlebar) ? 57 : 42, w - 50, 0, 50, 14);
    } else {
        if (playlistwin_focus || !cfg.dim_titlebar)
            y = 0;
        else
            y = 21;
        /* Titlebar left corner */
        skin_draw_pixmap(playlistwin_gc, src, 0, y, 0, 0, 25, 20);
        c = (w - 150) / 25;
        /* Titlebar, left and right of the title */
        for (i = 0; i < c / 2; i++) {
            skin_draw_pixmap(playlistwin_gc, src, 127, y, (i * 25) + 25, 0, 25, 20);
            skin_draw_pixmap(playlistwin_gc, src, 127, y, (i * 25) + (w / 2) + 50, 0, 25, 20);
        }
        if (c & 1) {
            skin_draw_pixmap(playlistwin_gc, src, 127, y, ((c / 2) * 25) + 25, 0, 12, 20);
            skin_draw_pixmap(playlistwin_gc, src, 127, y, (w / 2) + ((c / 2) * 25) + 50, 0, 13, 20);
        }

        /* Titlebar title */
        skin_draw_pixmap(playlistwin_gc, src, 26, y, (w / 2) - 50, 0, 100, 20);
        /* Titlebar, right corner */
        skin_draw_pixmap(playlistwin_gc, src, 153, y, w - 25, 0, 25, 20);

        /* Left and right side */
        for (i = 0; i < (h - 58) / 29; i++) {
            skin_draw_pixmap(playlistwin_gc, src, 0, 42, 0, (i * 29) + 20, 12, 29);
            skin_draw_pixmap(playlistwin_gc, src, 32, 42, w - 19, (i * 29) + 20, 19, 29);
        }
        /* Bottom left corner (menu buttons) */
        skin_draw_pixmap(playlistwin_gc, src, 0, 72, 0, h - 38, 125, 38);
        c = (w - 275) / 25;
        /* Visualization window */
        if (c >= 3) {
            c -= 3;
            skin_draw_pixmap(playlistwin_gc, src, 205, 0, w - 225, h - 38, 75, 38);
        }
        /* Bottom blank parts */
        for (i = 0; i < c; i++)
            skin_draw_pixmap(playlistwin_gc, src, 179, 0, (i * 25) + 125, h - 38, 25, 38);
        /* Bottom right corner (playbuttons etc) */
        skin_draw_pixmap(playlistwin_gc, src, 125, 72, w - 150, h - 38, 150, 38);
    }
}

void draw_playlist_window(gboolean force)
{
    gboolean redraw;
    GList *wl;
    Widget *w;

    if (force) {
        playlistwin_draw_frame();
        lock_widget_list(playlistwin_wlist);
        draw_widget_list(playlistwin_wlist, &redraw, TRUE);

    } else {
        lock_widget_list(playlistwin_wlist);
        draw_widget_list(playlistwin_wlist, &redraw, FALSE);
    }
    if (redraw || force) {
        if (!force) {
            wl = playlistwin_wlist;
            while (wl) {
                w = (Widget *)wl->data;
                if (w->redraw && w->visible)
                    w->redraw = FALSE;
                wl = wl->next;
            }
        }
        /* GTK3: always queue the window repaint when backing surface was updated */
        gtk_widget_queue_draw(playlistwin);
        /* gdk_flush() no-op in GTK3 */
    }
    unlock_widget_list(playlistwin_wlist);
}

static void playlistwin_sort_menu_callback(gpointer cb_data, guint action, GtkWidget *w)
{
    switch (action) {
    case PLAYLISTWIN_SORT_BYTITLE:
        playlist_sort_by_title();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_BYFILENAME:
        playlist_sort_by_filename();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_BYPATH:
        playlist_sort_by_path();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_BYDATE:
        playlist_sort_by_date();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_SEL_BYTITLE:
        playlist_sort_selected_by_title();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_SEL_BYFILENAME:
        playlist_sort_selected_by_filename();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_SEL_BYPATH:
        playlist_sort_selected_by_path();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_SEL_BYDATE:
        playlist_sort_selected_by_date();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_REVERSE:
        playlist_reverse();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_RANDOMIZE:
        playlist_random();
        playlistwin_update_list();
        break;
    case PLAYLISTWIN_SORT_SEL_RANDOMIZE:
        playlist_randomize_selected();
        playlistwin_update_list();
        break;
    }
}

static void playlistwin_sub_menu_callback(gpointer cb_data, guint action, GtkWidget *w)
{
    switch (action) {
    case PLAYLISTWIN_REMOVE_DEAD_FILES:
        playlist_remove_dead_files();
        break;
    case PLAYLISTWIN_PHYSICALLY_DELETE:
        playlistwin_physically_delete();
        break;
    }
}


void playlistwin_hide_timer(void)
{
    textbox_set_text(playlistwin_time_min, "   ");
    textbox_set_text(playlistwin_time_sec, "  ");
}

void playlistwin_vis_enable(void)
{
    playlistwin_vis_enabled = TRUE;
    if (cfg.playlist_width >= 350)
        show_widget(playlistwin_vis);
}

void playlistwin_vis_disable(void)
{
    playlistwin_vis_enabled = FALSE;
    hide_widget(playlistwin_vis);
    draw_playlist_window(TRUE);
}

void playlistwin_set_time(int time, int length, TimerMode mode)
{
    char *text, sign;

    if (mode == TIMER_REMAINING && length != -1) {
        time = length - time;

        sign = '-';
    } else
        sign = ' ';

    time /= 1000;

    if (time < 0)
        time = 0;
    if (time > 99 * 60)
        time /= 60;

    text = g_strdup_printf("%c%-2.2d", sign, time / 60);
    textbox_set_text(playlistwin_time_min, text);
    g_free(text);
    text = g_strdup_printf("%-2.2d", time % 60);
    textbox_set_text(playlistwin_time_sec, text);
    g_free(text);
}

static void playlistwin_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x,
                                           gint y, GtkSelectionData *selection_data, guint info,
                                           guint time, gpointer user_data)
{
    guint pos;

    if (gtk_selection_data_get_data(selection_data)) {
        if (inside_widget(x, y, playlistwin_list)) {
            pos = ((y - ((Widget *)playlistwin_list)->y) / playlistwin_list->pl_fheight) +
                  playlistwin_list->pl_first;
            if (pos > get_playlist_length())
                pos = get_playlist_length();
            playlist_ins_url_string(gtk_selection_data_get_data(selection_data), pos);
        } else
            playlist_add_url_string(gtk_selection_data_get_data(selection_data));
    }
}

static void playlistwin_close_cb(void)
{
    playlistwin_show(FALSE);
}

static void playlistwin_create_widgets(void)
{
    playlistwin_sinfo = create_textbox(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, 4, 4,
                                       cfg.playlist_width - 35, FALSE, SKIN_TEXT);
    if (!cfg.playlist_shaded)
        hide_widget(playlistwin_sinfo);
    if (cfg.playlist_shaded)
        playlistwin_shade = create_pbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc,
                                           cfg.playlist_width - 21, 3, 9, 9, 128, 45, 150, 42,
                                           playlistwin_shade_toggle, SKIN_PLEDIT);
    else
        playlistwin_shade = create_pbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc,
                                           cfg.playlist_width - 21, 3, 9, 9, 157, 3, 62, 42,
                                           playlistwin_shade_toggle, SKIN_PLEDIT);
    playlistwin_shade->pb_allow_draw = FALSE;
    playlistwin_close =
        create_pbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 11,
                       3, 9, 9, cfg.playlist_shaded ? 138 : 167, cfg.playlist_shaded ? 45 : 3, 52,
                       42, playlistwin_close_cb, SKIN_PLEDIT);
    playlistwin_close->pb_allow_draw = FALSE;
    playlistwin_list = create_playlist_list(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, 12,
                                            20, cfg.playlist_width - 31, cfg.playlist_height - 58);
    playlist_list_set_font(cfg.playlist_font);
    playlistwin_slider = create_playlistslider(&playlistwin_wlist, playlistwin_bg, playlistwin_gc,
                                               cfg.playlist_width - 15, 20,
                                               cfg.playlist_height - 58, playlistwin_list);
    playlistwin_time_min =
        create_textbox(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 82,
                       cfg.playlist_height - 15, 15, FALSE, SKIN_TEXT);
    playlistwin_time_sec =
        create_textbox(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 64,
                       cfg.playlist_height - 15, 10, FALSE, SKIN_TEXT);
    playlistwin_info =
        create_textbox(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 143,
                       cfg.playlist_height - 28, 85, FALSE, SKIN_TEXT);
    playlistwin_vis =
        create_vis(&playlistwin_wlist, playlistwin_bg, gtk_widget_get_window(playlistwin),
                   playlistwin_gc, cfg.playlist_width - 223, cfg.playlist_height - 26, 72, FALSE);
    hide_widget(playlistwin_vis);

    playlistwin_srew =
        create_sbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 144,
                       cfg.playlist_height - 16, 8, 7, playlist_prev);
    playlistwin_splay =
        create_sbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 138,
                       cfg.playlist_height - 16, 10, 7, mainwin_play_pushed);
    playlistwin_spause =
        create_sbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 128,
                       cfg.playlist_height - 16, 10, 7, input_pause);
    playlistwin_sstop =
        create_sbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 118,
                       cfg.playlist_height - 16, 9, 7, mainwin_stop_pushed);
    playlistwin_sfwd =
        create_sbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 109,
                       cfg.playlist_height - 16, 8, 7, playlist_next);
    playlistwin_seject =
        create_sbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 100,
                       cfg.playlist_height - 16, 9, 7, mainwin_eject_pushed);
    playlistwin_sscroll_up =
        create_sbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 14,
                       cfg.playlist_height - 35, 8, 5, playlistwin_scroll_up_pushed);
    playlistwin_sscroll_down =
        create_sbutton(&playlistwin_wlist, playlistwin_bg, playlistwin_gc, cfg.playlist_width - 14,
                       cfg.playlist_height - 30, 8, 5, playlistwin_scroll_down_pushed);
}

static void selection_received(GtkWidget *widget, GtkSelectionData *selection_data, gpointer data)
{
    if (gtk_selection_data_get_data_type(selection_data) == GDK_SELECTION_TYPE_STRING &&
        gtk_selection_data_get_length(selection_data) > 0)
        playlist_add_url_string(gtk_selection_data_get_data(selection_data));
}

/* GTK3: blit backing surface to playlist window on every redraw */
static gboolean playlistwin_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    (void)widget;
    (void)data;

    if (playlistwin_bg) {
        GdkColor *bg = get_skin_color(SKIN_PLEDIT_NORMALBG);
        if (bg)
            cairo_set_source_rgb(cr, bg->red / 65535.0, bg->green / 65535.0, bg->blue / 65535.0);
        else
            cairo_set_source_rgb(cr, 0.0, 0.039, 0.118);
        cairo_paint(cr);

        cairo_set_source_surface(cr, playlistwin_bg, 0, 0);
        cairo_paint(cr);
    }
    return FALSE;
}

static void playlistwin_create_gtk(void)
{
    playlistwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    dock_add_window(dock_window_list, playlistwin);
    gtk_widget_set_app_paintable(playlistwin, TRUE);
    if (cfg.show_wm_decorations)
        gtk_window_set_resizable(GTK_WINDOW(playlistwin), TRUE); /* TODO(#gtk3): was set_policy */
    else {
        /* GTK3: gtk_window_set_policy(non-resizable) → set_resizable(FALSE).
         * This suppresses GTK3's built-in resize grip, which is a child GdkWindow
         * painted opaque black when app_paintable=TRUE, causing a visible black
         * block in the bottom-right corner of the playlist skin area (issue #21).
         * Custom corner-drag resize via gdk_window_resize() is unaffected. */
        gtk_window_set_resizable(GTK_WINDOW(playlistwin), FALSE);
        gtk_window_set_title(GTK_WINDOW(playlistwin), _("XMMS Playlist"));
    }
    gtk_window_set_wmclass(GTK_WINDOW(playlistwin), "XMMS_Playlist", "xmms");
    /* GTK3: do not set transient_for for peer skin windows — causes WM to hide
     * playlistwin when mainwin loses focus. Dialogs set their own transient_for. */
    if (cfg.playlist_x != -1 && cfg.save_window_position)
        dock_set_uposition(playlistwin, cfg.playlist_x, cfg.playlist_y);
    gtk_widget_set_size_request(playlistwin, cfg.playlist_width,
                                cfg.playlist_shaded ? 14 : cfg.playlist_height);
    gtk_widget_set_events(playlistwin, GDK_FOCUS_CHANGE_MASK | GDK_BUTTON_MOTION_MASK |
                                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                           GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
    gtk_widget_realize(playlistwin);
    hint_set_skip_winlist(playlistwin);
    playlistwin_set_hints();
    util_set_cursor(playlistwin);

    g_signal_connect(G_OBJECT(playlistwin), "delete_event", G_CALLBACK(playlistwin_delete), NULL);
    g_signal_connect(G_OBJECT(playlistwin), "button_press_event", G_CALLBACK(playlistwin_press),
                     NULL);
    g_signal_connect(G_OBJECT(playlistwin), "button_release_event", G_CALLBACK(playlistwin_release),
                     NULL);
    g_signal_connect(G_OBJECT(playlistwin), "motion_notify_event", G_CALLBACK(playlistwin_motion),
                     NULL);
    g_signal_connect(G_OBJECT(playlistwin), "focus_in_event", G_CALLBACK(playlistwin_focus_in),
                     NULL);
    g_signal_connect(G_OBJECT(playlistwin), "focus_out_event", G_CALLBACK(playlistwin_focus_out),
                     NULL);
    g_signal_connect(G_OBJECT(playlistwin), "configure_event", G_CALLBACK(playlistwin_configure),
                     NULL);
    /* GTK3: client_event signal removed */
    xmms_drag_dest_set(playlistwin);
    g_signal_connect(G_OBJECT(playlistwin), "drag-data-received",
                     G_CALLBACK(playlistwin_drag_data_received), NULL);
    g_signal_connect(G_OBJECT(playlistwin), "key-press-event", G_CALLBACK(playlistwin_keypress),
                     NULL);
    /* GTK3: scroll-event for mouse-wheel list scrolling */
    g_signal_connect(G_OBJECT(playlistwin), "scroll-event", G_CALLBACK(playlistwin_scroll_event_cb),
                     NULL);
    g_signal_connect(G_OBJECT(playlistwin), "selection_received", G_CALLBACK(selection_received),
                     NULL);
    /* GTK3: blit backing surface on every redraw */
    g_signal_connect(G_OBJECT(playlistwin), "draw", G_CALLBACK(playlistwin_draw_cb), NULL);

    if (!cfg.show_wm_decorations)
        gdk_window_set_decorations(gtk_widget_get_window(playlistwin), 0);

    playlistwin_create_mask();
}

void playlistwin_create(void)
{
    GtkWidget *item, *menu;

    playlistwin_accel = gtk_accel_group_new();

    playlistwin_sort_menu = gtk_menu_new();
    /* TODO(#gtk3): gtk_item_factory_set_translate_func removed */
    /* TODO(#gtk3): gtk_item_factory_create_items removed */
    playlistwin_sub_menu = gtk_menu_new();
    /* TODO(#gtk3): gtk_item_factory_set_translate_func removed */
    /* TODO(#gtk3): gtk_item_factory_create_items removed */
    playlistwin_bg =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, cfg.playlist_width, cfg.playlist_height);

    playlistwin_popup_menu = gtk_menu_new();
    /* TODO(#gtk3): gtk_item_factory_set_translate_func removed */
    /* TODO(#gtk3): gtk_item_factory_create_items removed */

    /* TODO(#gtk3): submenu items not yet ported from GtkItemFactory */

    playlistwin_save_menu = gtk_menu_new();
    /* TODO(#gtk3): gtk_item_factory_set_translate_func removed */
    /* TODO(#gtk3): gtk_item_factory_create_items removed */

    playlistwin_create_gtk();
    playlistwin_gc = cairo_create(playlistwin_bg);
    playlistwin_create_widgets();

    playlistwin_update_info();
}

void playlistwin_recreate(void)
{
    dock_window_list = g_list_remove(dock_window_list, playlistwin);
    gtk_widget_destroy(playlistwin);
    playlistwin_create_gtk();
    vis_set_window(playlistwin_vis, gtk_widget_get_window(playlistwin));
}


void playlistwin_show(gboolean show)
{
    if (show)
        playlistwin_real_show();
    else
        playlistwin_real_hide();
}

void playlistwin_real_show(void)
{
    /* GTK3 fix: single unified position logic — saved position wins, otherwise snap
     * to the right edge of mainwin so they appear adjacent on first run */
    if (cfg.save_window_position && cfg.playlist_x != -1) {
        dock_set_uposition(playlistwin, cfg.playlist_x, cfg.playlist_y);
    } else {
        gint mx = 0, my = 0;
        gtk_window_get_position(GTK_WINDOW(mainwin), &mx, &my);
        dock_set_uposition(playlistwin, mx + 275, my);
    }
    gtk_widget_show(playlistwin);
    gtk_widget_set_size_request(playlistwin, cfg.playlist_width, PLAYLIST_HEIGHT);
    /* GTK3 fix: explicitly resize the GdkWindow to the exact skin dimensions.
     * set_size_request only sets the minimum; the WM may honour a larger size
     * (e.g. adding CSD padding/resize-handle area) leaving unpainted overflow.
     * This matches the gdk_window_resize call already used in playlistwin_motion. */
    gdk_window_resize(gtk_widget_get_window(playlistwin), cfg.playlist_width, PLAYLIST_HEIGHT);
    /* gdk_flush() no-op in GTK3 */
    draw_playlist_window(TRUE);
    tbutton_set_toggled(mainwin_pl, TRUE);
    cfg.playlist_visible = TRUE;
    playlistwin_set_toprow(0);
    playlist_check_pos_current();
    hint_set_always(cfg.always_on_top);
    hint_set_sticky(cfg.sticky);
    hint_set_skip_winlist(playlistwin);
    /* GTK3 fix: bring window to front — bare gtk_widget_show() doesn't raise */
    gtk_window_present(GTK_WINDOW(playlistwin));
}

void playlistwin_real_hide(void)
{
    gtk_widget_hide(playlistwin);
    cfg.playlist_visible = FALSE;
    tbutton_set_toggled(mainwin_pl, FALSE);
}

static void playlistwin_popup_menu_callback(gpointer cb_data, guint action, GtkWidget *w)
{
    int pos = 0; /* TODO(#gtk3): gtk_item_factory_popup_data_from_widget removed */
    switch (action) {
    case MISC_FILEINFO:
        if (pos == 0)
            /* We got here via an accelerator */
            playlistwin_fileinfo();
        else if (pos != -1)
            playlist_fileinfo(pos - 1);
        break;
    case MISC_QUEUE:
        if (pos == 0 || (pos != -1 && playlist_is_position_selected(pos - 1)))
            /* We got here via an accelerator or by right-clicking on a selection */
            playlist_queue_selected();
        else if (pos != -1)
            playlist_queue_position(pos - 1);
        break;
    case MISC_QUEUE_MANAGER:
        mainwin_queue_manager();
        break;
    case SEL_LOOKUP:
        playlist_read_info_selection();
        break;
    default:
        playlistwin_popup_handler(action);
    }
}
