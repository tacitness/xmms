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
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <signal.h>

#include <getopt.h>

#include "mpris2.h"
#include "xmms.h"

/* GTK3: wrapper to silence incompatible-pointer-types warning */
static void on_widget_destroyed(GtkWidget *w, gpointer data)
{
    gtk_widget_destroyed(w, (GtkWidget **)data);
}
#ifdef HAVE_SCHED_H
#    include <sched.h>
#elif defined HAVE_SYS_SCHED_H
#    include <sys/sched.h>
#endif
#include "libxmms/configfile.h"
#include "libxmms/dirbrowser.h"
#include "libxmms/snap.h"
#include "libxmms/util.h"
#include "libxmms/xmmsctrl.h"
#include "xmms_mini.xpm"

#define RANDTABLE_SIZE 128

GtkWidget *mainwin, *mainwin_url_window = NULL, *mainwin_dir_browser = NULL;
GtkWidget *mainwin_jtt = NULL, *mainwin_jtf = NULL, *mainwin_qm = NULL;
GtkWidget *mainwin_options_menu, *mainwin_songname_menu, *mainwin_vis_menu;
GtkWidget *mainwin_general_menu;
static GtkWidget *mainwin_vis_plugins_sep = NULL;
static GtkWidget *mainwin_vis_plugins_manage_item = NULL;
static GList *mainwin_vis_plugin_items = NULL;

/* Globally stored menu item widgets for programmatic toggling */
static GtkWidget *mi_opt_shuffle = NULL, *mi_opt_repeat = NULL, *mi_opt_npa = NULL;
static GtkWidget *mi_opt_always = NULL, *mi_opt_sticky = NULL;
static GtkWidget *mi_opt_ws = NULL, *mi_opt_pws = NULL, *mi_opt_eqws = NULL;
static GtkWidget *mi_opt_doublesize = NULL, *mi_opt_easymove = NULL;
static GtkWidget *mi_opt_telapsed = NULL, *mi_opt_tremaining = NULL, *mi_opt_tdisplay = NULL;
static GtkWidget *mi_sn_autoscroll = NULL;
static GtkWidget *mi_gen_showmwin = NULL, *mi_gen_showplwin = NULL, *mi_gen_showeqwin = NULL;
static GtkWidget *mi_vis_analyzer = NULL, *mi_vis_scope = NULL, *mi_vis_off = NULL;
cairo_surface_t *mainwin_bg = NULL, *mainwin_bg_dblsize;
cairo_t *mainwin_cr;

GtkAccelGroup *mainwin_accel;

gboolean mainwin_focus = FALSE;
gboolean setting_volume = FALSE;
gint mainwin_timeout_tag;
static guint mainwin_tick_id = 0;

PButton *mainwin_menubtn, *mainwin_minimize, *mainwin_shade, *mainwin_close;
PButton *mainwin_rew, *mainwin_play, *mainwin_pause, *mainwin_stop, *mainwin_fwd, *mainwin_eject;
SButton *mainwin_srew, *mainwin_splay, *mainwin_spause, *mainwin_sstop, *mainwin_sfwd,
    *mainwin_seject, *mainwin_about;
TButton *mainwin_shuffle, *mainwin_repeat, *mainwin_eq, *mainwin_pl;
TextBox *mainwin_info, *mainwin_rate_text, *mainwin_freq_text, *mainwin_stime_min,
    *mainwin_stime_sec;
MenuRow *mainwin_menurow;
HSlider *mainwin_volume, *mainwin_balance, *mainwin_position, *mainwin_sposition = NULL;
MonoStereo *mainwin_monostereo;
PlayStatus *mainwin_playstatus;
Number *mainwin_minus_num, *mainwin_10min_num, *mainwin_min_num, *mainwin_10sec_num,
    *mainwin_sec_num;
Vis *mainwin_vis;
SVis *mainwin_svis;

GList *mainwin_wlist = NULL;
GList *disabled_iplugins = NULL;

GList *dock_window_list = NULL;

static int bitrate = 0, frequency = 0, numchannels = 0;

Config cfg;

static gboolean mainwin_force_redraw = FALSE;
static gchar *mainwin_title_text = NULL;
static gboolean mainwin_info_text_locked = FALSE;

/* For x11r5 session management */
static char **restart_argv;
static int restart_argc;

Vis *active_vis;
static cairo_surface_t *nullmask;
static gint balance;
gboolean pposition_broken = FALSE;
static pthread_mutex_t title_mutex = PTHREAD_MUTEX_INITIALIZER;

extern gchar *plugin_dir_list[];

enum {
    MAINWIN_JTF_COL_QUEUE,
    MAINWIN_JTF_COL_FILE,
    MAINWIN_JTF_COL_PLAYLIST_POS,
    MAINWIN_JTF_N_COLUMNS
};

enum {
    MAINWIN_QM_COL_QUEUE,
    MAINWIN_QM_COL_FILE,
    MAINWIN_QM_COL_QUEUE_POS,
    MAINWIN_QM_N_COLUMNS
};

enum { VOLSET_STARTUP, VOLSET_UPDATE, VOLUME_ADJUSTED, VOLUME_SET };

void read_volume(gint when);

static cairo_region_t *mainwin_create_opaque_region(gint width, gint height,
                                                    cairo_surface_t **surface_out)
{
    cairo_region_t *region;
    cairo_surface_t *surface;
    cairo_t *cr;

    g_return_val_if_fail(width > 0, NULL);
    g_return_val_if_fail(height > 0, NULL);

    /* GTK3: replace 1-bit GdkBitmap masks with an opaque cairo A1 surface. */
    surface = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
    cr = cairo_create(surface);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    cairo_destroy(cr);

    region = gdk_cairo_region_create_from_surface(surface);
    if (surface_out)
        *surface_out = surface;
    else
        cairo_surface_destroy(surface);

    return region;
}

const GtkTargetEntry _xmms_drop_types[] = {{"text/plain", 0, XMMS_DROP_PLAINTEXT},
                                           {"text/uri-list", 0, XMMS_DROP_URLENCODED},
                                           {"STRING", 0, XMMS_DROP_STRING}};

void mainwin_options_menu_callback(gpointer cb_data, guint action, GtkWidget *w);
void mainwin_volume_motioncb(gint pos);
static void set_timer_mode_menu_cb(TimerMode mode);
static void mainwin_queue_manager_queue_refresh(GtkWidget *widget, gpointer userdata);

static gboolean mainwin_tree_view_get_selected_int(GtkTreeView *view, gint column, gint *value)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    g_return_val_if_fail(view != NULL, FALSE);
    g_return_val_if_fail(value != NULL, FALSE);

    selection = gtk_tree_view_get_selection(view);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return FALSE;

    gtk_tree_model_get(model, &iter, column, value, -1);
    return TRUE;
}

static gint mainwin_tree_view_get_cursor_int(GtkTreeView *view, gint column)
{
    GtkTreeModel *model;
    GtkTreePath *path = NULL;
    GtkTreeIter iter;
    gint value = -1;

    g_return_val_if_fail(view != NULL, -1);

    model = gtk_tree_view_get_model(view);
    gtk_tree_view_get_cursor(view, &path, NULL);
    if (path == NULL)
        return -1;

    if (gtk_tree_model_get_iter(model, &iter, path))
        gtk_tree_model_get(model, &iter, column, &value, -1);
    gtk_tree_path_free(path);

    return value;
}

static void mainwin_tree_view_select_int(GtkTreeView *view, gint column, gint value)
{
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    g_return_if_fail(view != NULL);

    model = gtk_tree_view_get_model(view);
    selection = gtk_tree_view_get_selection(view);

    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do {
        gint row_value = -1;
        gtk_tree_model_get(model, &iter, column, &row_value, -1);
        if (row_value == value) {
            GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
            gtk_tree_selection_select_iter(selection, &iter);
            gtk_tree_view_set_cursor(view, path, NULL, FALSE);
            gtk_tree_view_scroll_to_cell(view, path, NULL, TRUE, 0.5f, 0.0f);
            gtk_tree_path_free(path);
            return;
        }
    } while (gtk_tree_model_iter_next(model, &iter));
}

static void mainwin_tree_view_select_first(GtkTreeView *view)
{
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreePath *path;

    g_return_if_fail(view != NULL);

    model = gtk_tree_view_get_model(view);
    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    selection = gtk_tree_view_get_selection(view);
    path = gtk_tree_model_get_path(model, &iter);
    gtk_tree_selection_select_iter(selection, &iter);
    gtk_tree_view_set_cursor(view, path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell(view, path, NULL, TRUE, 0.0f, 0.0f);
    gtk_tree_path_free(path);
}

static GtkWidget *mainwin_create_jump_tree_view(void)
{
    GtkListStore *store;
    GtkWidget *view;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    store = gtk_list_store_new(MAINWIN_JTF_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Q"), renderer, "text",
                                                      MAINWIN_JTF_COL_QUEUE, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Files"), renderer, "text",
                                                      MAINWIN_JTF_COL_FILE, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    return view;
}

static GtkWidget *mainwin_create_queue_tree_view(void)
{
    GtkListStore *store;
    GtkWidget *view;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    store = gtk_list_store_new(MAINWIN_QM_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Q"), renderer, "text",
                                                      MAINWIN_QM_COL_QUEUE, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Files"), renderer, "text",
                                                      MAINWIN_QM_COL_FILE, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    return view;
}

enum {
    MAINWIN_OPT_PREFS,
    MAINWIN_OPT_SKIN,
    MAINWIN_OPT_RELOADSKIN,
    MAINWIN_OPT_REPEAT,
    MAINWIN_OPT_SHUFFLE,
    MAINWIN_OPT_NPA,
    MAINWIN_OPT_TELAPSED,
    MAINWIN_OPT_TREMAINING,
    MAINWIN_OPT_TDISPLAY,
    MAINWIN_OPT_ALWAYS,
    MAINWIN_OPT_STICKY,
    MAINWIN_OPT_WS,
    MAINWIN_OPT_PWS,
    MAINWIN_OPT_EQWS,
    MAINWIN_OPT_DOUBLESIZE,
    MAINWIN_OPT_EASY_MOVE
};

#if 0 /* GTK3: GtkItemFactory removed */
GtkItemFactoryEntry mainwin_options_menu_entries[] = {
    {N_("/Preferences"), "<control>P", mainwin_options_menu_callback, MAINWIN_OPT_PREFS, "<Item>"},
    {N_("/Skin Browser"), "<alt>S", mainwin_options_menu_callback, MAINWIN_OPT_SKIN, "<Item>"},
    {N_("/Reload skin"), "F5", mainwin_options_menu_callback, MAINWIN_OPT_RELOADSKIN, "<Item>"},
    {N_("/-"), NULL, NULL, 0, "<Separator>"},
    {N_("/Repeat"), "R", mainwin_options_menu_callback, MAINWIN_OPT_REPEAT, "<ToggleItem>"},
    {N_("/Shuffle"), "S", mainwin_options_menu_callback, MAINWIN_OPT_SHUFFLE, "<ToggleItem>"},
    {N_("/No Playlist Advance"), "<control>N", mainwin_options_menu_callback, MAINWIN_OPT_NPA,
     "<ToggleItem>"},
    {N_("/-"), NULL, NULL, 0, "<Separator>"},
    {N_("/Time Elapsed"), "<control>E", mainwin_options_menu_callback, MAINWIN_OPT_TELAPSED,
     "<RadioItem>"},
    {N_("/Time Remaining"), "<control>R", mainwin_options_menu_callback, MAINWIN_OPT_TREMAINING,
     "/Time Elapsed"},
    {N_("/Time Display (MMM:SS)"), "<shift><control>L", mainwin_options_menu_callback,
     MAINWIN_OPT_TDISPLAY, "<ToggleItem>"},
    {N_("/-"), NULL, NULL, 0, "<Separator>"},
    {N_("/Always On Top"), "<control>A", mainwin_options_menu_callback, MAINWIN_OPT_ALWAYS,
     "<ToggleItem>"},
    {N_("/Show on all desktops"), "<control>S", mainwin_options_menu_callback, MAINWIN_OPT_STICKY,
     "<ToggleItem>"},
    {N_("/WindowShade Mode"), "<control>W", mainwin_options_menu_callback, MAINWIN_OPT_WS,
     "<ToggleItem>"},
    {N_("/Playlist WindowShade Mode"), "<control><shift>W", mainwin_options_menu_callback,
     MAINWIN_OPT_PWS, "<ToggleItem>"},
    {N_("/Equalizer WindowShade Mode"), "<control><alt>W", mainwin_options_menu_callback,
     MAINWIN_OPT_EQWS, "<ToggleItem>"},
    {N_("/DoubleSize"), "<control>D", mainwin_options_menu_callback, MAINWIN_OPT_DOUBLESIZE,
     "<ToggleItem>"},
    {N_("/Easy Move"), "<control>E", mainwin_options_menu_callback, MAINWIN_OPT_EASY_MOVE,
     "<ToggleItem>"},
};
#endif

static gint mainwin_options_menu_entries_num = 0; /* GTK3: entries disabled */

void mainwin_songname_menu_callback(gpointer cb_data, guint action, GtkWidget *w);

enum {
    MAINWIN_SONGNAME_FILEINFO,
    MAINWIN_SONGNAME_JTF,
    MAINWIN_SONGNAME_JTT,
    MAINWIN_SONGNAME_SCROLL
};

#if 0 /* GTK3: GtkItemFactory removed */
GtkItemFactoryEntry mainwin_songname_menu_entries[] = {
    {N_("/File Info"), NULL, mainwin_songname_menu_callback, MAINWIN_SONGNAME_FILEINFO, "<Item>"},
    {N_("/Jump To File"), "J", mainwin_songname_menu_callback, MAINWIN_SONGNAME_JTF, "<Item>"},
    {N_("/Jump To Time"), "<control>J", mainwin_songname_menu_callback, MAINWIN_SONGNAME_JTT,
     "<Item>"},
    {N_("/Autoscroll Song Name"), NULL, mainwin_songname_menu_callback, MAINWIN_SONGNAME_SCROLL,
     "<ToggleItem>"},
};
#endif

static gint mainwin_songname_menu_entries_num = 0; /* GTK3: entries disabled */

void mainwin_vis_menu_callback(gpointer cb_data, guint action, GtkWidget *w);
static void mainwin_vis_plugin_menu_cb(GtkCheckMenuItem *item, gpointer data);
static void mainwin_clear_vis_plugin_menu_items(void);

enum {
    MAINWIN_VIS_ANALYZER,
    MAINWIN_VIS_SCOPE,
    MAINWIN_VIS_OFF,
    MAINWIN_VIS_ANALYZER_NORMAL,
    MAINWIN_VIS_ANALYZER_FIRE,
    MAINWIN_VIS_ANALYZER_VLINES,
    MAINWIN_VIS_ANALYZER_LINES,
    MAINWIN_VIS_ANALYZER_BARS,
    MAINWIN_VIS_ANALYZER_PEAKS,
    MAINWIN_VIS_SCOPE_DOT,
    MAINWIN_VIS_SCOPE_LINE,
    MAINWIN_VIS_SCOPE_SOLID,
    MAINWIN_VIS_VU_NORMAL,
    MAINWIN_VIS_VU_SMOOTH,
    MAINWIN_VIS_REFRESH_FULL,
    MAINWIN_VIS_REFRESH_HALF,
    MAINWIN_VIS_REFRESH_QUARTER,
    MAINWIN_VIS_REFRESH_EIGHTH,
    MAINWIN_VIS_AFALLOFF_SLOWEST,
    MAINWIN_VIS_AFALLOFF_SLOW,
    MAINWIN_VIS_AFALLOFF_MEDIUM,
    MAINWIN_VIS_AFALLOFF_FAST,
    MAINWIN_VIS_AFALLOFF_FASTEST,
    MAINWIN_VIS_PFALLOFF_SLOWEST,
    MAINWIN_VIS_PFALLOFF_SLOW,
    MAINWIN_VIS_PFALLOFF_MEDIUM,
    MAINWIN_VIS_PFALLOFF_FAST,
    MAINWIN_VIS_PFALLOFF_FASTEST,
    MAINWIN_VIS_PLUGINS
};

#if 0 /* GTK3: GtkItemFactory removed */
GtkItemFactoryEntry mainwin_vis_menu_entries[] =
    {{N_("/Visualization Mode"), NULL, NULL, 0, "<Branch>"},
     {N_("/Visualization Mode/Analyzer"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_ANALYZER,
      "<RadioItem>"},
     {N_("/Visualization Mode/Scope"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_SCOPE,
      "/Visualization Mode/Analyzer"},
     {N_("/Visualization Mode/Off"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_OFF,
      "/Visualization Mode/Analyzer"},
     {N_("/Analyzer Mode"), NULL, NULL, 0, "<Branch>"},
     {N_("/Analyzer Mode/Normal"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_ANALYZER_NORMAL,
      "<RadioItem>"},
     {N_("/Analyzer Mode/Fire"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_ANALYZER_FIRE,
      "/Analyzer Mode/Normal"},
     {N_("/Analyzer Mode/Vertical Lines"), NULL, mainwin_vis_menu_callback,
      MAINWIN_VIS_ANALYZER_VLINES, "/Analyzer Mode/Normal"},
     {N_("/Analyzer Mode/-"), NULL, NULL, 0, "<Separator>"},
     {N_("/Analyzer Mode/Lines"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_ANALYZER_LINES,
      "<RadioItem>"},
     {N_("/Analyzer Mode/Bars"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_ANALYZER_BARS,
      "/Analyzer Mode/Lines"},
     {N_("/Analyzer Mode/-"), NULL, NULL, 0, "<Separator>"},
     {N_("/Analyzer Mode/Peaks"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_ANALYZER_PEAKS,
      "<ToggleItem>"},
     {N_("/Scope Mode"), NULL, NULL, 0, "<Branch>"},
     {N_("/Scope Mode/Dot Scope"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_SCOPE_DOT,
      "<RadioItem>"},
     {N_("/Scope Mode/Line Scope"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_SCOPE_LINE,
      "/Scope Mode/Dot Scope"},
     {N_("/Scope Mode/Solid Scope"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_SCOPE_SOLID,
      "/Scope Mode/Dot Scope"},
     {N_("/WindowShade VU Mode"), NULL, NULL, 0, "<Branch>"},
     {N_("/WindowShade VU Mode/Normal"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_VU_NORMAL,
      "<RadioItem>"},
     {N_("/WindowShade VU Mode/Smooth"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_VU_SMOOTH,
      "/WindowShade VU Mode/Normal"},
     {N_("/Refresh Rate"), NULL, NULL, 0, "<Branch>"},
     {N_("/Refresh Rate/Full (~50 fps)"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_REFRESH_FULL,
      "<RadioItem>"},
     {N_("/Refresh Rate/Half (~25 fps)"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_REFRESH_HALF,
      "/Refresh Rate/Full (~50 fps)"},
     {N_("/Refresh Rate/Quarter (~13 fps)"), NULL, mainwin_vis_menu_callback,
      MAINWIN_VIS_REFRESH_QUARTER, "/Refresh Rate/Full (~50 fps)"},
     {N_("/Refresh Rate/Eighth (~6 fps)"), NULL, mainwin_vis_menu_callback,
      MAINWIN_VIS_REFRESH_EIGHTH, "/Refresh Rate/Full (~50 fps)"},
     {N_("/Analyzer Falloff"), NULL, NULL, 0, "<Branch>"},
     {N_("/Analyzer Falloff/Slowest"), NULL, mainwin_vis_menu_callback,
      MAINWIN_VIS_AFALLOFF_SLOWEST, "<RadioItem>"},
     {N_("/Analyzer Falloff/Slow"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_AFALLOFF_SLOW,
      "/Analyzer Falloff/Slowest"},
     {N_("/Analyzer Falloff/Medium"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_AFALLOFF_MEDIUM,
      "/Analyzer Falloff/Slowest"},
     {N_("/Analyzer Falloff/Fast"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_AFALLOFF_FAST,
      "/Analyzer Falloff/Slowest"},
     {N_("/Analyzer Falloff/Fastest"), NULL, mainwin_vis_menu_callback,
      MAINWIN_VIS_AFALLOFF_FASTEST, "/Analyzer Falloff/Slowest"},
     {N_("/Peaks Falloff"), NULL, NULL, 0, "<Branch>"},
     {N_("/Peaks Falloff/Slowest"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_PFALLOFF_SLOWEST,
      "<RadioItem>"},
     {N_("/Peaks Falloff/Slow"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_PFALLOFF_SLOW,
      "/Peaks Falloff/Slowest"},
     {N_("/Peaks Falloff/Medium"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_PFALLOFF_MEDIUM,
      "/Peaks Falloff/Slowest"},
     {N_("/Peaks Falloff/Fast"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_PFALLOFF_FAST,
      "/Peaks Falloff/Slowest"},
     {N_("/Peaks Falloff/Fastest"), NULL, mainwin_vis_menu_callback, MAINWIN_VIS_PFALLOFF_FASTEST,
      "/Peaks Falloff/Slowest"},
     {N_("/Visualization plugins"), "<control>V", mainwin_vis_menu_callback, MAINWIN_VIS_PLUGINS,
      "<Item>"}};
#endif

static gint mainwin_vis_menu_entries_num = 0; /* GTK3: entries disabled */

/*
 * If you change the menu above change these defines also
 */

#define MAINWIN_VIS_MENU_VIS_MODE 1
#define MAINWIN_VIS_MENU_NUM_VIS_MODE 3
#define MAINWIN_VIS_MENU_ANALYZER_MODE 5
#define MAINWIN_VIS_MENU_NUM_ANALYZER_MODE 3
#define MAINWIN_VIS_MENU_ANALYZER_TYPE 9
#define MAINWIN_VIS_MENU_NUM_ANALYZER_TYPE 2
#define MAINWIN_VIS_MENU_ANALYZER_PEAKS 12
#define MAINWIN_VIS_MENU_SCOPE_MODE 14
#define MAINWIN_VIS_MENU_NUM_SCOPE_MODE 3
#define MAINWIN_VIS_MENU_WSHADEVU_MODE 18
#define MAINWIN_VIS_MENU_NUM_WSHADEVU_MODE 2
#define MAINWIN_VIS_MENU_REFRESH_RATE 21
#define MAINWIN_VIS_MENU_NUM_REFRESH_RATE 4
#define MAINWIN_VIS_MENU_AFALLOFF 26
#define MAINWIN_VIS_MENU_NUM_AFALLOFF 5
#define MAINWIN_VIS_MENU_PFALLOFF 32
#define MAINWIN_VIS_MENU_NUM_PFALLOFF 5

enum {
    MAINWIN_GENERAL_ABOUT,
    MAINWIN_GENERAL_PLAYFILE,
    MAINWIN_GENERAL_PLAYDIRECTORY,
    MAINWIN_GENERAL_PLAYLOCATION,
    MAINWIN_GENERAL_FILEINFO,
    MAINWIN_GENERAL_SHOWMWIN,
    MAINWIN_GENERAL_SHOWPLWIN,
    MAINWIN_GENERAL_SHOWEQWIN,
    MAINWIN_GENERAL_PREV,
    MAINWIN_GENERAL_PLAY,
    MAINWIN_GENERAL_PAUSE,
    MAINWIN_GENERAL_STOP,
    MAINWIN_GENERAL_NEXT,
    MAINWIN_GENERAL_STOPFADE,
    MAINWIN_GENERAL_BACK5SEC,
    MAINWIN_GENERAL_FWD5SEC,
    MAINWIN_GENERAL_START,
    MAINWIN_GENERAL_BACK10,
    MAINWIN_GENERAL_FWD10,
    MAINWIN_GENERAL_JTT,
    MAINWIN_GENERAL_JTF,
    MAINWIN_GENERAL_CQUEUE,
    MAINWIN_GENERAL_EXIT
};

void mainwin_general_menu_callback(gpointer cb_data, guint action, GtkWidget *w);

#if 0 /* GTK3: GtkItemFactory removed */
GtkItemFactoryEntry mainwin_general_menu_entries[] =
    {{N_("/About XMMS"), NULL, mainwin_general_menu_callback, MAINWIN_GENERAL_ABOUT, "<Item>"},
     {N_("/-"), NULL, NULL, 0, "<Separator>"},
     {N_("/Play File"), "L", mainwin_general_menu_callback, MAINWIN_GENERAL_PLAYFILE, "<Item>"},
     {N_("/Play Directory"), "<shift>L", mainwin_general_menu_callback,
      MAINWIN_GENERAL_PLAYDIRECTORY, "<Item>"},
     {N_("/Play Location"), "<control>L", mainwin_general_menu_callback,
      MAINWIN_GENERAL_PLAYLOCATION, "<Item>"},
     {N_("/View File Info"), "<control>3", mainwin_general_menu_callback, MAINWIN_GENERAL_FILEINFO,
      "<Item>"},
     {N_("/-"), NULL, NULL, 0, "<Separator>"},
     {N_("/Main Window"), "<alt>W", mainwin_general_menu_callback, MAINWIN_GENERAL_SHOWMWIN,
      "<ToggleItem>"},
     {N_("/Playlist Editor"), "<alt>E", mainwin_general_menu_callback, MAINWIN_GENERAL_SHOWPLWIN,
      "<ToggleItem>"},
     {N_("/Graphical EQ"), "<alt>G", mainwin_general_menu_callback, MAINWIN_GENERAL_SHOWEQWIN,
      "<ToggleItem>"},
     {N_("/-"), NULL, NULL, 0, "<Separator>"},
     {N_("/Options"), NULL, NULL, 0, "<Item>"},
     {N_("/Playback"), NULL, NULL, 0, "<Branch>"},
     {N_("/Playback/Previous"), "Z", mainwin_general_menu_callback, MAINWIN_GENERAL_PREV, "<Item>"},
     {N_("/Playback/Play"), "X", mainwin_general_menu_callback, MAINWIN_GENERAL_PLAY, "<Item>"},
     {N_("/Playback/Pause"), "C", mainwin_general_menu_callback, MAINWIN_GENERAL_PAUSE, "<Item>"},
     {N_("/Playback/Stop"), "V", mainwin_general_menu_callback, MAINWIN_GENERAL_STOP, "<Item>"},
     {N_("/Playback/Next"), "B", mainwin_general_menu_callback, MAINWIN_GENERAL_NEXT, "<Item>"},
     {N_("/Playback/-"), NULL, NULL, 0, "<Separator>"},
     /*      {N_("/Playback/Stop with
        Fadeout"),"<Shift>V",mainwin_general_menu_callback,MAINWIN_GENERAL_STOPFADE,"<Item>"}, */
     {N_("/Playback/Back 5 Seconds"), NULL, mainwin_general_menu_callback, MAINWIN_GENERAL_BACK5SEC,
      "<Item>"},
     {N_("/Playback/Fwd 5 Seconds"), NULL, mainwin_general_menu_callback, MAINWIN_GENERAL_FWD5SEC,
      "<Item>"},
     {N_("/Playback/Start of List"), "<control>Z", mainwin_general_menu_callback,
      MAINWIN_GENERAL_START, "<Item>"},
     {N_("/Playback/10 Tracks Back"), NULL, mainwin_general_menu_callback, MAINWIN_GENERAL_BACK10,
      "<Item>"},
     {N_("/Playback/10 Tracks Fwd"), NULL, mainwin_general_menu_callback, MAINWIN_GENERAL_FWD10,
      "<Item>"},
     {N_("/Playback/-"), NULL, NULL, 0, "<Separator>"},
     {N_("/Playback/Jump to Time"), "<control>J", mainwin_general_menu_callback,
      MAINWIN_GENERAL_JTT, "<Item>"},
     {N_("/Playback/Jump to File"), "J", mainwin_general_menu_callback, MAINWIN_GENERAL_JTF,
      "<Item>"},
     {N_("/Playback/Clear Queue"), "<shift>Q", mainwin_general_menu_callback,
      MAINWIN_GENERAL_CQUEUE, "<Item>"},
     {N_("/Visualization"), NULL, NULL, 0, "<Item>"},
     {N_("/-"), NULL, NULL, 0, "<Separator>"},
     {N_("/Exit"), "<control>Q", mainwin_general_menu_callback, MAINWIN_GENERAL_EXIT, "<Item>"}};
#endif

static const int mainwin_general_menu_entries_num = 0; /* GTK3: entries disabled */

static void make_xmms_dir(void)
{
    gchar *filename;

    filename = g_strconcat(g_get_home_dir(), "/.xmms", NULL);
    mkdir(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    g_free(filename);
    filename = g_strconcat(g_get_home_dir(), "/.xmms/Skins", NULL);
    mkdir(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    g_free(filename);
    filename = g_strconcat(g_get_home_dir(), "/.xmms/Plugins", NULL);
    mkdir(filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    g_free(filename);
}

static void read_config(void)
{
    ConfigFile *cfgfile;
    gchar *filename;
    gint i, length;

    memset(&cfg, 0, sizeof(Config));
    cfg.autoscroll = TRUE;
    cfg.player_x = 20;
    cfg.player_y = 20;
    cfg.always_on_top = FALSE;
    cfg.sticky = FALSE;
    cfg.always_show_cb = TRUE;
    cfg.convert_underscore = TRUE;
    cfg.convert_twenty = TRUE;
    cfg.show_numbers_in_pl = TRUE;
    cfg.snap_windows = TRUE;
    cfg.save_window_position = TRUE;
    cfg.dim_titlebar = TRUE;
    cfg.get_info_on_load = FALSE;
    cfg.get_info_on_demand = TRUE;
    cfg.eq_doublesize_linked = TRUE;
    cfg.eq_auto_level = TRUE; /* default on: auto-reduce preamp to prevent clipping (#14) */
    cfg.player_visible = TRUE;
    cfg.no_playlist_advance = FALSE;
    cfg.smooth_title_scroll = TRUE;
    cfg.random_skin_on_play = FALSE;
    cfg.mainwin_use_xfont = FALSE;
    cfg.use_pl_metadata = TRUE;

    cfg.playlist_x = 295;
    cfg.playlist_y = 20;
    cfg.playlist_width = 300;
    cfg.playlist_height = 232;
    cfg.playlist_transparent = FALSE;

    cfg.filesel_path = NULL;
    cfg.playlist_path = NULL;

    cfg.equalizer_x = 20;
    cfg.equalizer_y = 136;

    cfg.snap_distance = 10;
    cfg.pause_between_songs_time = 2;

    cfg.vis_type = VIS_ANALYZER;
    cfg.analyzer_mode = ANALYZER_NORMAL;
    cfg.analyzer_type = ANALYZER_BARS;
    cfg.analyzer_peaks = TRUE;
    cfg.scope_mode = SCOPE_DOT;
    cfg.vu_mode = VU_SMOOTH;
    cfg.vis_refresh = REFRESH_FULL;
    cfg.analyzer_falloff = FALLOFF_FAST;
    cfg.peaks_falloff = FALLOFF_SLOW;
    cfg.disabled_iplugins = NULL;
    cfg.enabled_gplugins = NULL;
    cfg.mouse_change = 8;

    cfg.gentitle_format = NULL;

    filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
    cfgfile = xmms_cfg_open_file(filename);
    if (cfgfile) {
        xmms_cfg_read_boolean(cfgfile, "xmms", "allow_multiple_instances",
                              &cfg.allow_multiple_instances);
        xmms_cfg_read_boolean(cfgfile, "xmms", "use_realtime", &cfg.use_realtime);
        xmms_cfg_read_boolean(cfgfile, "xmms", "always_show_cb", &cfg.always_show_cb);
        xmms_cfg_read_boolean(cfgfile, "xmms", "convert_underscore", &cfg.convert_underscore);
        xmms_cfg_read_boolean(cfgfile, "xmms", "convert_%20", &cfg.convert_twenty);
        xmms_cfg_read_boolean(cfgfile, "xmms", "show_numbers_in_pl", &cfg.show_numbers_in_pl);
        xmms_cfg_read_boolean(cfgfile, "xmms", "snap_windows", &cfg.snap_windows);
        xmms_cfg_read_boolean(cfgfile, "xmms", "save_window_positions", &cfg.save_window_position);
        xmms_cfg_read_boolean(cfgfile, "xmms", "dim_titlebar", &cfg.dim_titlebar);
        xmms_cfg_read_boolean(cfgfile, "xmms", "get_info_on_load", &cfg.get_info_on_load);
        xmms_cfg_read_boolean(cfgfile, "xmms", "get_info_on_demand", &cfg.get_info_on_demand);
        xmms_cfg_read_boolean(cfgfile, "xmms", "eq_doublesize_linked", &cfg.eq_doublesize_linked);
        xmms_cfg_read_boolean(cfgfile, "xmms", "eq_auto_level", &cfg.eq_auto_level);
        xmms_cfg_read_boolean(cfgfile, "xmms", "no_playlist_advance", &cfg.no_playlist_advance);
        xmms_cfg_read_boolean(cfgfile, "xmms", "sort_jump_to_file", &cfg.sort_jump_to_file);
        xmms_cfg_read_boolean(cfgfile, "xmms", "use_pl_metadata", &cfg.use_pl_metadata);
        xmms_cfg_read_boolean(cfgfile, "xmms", "smooth_title_scroll", &cfg.smooth_title_scroll);
        xmms_cfg_read_boolean(cfgfile, "xmms", "use_backslash_as_dir_delimiter",
                              &cfg.use_backslash_as_dir_delimiter);
        xmms_cfg_read_int(cfgfile, "xmms", "player_x", &cfg.player_x);
        xmms_cfg_read_int(cfgfile, "xmms", "player_y", &cfg.player_y);
        xmms_cfg_read_boolean(cfgfile, "xmms", "player_shaded", &cfg.player_shaded);
        xmms_cfg_read_boolean(cfgfile, "xmms", "player_visible", &cfg.player_visible);
        xmms_cfg_read_boolean(cfgfile, "xmms", "shuffle", &cfg.shuffle);
        xmms_cfg_read_boolean(cfgfile, "xmms", "repeat", &cfg.repeat);
        xmms_cfg_read_boolean(cfgfile, "xmms", "doublesize", &cfg.doublesize);
        xmms_cfg_read_boolean(cfgfile, "xmms", "autoscroll_songname", &cfg.autoscroll);
        xmms_cfg_read_int(cfgfile, "xmms", "timer_mode", &cfg.timer_mode);
        xmms_cfg_read_boolean(cfgfile, "xmms", "timer_minutes_only", &cfg.timer_minutes_only);
        xmms_cfg_read_int(cfgfile, "xmms", "vis_type", &cfg.vis_type);
        xmms_cfg_read_int(cfgfile, "xmms", "analyzer_mode", &cfg.analyzer_mode);
        xmms_cfg_read_int(cfgfile, "xmms", "analyzer_type", &cfg.analyzer_type);
        xmms_cfg_read_boolean(cfgfile, "xmms", "analyzer_peaks", &cfg.analyzer_peaks);
        xmms_cfg_read_int(cfgfile, "xmms", "scope_mode", &cfg.scope_mode);
        xmms_cfg_read_int(cfgfile, "xmms", "vu_mode", &cfg.vu_mode);
        xmms_cfg_read_int(cfgfile, "xmms", "vis_refresh_rate", &cfg.vis_refresh);
        xmms_cfg_read_int(cfgfile, "xmms", "analyzer_falloff", &cfg.analyzer_falloff);
        xmms_cfg_read_int(cfgfile, "xmms", "peaks_falloff", &cfg.peaks_falloff);
        xmms_cfg_read_int(cfgfile, "xmms", "playlist_x", &cfg.playlist_x);
        xmms_cfg_read_int(cfgfile, "xmms", "playlist_y", &cfg.playlist_y);
        xmms_cfg_read_int(cfgfile, "xmms", "playlist_width", &cfg.playlist_width);
        xmms_cfg_read_int(cfgfile, "xmms", "playlist_height", &cfg.playlist_height);
        xmms_cfg_read_boolean(cfgfile, "xmms", "playlist_shaded", &cfg.playlist_shaded);
        xmms_cfg_read_boolean(cfgfile, "xmms", "playlist_visible", &cfg.playlist_visible);
        xmms_cfg_read_boolean(cfgfile, "xmms", "playlist_transparent", &cfg.playlist_transparent);
        xmms_cfg_read_string(cfgfile, "xmms", "playlist_font", &cfg.playlist_font);
        xmms_cfg_read_boolean(cfgfile, "xmms", "use_fontsets", &cfg.use_fontsets);
        xmms_cfg_read_boolean(cfgfile, "xmms", "mainwin_use_xfont", &cfg.mainwin_use_xfont);
        xmms_cfg_read_string(cfgfile, "xmms", "mainwin_font", &cfg.mainwin_font);
        xmms_cfg_read_int(cfgfile, "xmms", "playlist_position", &cfg.playlist_position);
        xmms_cfg_read_int(cfgfile, "xmms", "equalizer_x", &cfg.equalizer_x);
        xmms_cfg_read_int(cfgfile, "xmms", "equalizer_y", &cfg.equalizer_y);
        xmms_cfg_read_int(cfgfile, "xmms", "snap_distance", &cfg.snap_distance);
        xmms_cfg_read_boolean(cfgfile, "xmms", "equalizer_visible", &cfg.equalizer_visible);
        xmms_cfg_read_boolean(cfgfile, "xmms", "equalizer_active", &cfg.equalizer_active);
        xmms_cfg_read_boolean(cfgfile, "xmms", "equalizer_shaded", &cfg.equalizer_shaded);
        xmms_cfg_read_boolean(cfgfile, "xmms", "equalizer_autoload", &cfg.equalizer_autoload);
        xmms_cfg_read_boolean(cfgfile, "xmms", "easy_move", &cfg.easy_move);
        xmms_cfg_read_float(cfgfile, "xmms", "equalizer_preamp", &cfg.equalizer_preamp);
        for (i = 0; i < 10; i++) {
            gchar eqtext[18];

            sprintf(eqtext, "equalizer_band%d", i);
            xmms_cfg_read_float(cfgfile, "xmms", eqtext, &cfg.equalizer_bands[i]);
        }
        xmms_cfg_read_string(cfgfile, "xmms", "eqpreset_default_file", &cfg.eqpreset_default_file);
        xmms_cfg_read_string(cfgfile, "xmms", "eqpreset_extension", &cfg.eqpreset_extension);
        xmms_cfg_read_string(cfgfile, "xmms", "skin", &cfg.skin);
        xmms_cfg_read_string(cfgfile, "xmms", "output_plugin", &cfg.outputplugin);
        xmms_cfg_read_string(cfgfile, "xmms", "enabled_gplugins", &cfg.enabled_gplugins);
        xmms_cfg_read_string(cfgfile, "xmms", "enabled_vplugins", &cfg.enabled_vplugins);
        xmms_cfg_read_string(cfgfile, "xmms", "enabled_eplugins", &cfg.enabled_eplugins);
        xmms_cfg_read_string(cfgfile, "xmms", "filesel_path", &cfg.filesel_path);
        xmms_cfg_read_string(cfgfile, "xmms", "playlist_path", &cfg.playlist_path);
        xmms_cfg_read_string(cfgfile, "xmms", "disabled_iplugins", &cfg.disabled_iplugins);
        xmms_cfg_read_boolean(cfgfile, "xmms", "use_eplugins", &cfg.use_eplugins);
        xmms_cfg_read_boolean(cfgfile, "xmms", "always_on_top", &cfg.always_on_top);
        xmms_cfg_read_boolean(cfgfile, "xmms", "sticky", &cfg.sticky);
        xmms_cfg_read_boolean(cfgfile, "xmms", "random_skin_on_play", &cfg.random_skin_on_play);
        xmms_cfg_read_boolean(cfgfile, "xmms", "pause_between_songs", &cfg.pause_between_songs);
        xmms_cfg_read_int(cfgfile, "xmms", "pause_between_songs_time",
                          &cfg.pause_between_songs_time);
        xmms_cfg_read_int(cfgfile, "xmms", "mouse_wheel_change", &cfg.mouse_change);
        xmms_cfg_read_boolean(cfgfile, "xmms", "show_wm_decorations", &cfg.show_wm_decorations);
        if (xmms_cfg_read_int(cfgfile, "xmms", "url_history_length", &length)) {
            for (i = 1; i <= length; i++) {
                gchar str[19], *temp;

                sprintf(str, "url_history%d", i);
                if (xmms_cfg_read_string(cfgfile, "xmms", str, &temp))
                    cfg.url_history = g_list_append(cfg.url_history, temp);
            }
        }
        xmms_cfg_read_string(cfgfile, "xmms", "generic_title_format", &cfg.gentitle_format);

        xmms_cfg_free(cfgfile);
    }

    if (cfg.playlist_font && strlen(cfg.playlist_font) == 0) {
        g_free(cfg.playlist_font);
        cfg.playlist_font = NULL;
    }
    if (cfg.mainwin_font && strlen(cfg.mainwin_font) == 0) {
        g_free(cfg.mainwin_font);
        cfg.mainwin_font = NULL;
    }
    if (cfg.playlist_font == NULL)
        cfg.playlist_font = g_strdup("-adobe-helvetica-bold-r-*-*-10-*");
    if (cfg.mainwin_font == NULL)
        cfg.mainwin_font = g_strdup("-adobe-helvetica-medium-r-*-*-8-*");
    if (cfg.gentitle_format == NULL)
        cfg.gentitle_format = g_strdup("%p - %t");
    if (cfg.outputplugin == NULL) {
#ifdef HAVE_OSS
        cfg.outputplugin = g_strdup_printf("%s/%s/libOSS.so", PLUGIN_DIR, plugin_dir_list[0]);
#elif defined(sun)
        cfg.outputplugin = g_strdup_printf("%s/%s/libSolaris.so", PLUGIN_DIR, plugin_dir_list[0]);
#else
        /*
         * FIXME: This implisitly means the output plugin that is first
         * in the alphabet will be used (usually the disk writer plugin)
         */
        cfg.outputplugin = g_strdup("");
#endif
    }
    if (cfg.eqpreset_default_file == NULL)
        cfg.eqpreset_default_file = g_strdup("dir_default.preset");
    if (cfg.eqpreset_extension == NULL)
        cfg.eqpreset_extension = g_strdup("preset");

    g_free(filename);
}

void save_config(void)
{
    GList *d_iplist, *node;
    gchar *temp;
    gchar *filename, *str;
    gint i;
    ConfigFile *cfgfile;

    if (cfg.disabled_iplugins)
        g_free(cfg.disabled_iplugins);
    if (disabled_iplugins && (g_list_length(disabled_iplugins) > 0)) {
        d_iplist = disabled_iplugins;
        cfg.disabled_iplugins = g_strdup(g_basename(((InputPlugin *)d_iplist->data)->filename));
        d_iplist = d_iplist->next;
        while (d_iplist != NULL) {
            temp = cfg.disabled_iplugins;
            cfg.disabled_iplugins =
                g_strconcat(temp, ",", g_basename(((InputPlugin *)d_iplist->data)->filename), NULL);
            g_free(temp);
            d_iplist = d_iplist->next;
        }
    } else
        cfg.disabled_iplugins = g_strdup("");
    filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
    cfgfile = xmms_cfg_open_file(filename);
    if (!cfgfile)
        cfgfile = xmms_cfg_new();
    xmms_cfg_write_boolean(cfgfile, "xmms", "allow_multiple_instances",
                           cfg.allow_multiple_instances);
    xmms_cfg_write_boolean(cfgfile, "xmms", "use_realtime", cfg.use_realtime);
    xmms_cfg_write_boolean(cfgfile, "xmms", "always_show_cb", cfg.always_show_cb);
    xmms_cfg_write_boolean(cfgfile, "xmms", "convert_underscore", cfg.convert_underscore);
    xmms_cfg_write_boolean(cfgfile, "xmms", "convert_%20", cfg.convert_twenty);
    xmms_cfg_write_boolean(cfgfile, "xmms", "show_numbers_in_pl", cfg.show_numbers_in_pl);
    xmms_cfg_write_boolean(cfgfile, "xmms", "snap_windows", cfg.snap_windows);
    xmms_cfg_write_boolean(cfgfile, "xmms", "save_window_positions", cfg.save_window_position);
    xmms_cfg_write_boolean(cfgfile, "xmms", "dim_titlebar", cfg.dim_titlebar);
    xmms_cfg_write_boolean(cfgfile, "xmms", "use_pl_metadata", cfg.use_pl_metadata);
    xmms_cfg_write_boolean(cfgfile, "xmms", "get_info_on_load", cfg.get_info_on_load);
    xmms_cfg_write_boolean(cfgfile, "xmms", "get_info_on_demand", cfg.get_info_on_demand);
    xmms_cfg_write_boolean(cfgfile, "xmms", "eq_doublesize_linked", cfg.eq_doublesize_linked);
    xmms_cfg_write_boolean(cfgfile, "xmms", "eq_auto_level", cfg.eq_auto_level);
    xmms_cfg_write_boolean(cfgfile, "xmms", "no_playlist_advance", cfg.no_playlist_advance);
    xmms_cfg_write_boolean(cfgfile, "xmms", "sort_jump_to_file", cfg.sort_jump_to_file);
    xmms_cfg_write_boolean(cfgfile, "xmms", "smooth_title_scroll", cfg.smooth_title_scroll);
    xmms_cfg_write_boolean(cfgfile, "xmms", "use_backslash_as_dir_delimiter",
                           cfg.use_backslash_as_dir_delimiter);
    dock_get_widget_pos(mainwin, &cfg.player_x, &cfg.player_y);
    xmms_cfg_write_int(cfgfile, "xmms", "player_x", cfg.player_x);
    xmms_cfg_write_int(cfgfile, "xmms", "player_y", cfg.player_y);
    xmms_cfg_write_boolean(cfgfile, "xmms", "player_shaded", cfg.player_shaded);
    xmms_cfg_write_boolean(cfgfile, "xmms", "player_visible", cfg.player_visible);
    xmms_cfg_write_boolean(cfgfile, "xmms", "shuffle", mainwin_shuffle->tb_selected);
    xmms_cfg_write_boolean(cfgfile, "xmms", "repeat", mainwin_repeat->tb_selected);
    xmms_cfg_write_boolean(cfgfile, "xmms", "doublesize", cfg.doublesize);
    xmms_cfg_write_boolean(cfgfile, "xmms", "autoscroll_songname", cfg.autoscroll);
    xmms_cfg_write_int(cfgfile, "xmms", "timer_mode", cfg.timer_mode);
    xmms_cfg_write_boolean(cfgfile, "xmms", "timer_minutes_only", cfg.timer_minutes_only);
    xmms_cfg_write_int(cfgfile, "xmms", "vis_type", cfg.vis_type);
    xmms_cfg_write_int(cfgfile, "xmms", "analyzer_mode", cfg.analyzer_mode);
    xmms_cfg_write_int(cfgfile, "xmms", "analyzer_type", cfg.analyzer_type);
    xmms_cfg_write_boolean(cfgfile, "xmms", "analyzer_peaks", cfg.analyzer_peaks);
    xmms_cfg_write_int(cfgfile, "xmms", "scope_mode", cfg.scope_mode);
    xmms_cfg_write_int(cfgfile, "xmms", "vu_mode", cfg.vu_mode);
    xmms_cfg_write_int(cfgfile, "xmms", "vis_refresh_rate", cfg.vis_refresh);
    xmms_cfg_write_int(cfgfile, "xmms", "analyzer_falloff", cfg.analyzer_falloff);
    xmms_cfg_write_int(cfgfile, "xmms", "peaks_falloff", cfg.peaks_falloff);
    dock_get_widget_pos(playlistwin, &cfg.playlist_x, &cfg.playlist_y);
    xmms_cfg_write_int(cfgfile, "xmms", "playlist_x", cfg.playlist_x);
    xmms_cfg_write_int(cfgfile, "xmms", "playlist_y", cfg.playlist_y);
    xmms_cfg_write_int(cfgfile, "xmms", "playlist_width", cfg.playlist_width);
    xmms_cfg_write_int(cfgfile, "xmms", "playlist_height", cfg.playlist_height);
    xmms_cfg_write_boolean(cfgfile, "xmms", "playlist_shaded", cfg.playlist_shaded);
    xmms_cfg_write_boolean(cfgfile, "xmms", "playlist_visible", cfg.playlist_visible);
    xmms_cfg_write_boolean(cfgfile, "xmms", "playlist_transparent", cfg.playlist_transparent);
    xmms_cfg_write_string(cfgfile, "xmms", "playlist_font", cfg.playlist_font);
    xmms_cfg_write_boolean(cfgfile, "xmms", "use_fontsets", cfg.use_fontsets);
    xmms_cfg_write_boolean(cfgfile, "xmms", "mainwin_use_xfont", cfg.mainwin_use_xfont);
    xmms_cfg_write_string(cfgfile, "xmms", "mainwin_font", cfg.mainwin_font);
    xmms_cfg_write_int(cfgfile, "xmms", "playlist_position", get_playlist_position());
    dock_get_widget_pos(equalizerwin, &cfg.equalizer_x, &cfg.equalizer_y);
    xmms_cfg_write_int(cfgfile, "xmms", "equalizer_x", cfg.equalizer_x);
    xmms_cfg_write_int(cfgfile, "xmms", "equalizer_y", cfg.equalizer_y);
    xmms_cfg_write_int(cfgfile, "xmms", "snap_distance", cfg.snap_distance);
    xmms_cfg_write_boolean(cfgfile, "xmms", "equalizer_visible", cfg.equalizer_visible);
    xmms_cfg_write_boolean(cfgfile, "xmms", "equalizer_shaded", cfg.equalizer_shaded);
    xmms_cfg_write_boolean(cfgfile, "xmms", "equalizer_active", cfg.equalizer_active);
    xmms_cfg_write_boolean(cfgfile, "xmms", "equalizer_autoload", cfg.equalizer_autoload);
    xmms_cfg_write_boolean(cfgfile, "xmms", "easy_move", cfg.easy_move);
    xmms_cfg_write_boolean(cfgfile, "xmms", "use_eplugins", cfg.use_eplugins);
    xmms_cfg_write_boolean(cfgfile, "xmms", "always_on_top", cfg.always_on_top);
    xmms_cfg_write_boolean(cfgfile, "xmms", "sticky", cfg.sticky);
    xmms_cfg_write_float(cfgfile, "xmms", "equalizer_preamp", cfg.equalizer_preamp);
    xmms_cfg_write_boolean(cfgfile, "xmms", "random_skin_on_play", cfg.random_skin_on_play);
    xmms_cfg_write_boolean(cfgfile, "xmms", "pause_between_songs", cfg.pause_between_songs);
    xmms_cfg_write_int(cfgfile, "xmms", "pause_between_songs_time", cfg.pause_between_songs_time);
    xmms_cfg_write_int(cfgfile, "xmms", "mouse_wheel_change", cfg.mouse_change);
    xmms_cfg_write_boolean(cfgfile, "xmms", "show_wm_decorations", cfg.show_wm_decorations);
    xmms_cfg_write_string(cfgfile, "xmms", "eqpreset_default_file", cfg.eqpreset_default_file);
    xmms_cfg_write_string(cfgfile, "xmms", "eqpreset_extension", cfg.eqpreset_extension);
    for (i = 0; i < 10; i++) {
        str = g_strdup_printf("equalizer_band%d", i);
        xmms_cfg_write_float(cfgfile, "xmms", str, cfg.equalizer_bands[i]);
        g_free(str);
    }
    if (skin->path)
        xmms_cfg_write_string(cfgfile, "xmms", "skin", skin->path);
    else
        xmms_cfg_remove_key(cfgfile, "xmms", "skin");
    if (get_current_output_plugin())
        xmms_cfg_write_string(cfgfile, "xmms", "output_plugin",
                              get_current_output_plugin()->filename);
    else
        xmms_cfg_remove_key(cfgfile, "xmms", "output_plugin");

    str = general_stringify_enabled_list();
    if (str) {
        xmms_cfg_write_string(cfgfile, "xmms", "enabled_gplugins", str);
        g_free(str);
    } else
        xmms_cfg_remove_key(cfgfile, "xmms", "enabled_gplugins");

    str = vis_stringify_enabled_list();
    if (str) {
        xmms_cfg_write_string(cfgfile, "xmms", "enabled_vplugins", str);
        g_free(str);
    } else
        xmms_cfg_remove_key(cfgfile, "xmms", "enabled_vplugins");

    str = effect_stringify_enabled_list();
    if (str) {
        xmms_cfg_write_string(cfgfile, "xmms", "enabled_eplugins", str);
        g_free(str);
    } else
        xmms_cfg_remove_key(cfgfile, "xmms", "enabled_eplugins");

    xmms_cfg_write_string(cfgfile, "xmms", "disabled_iplugins", cfg.disabled_iplugins);
    if (cfg.filesel_path)
        xmms_cfg_write_string(cfgfile, "xmms", "filesel_path", cfg.filesel_path);
    if (cfg.playlist_path)
        xmms_cfg_write_string(cfgfile, "xmms", "playlist_path", cfg.playlist_path);
    xmms_cfg_write_int(cfgfile, "xmms", "url_history_length", g_list_length(cfg.url_history));
    for (node = cfg.url_history, i = 1; node; node = g_list_next(node), i++) {
        str = g_strdup_printf("url_history%d", i);
        xmms_cfg_write_string(cfgfile, "xmms", str, node->data);
        g_free(str);
    }
    xmms_cfg_write_string(cfgfile, "xmms", "generic_title_format", cfg.gentitle_format);

    xmms_cfg_write_file(cfgfile, filename);
    xmms_cfg_free(cfgfile);

    g_free(filename);
    filename = g_strconcat(g_get_home_dir(), "/.xmms/xmms.m3u", NULL);
    playlist_save(filename, FALSE);
    g_free(filename);
}

gchar *xmms_get_gentitle_format(void)
{
    return cfg.gentitle_format;
}

void mainwin_set_always_on_top(gboolean always)
{
    if (mi_opt_always)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi_opt_always),
                                       mainwin_menurow->mr_always_selected);
}

void mainwin_set_shape_mask(void)
{
    cairo_region_t *region;

    if (!cfg.player_visible || cfg.show_wm_decorations)
        return;

    /* GTK3: apply the skin mask via cairo_region_t. */
    region = skin_get_mask(SKIN_MASK_MAIN, cfg.doublesize, cfg.player_shaded);
    gtk_widget_shape_combine_region(mainwin, region);
}


void set_doublesize(gboolean ds)
{
    gint height;

    cfg.doublesize = ds;

    if (cfg.player_shaded)
        height = 14;
    else
        height = 116;

    mainwin_set_shape_mask();
    if (cfg.doublesize) {
        dock_resize(dock_window_list, mainwin, 550, height * 2);
        /* TODO(#gtk3): gdk_window_set_back_pixmap removed */
    } else {
        dock_resize(dock_window_list, mainwin, 275, height);
        /* TODO(#gtk3): gdk_window_set_back_pixmap removed */
    }
    draw_main_window(TRUE);
    vis_set_doublesize(mainwin_vis, ds);

    if (cfg.eq_doublesize_linked)
        equalizerwin_set_doublesize(ds);
}

void mainwin_set_shade(gboolean shaded)
{
    if (mi_opt_ws)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi_opt_ws), shaded);
}

void mainwin_set_shade_menu_cb(gboolean shaded)
{
    cfg.player_shaded = shaded;

    mainwin_set_shape_mask();
    if (shaded) {
        dock_shade(dock_window_list, mainwin, 14 * (cfg.doublesize + 1));

        show_widget(mainwin_svis);
        vis_clear_data(mainwin_vis);

        show_widget(mainwin_srew);
        show_widget(mainwin_splay);
        show_widget(mainwin_spause);
        show_widget(mainwin_sstop);
        show_widget(mainwin_sfwd);
        show_widget(mainwin_seject);

        show_widget(mainwin_stime_min);
        show_widget(mainwin_stime_sec);

        if (get_input_playing() && playlist_get_current_length() != -1)
            show_widget(mainwin_sposition);

        mainwin_shade->pb_ny = mainwin_shade->pb_py = 27;

    } else {
        dock_shade(dock_window_list, mainwin, 116 * (cfg.doublesize + 1));

        hide_widget(mainwin_svis);
        svis_clear_data(mainwin_svis);

        hide_widget(mainwin_srew);
        hide_widget(mainwin_splay);
        hide_widget(mainwin_spause);
        hide_widget(mainwin_sstop);
        hide_widget(mainwin_sfwd);
        hide_widget(mainwin_seject);

        hide_widget(mainwin_stime_min);
        hide_widget(mainwin_stime_sec);
        hide_widget(mainwin_sposition);

        mainwin_shade->pb_ny = mainwin_shade->pb_py = 18;
    }

    draw_main_window(TRUE);
}

enum { MAINWIN_VIS_ACTIVE_MAINWIN, MAINWIN_VIS_ACTIVE_PLAYLISTWIN };

void mainwin_vis_set_active_vis(gint new_vis)
{
    switch (new_vis) {
    case MAINWIN_VIS_ACTIVE_MAINWIN:
        playlistwin_vis_disable();
        active_vis = mainwin_vis;
        break;
    case MAINWIN_VIS_ACTIVE_PLAYLISTWIN:
        playlistwin_vis_enable();
        active_vis = playlistwin_vis;
        break;
    }
}

void mainwin_vis_set_refresh(RefreshRate rate)
{
    cfg.vis_refresh = rate;
}

void mainwin_vis_set_afalloff(FalloffSpeed speed)
{
    cfg.analyzer_falloff = speed;
}

void mainwin_vis_set_pfalloff(FalloffSpeed speed)
{
    cfg.peaks_falloff = speed;
}

void mainwin_vis_set_analyzer_mode(AnalyzerMode mode)
{
    cfg.analyzer_mode = mode;
}

void mainwin_vis_set_analyzer_type(AnalyzerType mode)
{
    cfg.analyzer_type = mode;
}

void mainwin_vis_set_type(VisType mode)
{
    GtkWidget *mi = NULL;
    if (mode == VIS_ANALYZER)
        mi = mi_vis_analyzer;
    else if (mode == VIS_SCOPE)
        mi = mi_vis_scope;
    else
        mi = mi_vis_off;
    if (mi)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), TRUE);
}

static void mainwin_vis_set_type_menu_cb(VisType mode)
{
    cfg.vis_type = mode;

    if (mode == VIS_OFF) {
        if (cfg.player_shaded && cfg.player_visible)
            svis_clear(mainwin_svis);
        else
            vis_clear(active_vis);
    }
    if (mode == VIS_ANALYZER) {
        vis_clear_data(active_vis);
        svis_clear_data(mainwin_svis);
    }
}

void mainwin_menubtn_cb(void)
{
    gint x, y;
    dock_get_widget_pos(mainwin, &x, &y);
    util_item_factory_popup(mainwin_general_menu, x + 6 * (1 + cfg.doublesize),
                            y + 14 * (1 + cfg.doublesize), 1, GDK_CURRENT_TIME);
}

void mainwin_minimize_cb(void)
{
    Window xwindow;

    if (!gtk_widget_get_window(mainwin))
        return;

    xwindow = gdk_x11_window_get_xid(gtk_widget_get_window(mainwin));
    XIconifyWindow(gdk_x11_get_default_xdisplay(), xwindow,
                   DefaultScreen(gdk_x11_get_default_xdisplay()));
}

void mainwin_shade_toggle(void)
{
    mainwin_set_shade(!cfg.player_shaded);
}

void mainwin_quit_cb(void)
{
    /* Destroy vis-plugin windows NOW, before input_stop() which can block for
     * several seconds draining the audio ring buffer.  Without this, the vis
     * windows stay visible and appear orphaned to the user while the process
     * is actually busy draining audio. */
    vis_disable_all();

    input_stop();
    gtk_widget_hide(equalizerwin);
    gtk_widget_hide(playlistwin);
    gtk_widget_hide(mainwin);
    /* gdk_flush() no-op in GTK3 */
    util_dump_menu_rc();
    g_source_remove(mainwin_timeout_tag);
    if (mainwin_tick_id) {
        gtk_widget_remove_tick_callback(mainwin, mainwin_tick_id);
        mainwin_tick_id = 0;
    }
    util_set_cursor(NULL);
    save_config();
    cleanup_ctrlsocket();
    playlist_stop_get_info_thread();
    playlist_clear();
    cleanup_plugins();
    sm_cleanup();
    exit(0);
}

void mainwin_destroy(GtkWidget *widget, gpointer data)
{
    mainwin_quit_cb();
}

void draw_mainwin_titlebar(int focus)
{
    if (focus || !cfg.dim_titlebar)
        skin_draw_pixmap(mainwin_cr, SKIN_TITLEBAR, 27, 29 * cfg.player_shaded, 0, 0, 275, 14);
    else
        skin_draw_pixmap(mainwin_cr, SKIN_TITLEBAR, 27, (27 * cfg.player_shaded) + 15, 0, 0, 275,
                         14);
}

void draw_main_window(gboolean force)
{
    GList *wl;
    Widget *w;
    gboolean redraw;

    if (!cfg.player_visible)
        return;
    lock_widget_list(mainwin_wlist);
    if (force) {
        skin_draw_pixmap(mainwin_cr, SKIN_MAIN, 0, 0, 0, 0, 275, cfg.player_shaded ? 14 : 116);
        draw_mainwin_titlebar(mainwin_focus);
        draw_widget_list(mainwin_wlist, &redraw, TRUE);
    } else
        draw_widget_list(mainwin_wlist, &redraw, FALSE);

    if (redraw || force) {
        if (!force) {
            wl = mainwin_wlist;
            while (wl) {
                w = (Widget *)wl->data;
                if (w->redraw && w->visible)
                    w->redraw = FALSE;
                wl = wl->next;
            }
        }
        if (cfg.doublesize && mainwin_bg_dblsize) {
            cairo_t *cr = cairo_create(mainwin_bg_dblsize);

            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
            cairo_paint(cr);
            cairo_scale(cr, 2.0, 2.0);
            cairo_set_source_surface(cr, mainwin_bg, 0, 0);
            cairo_paint(cr);
            cairo_destroy(cr);
        }
        gtk_widget_queue_draw(mainwin);
    }
    unlock_widget_list(mainwin_wlist);
}

static void mainwin_update_title_text(void)
{
    char *tmp = NULL;

    tmp = playlist_get_info_text();

    pthread_mutex_lock(&title_mutex);
    g_free(mainwin_title_text);
    if (tmp != NULL) {
        mainwin_title_text = g_strdup_printf("XMMS - %s", tmp);
        g_free(tmp);
    } else
        mainwin_title_text = g_strdup("XMMS");
    pthread_mutex_unlock(&title_mutex);
}

void mainwin_set_info_text(void)
{
    char *text;

    if (get_input_playing())
        mainwin_update_title_text();

    if (mainwin_info_text_locked)
        return;

    if ((text = input_get_info_text()) != NULL) {
        textbox_set_text(mainwin_info, text);
    } else if ((text = playlist_get_info_text()) != NULL) {
        textbox_set_text(mainwin_info, text);
        g_free(text);
    } else {
        text = g_strdup_printf("%s %s", PACKAGE, VERSION);
        textbox_set_text(mainwin_info, text);
        g_free(text);
    }
}

void mainwin_lock_info_text(char *text)
{
    mainwin_info_text_locked = TRUE;
    textbox_set_text(mainwin_info, text);
}

void mainwin_release_info_text(void)
{
    mainwin_info_text_locked = FALSE;
    mainwin_set_info_text();
}

void mainwin_set_song_info(int rate, int freq, int nch)
{
    bitrate = rate;
    frequency = freq;
    numchannels = nch;
    mainwin_set_info_text();
    /* Refs #37: notify MPRIS2 clients of track/state change */
    mpris2_update_state();
}

void mainwin_get_song_info(int *rate, int *freq, int *nch)
{
    *rate = bitrate;
    *freq = frequency;
    *nch = numchannels;
}

static void mainwin_update_song_info(void)
{
    char text[10];

    if (bitrate != -1) {
        int rate = bitrate / 1000;
        if (rate < 1000) {
            sprintf(text, "%3d", rate);
            textbox_set_text(mainwin_rate_text, text);
        } else {
            rate /= 100;
            sprintf(text, "%2dH", rate);
            textbox_set_text(mainwin_rate_text, text);
        }
    } else
        textbox_set_text(mainwin_rate_text, "VBR");

    sprintf(text, "%2d", frequency / 1000);
    textbox_set_text(mainwin_freq_text, text);
    monostereo_set_num_channels(mainwin_monostereo, numchannels);

    show_widget(mainwin_minus_num);
    show_widget(mainwin_10min_num);
    show_widget(mainwin_min_num);
    show_widget(mainwin_10sec_num);
    show_widget(mainwin_sec_num);
    if (!get_input_paused())
        playstatus_set_status(mainwin_playstatus, STATUS_PLAY);
    if (playlist_get_current_length() != -1) {
        if (cfg.player_shaded)
            show_widget(mainwin_sposition);
        show_widget(mainwin_position);
    } else {
        hide_widget(mainwin_position);
        hide_widget(mainwin_sposition);
        mainwin_force_redraw = TRUE;
    }
}

/* Refs #37: notify MPRIS2 clients that playback stopped */
static void _mpris2_on_stop(void)
{
    mpris2_update_state();
}

void mainwin_clear_song_info(void)
{
    pthread_mutex_lock(&title_mutex);
    g_free(mainwin_title_text);
    mainwin_title_text = NULL;
    pthread_mutex_unlock(&title_mutex);

    bitrate = 0;
    frequency = 0;
    numchannels = 0;

    mainwin_position->hs_pressed = FALSE;
    mainwin_sposition->hs_pressed = FALSE;
    textbox_set_text(mainwin_rate_text, "   ");
    textbox_set_text(mainwin_freq_text, "  ");
    monostereo_set_num_channels(mainwin_monostereo, 0);
    playstatus_set_status(mainwin_playstatus, STATUS_STOP);
    hide_widget(mainwin_minus_num);
    hide_widget(mainwin_10min_num);
    hide_widget(mainwin_min_num);
    hide_widget(mainwin_10sec_num);
    hide_widget(mainwin_sec_num);
    textbox_set_text(mainwin_stime_min, "   ");
    textbox_set_text(mainwin_stime_sec, "  ");
    hide_widget(mainwin_position);
    hide_widget(mainwin_sposition);
    playlistwin_hide_timer();
    draw_main_window(TRUE);
    vis_clear(active_vis);
    gtk_window_set_title(GTK_WINDOW(mainwin), "XMMS");
}

void mainwin_disable_seekbar(void)
{
    /*
     * We dont call draw_main_window() here so this will not
     * remove them visually.  It will only prevent us from sending
     * any seek calls to the input plugin before the input plugin
     * calls ->set_info().
     */
    hide_widget(mainwin_position);
    hide_widget(mainwin_sposition);
}

void mainwin_release(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
    /* GTK3: ungrab pointer on button release */
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    /* gdk_flush() no-op in GTK3 */

    if (dock_is_moving(mainwin)) {
        dock_move_release(mainwin);
    }
    if (mainwin_menurow->mr_doublesize_selected) {
        event->x /= 2;
        event->y /= 2;
    }
    handle_release_cb(mainwin_wlist, widget, event);

    draw_main_window(FALSE);
}

void mainwin_motion(GtkWidget *widget, GdkEventMotion *event, gpointer callback_data)
{
    XEvent ev;
    gint i = 0;

    XSync(gdk_x11_get_default_xdisplay(), False);

    while (XCheckTypedEvent(gdk_x11_get_default_xdisplay(), MotionNotify, &ev)) {
        event->x = ev.xmotion.x;
        event->y = ev.xmotion.y;
        i++;
    }
    if (cfg.doublesize) {
        event->x /= 2;
        event->y /= 2;
    }
    if (dock_is_moving(mainwin)) {
        dock_move_motion(mainwin, event);
    } else {
        handle_motion_cb(mainwin_wlist, widget, event);
        draw_main_window(FALSE);
    }
    /* gdk_flush() no-op in GTK3 */
}

static gboolean inside_sensitive_widgets(gint x, gint y)
{
    return (inside_widget(x, y, mainwin_menubtn) || inside_widget(x, y, mainwin_minimize) ||
            inside_widget(x, y, mainwin_shade) || inside_widget(x, y, mainwin_close) ||
            inside_widget(x, y, mainwin_rew) || inside_widget(x, y, mainwin_play) ||
            inside_widget(x, y, mainwin_pause) || inside_widget(x, y, mainwin_stop) ||
            inside_widget(x, y, mainwin_fwd) || inside_widget(x, y, mainwin_eject) ||
            inside_widget(x, y, mainwin_shuffle) || inside_widget(x, y, mainwin_repeat) ||
            inside_widget(x, y, mainwin_pl) || inside_widget(x, y, mainwin_eq) ||
            inside_widget(x, y, mainwin_info) || inside_widget(x, y, mainwin_menurow) ||
            inside_widget(x, y, mainwin_volume) || inside_widget(x, y, mainwin_balance) ||
            (inside_widget(x, y, mainwin_position) && ((Widget *)mainwin_position)->visible) ||
            inside_widget(x, y, mainwin_minus_num) || inside_widget(x, y, mainwin_10min_num) ||
            inside_widget(x, y, mainwin_min_num) || inside_widget(x, y, mainwin_10sec_num) ||
            inside_widget(x, y, mainwin_sec_num) || inside_widget(x, y, mainwin_vis) ||
            inside_widget(x, y, mainwin_minimize) || inside_widget(x, y, mainwin_shade) ||
            inside_widget(x, y, mainwin_close) || inside_widget(x, y, mainwin_menubtn) ||
            inside_widget(x, y, mainwin_sposition) || inside_widget(x, y, mainwin_stime_min) ||
            inside_widget(x, y, mainwin_stime_sec) || inside_widget(x, y, mainwin_srew) ||
            inside_widget(x, y, mainwin_splay) || inside_widget(x, y, mainwin_spause) ||
            inside_widget(x, y, mainwin_sstop) || inside_widget(x, y, mainwin_sfwd) ||
            inside_widget(x, y, mainwin_seject) || inside_widget(x, y, mainwin_svis) ||
            inside_widget(x, y, mainwin_about));
}

/* GTK3: scroll-event replaces the old button==4/5 hack for mouse-wheel volume.
 * GDK stopped synthesising button 4/5 press events for scroll wheels in GTK3. */
static gboolean mainwin_scroll_cb(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
    int d = cfg.mouse_change;

    if (event->direction == GDK_SCROLL_DOWN ||
        (event->direction == GDK_SCROLL_SMOOTH && event->delta_y > 0.0))
        d = -d;
    else if (event->direction == GDK_SCROLL_UP ||
             (event->direction == GDK_SCROLL_SMOOTH && event->delta_y < 0.0))
        ; /* d unchanged — scroll up raises volume */
    else
        return FALSE;

    mainwin_set_volume_diff(d);
    return TRUE;
}

void mainwin_press(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
    gboolean grab = TRUE;

    if (cfg.doublesize) {
        /*
         * A hack to make doublesize transparent to callbacks.
         * We should make a copy of this data instead of
         * tampering with the data we get from gtk+
         */
        event->x /= 2;
        event->y /= 2;
    }

    if ((event->button == 4 || event->button == 5) && event->type == GDK_BUTTON_PRESS) {
        /* Handle mouse wheel events */
        int d = cfg.mouse_change;
        if (event->button == 5)
            d *= -1;
        mainwin_set_volume_diff(d);
    }

    if ((event->button == 6 || event->button == 7) && event->type == GDK_BUTTON_PRESS &&
        playlist_get_current_length() != -1) {
        /* Horizontal scrolling seeks */
        int d = cfg.mouse_change * 1000;
        if (event->button == 6)
            d *= -1;

        input_seek(CLAMP(input_get_time() + d, 0, playlist_get_current_length()) / 1000);
    }

    if (event->button == 1 && event->type == GDK_BUTTON_PRESS &&
        !inside_sensitive_widgets(event->x, event->y) && (cfg.easy_move || event->y < 14)) {
        if (0 && hint_move_resize_available()) {
            hint_move_resize(mainwin, event->x_root, event->y_root, TRUE);
            grab = FALSE;
        } else {
            gdk_window_raise(gtk_widget_get_window(mainwin));
            equalizerwin_raise();
            playlistwin_raise();
            dock_move_press(dock_window_list, mainwin, event, TRUE);
        }
    } else if (event->button == 1 && event->type == GDK_2BUTTON_PRESS && event->y < 14 &&
               !inside_sensitive_widgets(event->x, event->y)) {
        mainwin_set_shade(!cfg.player_shaded);
        if (dock_is_moving(mainwin))
            dock_move_release(mainwin);
    } else if (event->button == 1 && event->type == GDK_2BUTTON_PRESS &&
               inside_widget(event->x, event->y, mainwin_info)) {
        playlist_fileinfo_current();
    } else {
        handle_press_cb(mainwin_wlist, widget, event);
        draw_main_window(FALSE);
    }

    if ((event->button == 1) && event->type != GDK_2BUTTON_PRESS &&
        (inside_widget(event->x, event->y, mainwin_vis) ||
         inside_widget(event->x, event->y, mainwin_svis))) {
        cfg.vis_type++;
        if (cfg.vis_type > VIS_OFF)
            cfg.vis_type = VIS_ANALYZER;
        mainwin_vis_set_type(cfg.vis_type);
    }
    if (event->button == 3) {
        if (inside_widget(event->x, event->y, mainwin_info)) {
            util_item_factory_popup(mainwin_songname_menu, event->x_root, event->y_root, 3,
                                    event->time);
            grab = FALSE;
        } else if (inside_widget(event->x, event->y, mainwin_vis) ||
                   inside_widget(event->x, event->y, mainwin_svis)) {
            util_item_factory_popup(mainwin_vis_menu, event->x_root, event->y_root, 3, event->time);
            grab = FALSE;
        } else {
            /*
             * Pop up the main menu a few pixels down.
             * This will avoid that anything is selected
             * if one right-clicks to focus the window
             * without raising it.
             */
            util_item_factory_popup(mainwin_general_menu, event->x_root, event->y_root + 2, 3,
                                    event->time);
            grab = FALSE;
        }
    }
    if (event->button == 1) {
        if ((event->x > 35 && event->x < 100 && event->y > 25 && event->y < 40) ||
            inside_widget(event->x, event->y, mainwin_stime_min) ||
            inside_widget(event->x, event->y, mainwin_stime_sec)) {
            if (cfg.timer_mode == TIMER_ELAPSED)
                set_timer_mode(TIMER_REMAINING);
            else
                set_timer_mode(TIMER_ELAPSED);
        }
    }
    if (grab)
        gdk_pointer_grab(gtk_widget_get_window(mainwin), FALSE,
                         GDK_BUTTON_MOTION_MASK | GDK_BUTTON_RELEASE_MASK, NULL, NULL,
                         GDK_CURRENT_TIME);
}

void mainwin_focus_in(GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
    mainwin_focus = 1;
    /* Raise all visible sibling skin windows together — Refs #25 */
    playlistwin_raise();
    equalizerwin_raise();
    /* Raise any visible visualization plugin windows — Refs #34 */
    {
        GList *toplevels = gtk_window_list_toplevels();
        GList *l;
        for (l = toplevels; l != NULL; l = l->next) {
            GtkWindow *w = GTK_WINDOW(l->data);
            if (g_object_get_data(G_OBJECT(w), "vis-chrome-bar") != NULL &&
                gtk_widget_is_visible(GTK_WIDGET(w))) {
                gtk_window_present(w);
            }
        }
        g_list_free(toplevels);
    }
    mainwin_menubtn->pb_allow_draw = TRUE;
    mainwin_minimize->pb_allow_draw = TRUE;
    mainwin_shade->pb_allow_draw = TRUE;
    mainwin_close->pb_allow_draw = TRUE;
    draw_main_window(TRUE);
}

void mainwin_focus_out(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
    mainwin_focus = 0;
    /* GTK3: keep buttons visible even when unfocused — invisible-but-clickable is
     * dangerous with no WM decorations. Focus dimming is cosmetic only. */
    draw_main_window(TRUE);
}

gboolean mainwin_keypress(GtkWidget *w, GdkEventKey *event, gpointer data)
{

    /* Ignore keypresses with modifiers to avoid stealing Ctrl+Z etc. */
    if (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
        return FALSE;

    switch (event->keyval) {
    /* Classic Winamp keyboard shortcuts — Refs #37 */
    case GDK_KEY_z:
        playlist_prev();
        break;
    case GDK_KEY_x:
        if (get_input_paused())
            input_pause(); /* unpause */
        else if (get_playlist_length())
            playlist_play();
        break;
    case GDK_KEY_c:
        input_pause();
        break;
    case GDK_KEY_v:
        mainwin_stop_pushed();
        break;
    case GDK_KEY_b:
        playlist_next();
        break;
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
        mainwin_set_volume_diff(2);
        break;
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
        mainwin_set_volume_diff(-2);
        break;
    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
        if (playlist_get_current_length() != -1)
            input_seek(CLAMP(input_get_time() - 5000, 0, playlist_get_current_length()) / 1000);
        break;
    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
        if (playlist_get_current_length() != -1)
            input_seek(CLAMP(input_get_time() + 5000, 0, playlist_get_current_length()) / 1000);
        break;
    default:
        break;
    }

    return TRUE;
}

void mainwin_jump_to_time_cb(GtkWidget *widget, GtkWidget *entry)
{
    guint min = 0, sec = 0, params, time;
    gchar timestr[6];

    strcpy(timestr, gtk_entry_get_text(GTK_ENTRY(entry)));

    params = sscanf(timestr, "%u:%u", &min, &sec);
    if (params == 2)
        time = (min * 60) + sec;
    else if (params == 1)
        time = min;
    else
        return;

    if (playlist_get_current_length() > -1 && time <= (playlist_get_current_length() / 1000)) {
        input_seek(time);
        gtk_widget_destroy(mainwin_jtt);
    }
}

void mainwin_jump_to_time(void)
{
    GtkWidget *vbox, *frame, *vbox_inside, *hbox_new, *hbox_total;
    GtkWidget *time_entry, *label, *bbox, *jump, *cancel;
    guint len, tindex;
    gchar timestr[10];

    if (!get_input_playing())
        return;

    mainwin_jtt = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mainwin_jtt), _("Jump to time"));
    gtk_window_set_resizable(GTK_WINDOW(mainwin_jtt), FALSE);
    gtk_window_set_transient_for(GTK_WINDOW(mainwin_jtt), GTK_WINDOW(mainwin));
    g_signal_connect(G_OBJECT(mainwin_jtt), "destroy", G_CALLBACK(on_widget_destroyed),
                     &mainwin_jtt);
    g_signal_connect(G_OBJECT(mainwin_jtt), "key_press_event", G_CALLBACK(util_dialog_keypress_cb),
                     NULL);
    gtk_container_set_border_width(GTK_CONTAINER(mainwin_jtt), 10);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(mainwin_jtt), vbox);
    gtk_widget_show(vbox);
    frame = gtk_frame_new(_("Jump to:"));
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
    gtk_widget_set_size_request(frame, 250, -1);
    gtk_widget_show(frame);
    vbox_inside = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox_inside);
    gtk_widget_show(vbox_inside);

    hbox_new = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inside), hbox_new, TRUE, TRUE, 5);
    gtk_widget_show(hbox_new);
    time_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox_new), time_entry, FALSE, FALSE, 5);
    g_signal_connect(G_OBJECT(time_entry), "activate", G_CALLBACK(mainwin_jump_to_time_cb),
                     time_entry);
    gtk_widget_show(time_entry);
    gtk_widget_set_size_request(time_entry, 70, -1);
    label = gtk_label_new(_("minutes:seconds"));
    gtk_box_pack_start(GTK_BOX(hbox_new), label, FALSE, FALSE, 5);
    gtk_widget_show(label);

    hbox_total = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inside), hbox_total, TRUE, TRUE, 5);
    gtk_widget_show(hbox_total);
    label = gtk_label_new(_("Track length:"));
    gtk_box_pack_start(GTK_BOX(hbox_total), label, FALSE, FALSE, 5);
    gtk_widget_show(label);
    len = playlist_get_current_length() / 1000;
    sprintf(timestr, "%u:%2.2u", len / 60, len % 60);
    label = gtk_label_new(timestr);
    gtk_box_pack_start(GTK_BOX(hbox_total), label, FALSE, FALSE, 10);
    gtk_widget_show(label);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 0);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_widget_show(bbox);
    jump = gtk_button_new_with_label(_("Jump"));
    gtk_widget_set_can_default(jump, TRUE);
    gtk_container_add(GTK_CONTAINER(bbox), jump);
    g_signal_connect(G_OBJECT(jump), "clicked", G_CALLBACK(mainwin_jump_to_time_cb), time_entry);
    gtk_widget_show(jump);
    cancel = gtk_button_new_with_label(_("Cancel"));
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_container_add(GTK_CONTAINER(bbox), cancel);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(mainwin_jtt));
    gtk_widget_show(cancel);

    tindex = input_get_time() / 1000;
    sprintf(timestr, "%u:%2.2u", tindex / 60, tindex % 60);
    gtk_entry_set_text(GTK_ENTRY(time_entry), timestr);
    gtk_editable_select_region(GTK_EDITABLE(time_entry), 0, strlen(timestr));

    gtk_window_set_modal(GTK_WINDOW(mainwin_jtt), 1);
    gtk_widget_show(mainwin_jtt);
    gtk_widget_grab_focus(time_entry);
    gtk_widget_grab_default(jump);
}

static void mainwin_jump_to_file_real_cb(GtkTreeView *clist)
{
    gint pos;

    if (mainwin_tree_view_get_selected_int(clist, MAINWIN_JTF_COL_PLAYLIST_POS, &pos)) {
        if (get_input_playing())
            input_stop();
        playlist_set_position(pos);
        playlist_play();
        gtk_widget_destroy(mainwin_jtf);
    }
}

static void mainwin_jtf_set_qbtn_label(GtkTreeView *clist, GtkWidget *button)
{
    char *label = _("Queue");
    gint pos;

    if (mainwin_tree_view_get_selected_int(clist, MAINWIN_JTF_COL_PLAYLIST_POS, &pos) &&
        playlist_is_position_queued(pos))
        label = _("Unqueue");
    gtk_button_set_label(GTK_BUTTON(button), label);
}

static void mainwin_jump_to_file_select_row_cb(GtkTreeView *widget, GtkTreePath *path,
                                               GtkTreeViewColumn *col, gpointer cb_data)
{
    mainwin_jump_to_file_real_cb(widget);
}

static void mainwin_jump_to_file_cursor_changed_cb(GtkTreeView *widget, gpointer cb_data)
{
    mainwin_jtf_set_qbtn_label(widget, GTK_WIDGET(cb_data));
}

static void mainwin_jump_to_file_keypress_cb(GtkWidget *widget, GdkEventKey *event,
                                             gpointer userdata)
{
    if (event && (event->keyval == GDK_KEY_Return))
        mainwin_jump_to_file_real_cb(GTK_TREE_VIEW(widget));
    else if (event && (event->keyval == GDK_KEY_Escape))
        gtk_widget_destroy(mainwin_jtf);
}

static gboolean mainwin_jump_to_file_entry_keypress_cb(GtkWidget *widget, GdkEventKey *event,
                                                       gpointer userdata)
{
    GtkTreeView *clist = GTK_TREE_VIEW(userdata);
    gboolean stop = FALSE;

    if (!event)
        return FALSE;

    switch (event->keyval) {
    case GDK_KEY_Return:
        mainwin_jump_to_file_real_cb(clist);
        break;
    case GDK_KEY_Escape:
        gtk_widget_destroy(mainwin_jtf);
        break;
    case GDK_KEY_Up:
    case GDK_KEY_Down:
    case GDK_KEY_Page_Up:
    case GDK_KEY_Page_Down:
        gtk_widget_event(GTK_WIDGET(clist), (GdkEvent *)event);
        /* Stop the signal or we might lose focus */
        stop = TRUE;
        break;
    case GDK_KEY_BackSpace:
    case GDK_KEY_Delete:
        if (strlen(gtk_entry_get_text(GTK_ENTRY(widget))) == 0)
            /* Optimization: Ignore delete keys if
             * the string already is empty */
            stop = TRUE;
        break;
    default:
        return FALSE;
    }

    if (stop)
        g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

    return TRUE;
}

static void mainwin_jump_to_file_jump_cb(GtkButton *widget, gpointer userdata)
{
    mainwin_jump_to_file_real_cb(GTK_TREE_VIEW(userdata));
}

static int mainwin_jump_to_file_match(gchar *song, gchar *keys[], gint nw)
{
    gint i;

    for (i = 0; i < nw; i++) {
        if (!strstr(song, keys[i]))
            return 0;
    }

    return 1;
}

static void mainwin_jump_to_file_edit_real(GtkWidget *widget, gpointer userdata)
{
    char *key;
    GtkTreeView *clist;
    GList *playlist;
    char *desc_buf[2];
    int songnr = 0;

    char *words[20];
    int nw = 0, i;
    char *ptr;

    PL_LOCK();
    playlist = get_playlist();
    key = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
    clist = GTK_TREE_VIEW(userdata);

    /* Figure out what is currently selected */
    gint row_to_select = -1;
    gint prev_focus = mainwin_tree_view_get_cursor_int(clist, MAINWIN_JTF_COL_PLAYLIST_POS);

    /* lowercase the key string */
    g_strdown(key);

    /* Chop the key string into ' '-separeted key words */
    for (ptr = key; nw < 20; ptr = strchr(ptr, ' ')) {
        if (!ptr)
            break;
        else if (*ptr == ' ') {
            while (*ptr == ' ') {
                *ptr = '\0';
                ptr++;
            }
            words[nw++] = ptr;
        } else {
            words[nw++] = ptr;
        }
    }

    gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(clist)));

    while (playlist) {
        int match = 0;
        char *title, *filename;

        title = ((PlaylistEntry *)playlist->data)->title;
        filename = ((PlaylistEntry *)playlist->data)->filename;

        if (title)
            desc_buf[1] = title;
        else if (strchr(filename, '/'))
            desc_buf[1] = strrchr(filename, '/') + 1;
        else
            desc_buf[1] = filename;

        /*
         * Optimize a little since the delay when beginning to
         * type is annoying.  We'll drop through if the key is
         * zero length, and if it is a single letter, we'll
         * take a shortcut and use strchr. This'll save us
         * from constructing the lowercased song name to
         * compare.
         *
         * These (admittedly ugly) hacks help noticeably.
         *
         * A further optimization would be to save the
         * lowercased versions of the filenames rather than
         * constructing them every time. This'd take memory, so
         * I won't do it this time.
         */

        if (nw == 0 ||                           /* zero char key */
            (nw == 1 && words[0][0] == '\0') ||  /* zero char key */
            (nw == 1 && strlen(words[0]) == 1 && /* one char key */
             ((title &&
               (strchr(title, tolower(words[0][0])) || strchr(title, toupper(words[0][0])))) ||
              strchr(filename, tolower(words[0][0])) || strchr(filename, toupper(words[0][0])))))
            match = 1;
        else if (nw == 1 && strlen(words[0]) == 1)
            match = 0;
        else {
            char song[256];

            /* Cook up a lowercased string that contains
             * the filename and the (possible) title
             */
            for (ptr = title, i = 0; ptr && *ptr && i < 254; i++, ptr++)
                song[i] = tolower(*ptr);
            for (ptr = filename; *ptr && i < 254; i++, ptr++)
                song[i] = tolower(*ptr);
            song[i] = '\0';

            /* Compare the key words to the string - if
             * all the words match, add to the clist
             */
            match = mainwin_jump_to_file_match(song, words, nw);
        }

        if (match) {
            GtkTreeIter iter;
            int queue_pos;
            char *tmp_buf;
            queue_pos = playlist_get_queue_position((PlaylistEntry *)playlist->data);
            if (queue_pos != -1) {
                tmp_buf = g_strdup_printf("%i", queue_pos + 1);
                desc_buf[0] = tmp_buf;
            } else {
                tmp_buf = g_strdup(" ");
                desc_buf[0] = tmp_buf;
            }
            gtk_list_store_append(GTK_LIST_STORE(gtk_tree_view_get_model(clist)), &iter);
            gtk_list_store_set(GTK_LIST_STORE(gtk_tree_view_get_model(clist)), &iter,
                               MAINWIN_JTF_COL_QUEUE, desc_buf[0], MAINWIN_JTF_COL_FILE,
                               desc_buf[1], MAINWIN_JTF_COL_PLAYLIST_POS, songnr, -1);
            g_free(tmp_buf);
            if (songnr == prev_focus)
                row_to_select = songnr;
        }

        songnr++;
        playlist = playlist->next;
    }

    PL_UNLOCK();

    if (cfg.sort_jump_to_file) {
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(gtk_tree_view_get_model(clist)), MAINWIN_JTF_COL_FILE,
            GTK_SORT_ASCENDING);
    } else {
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(gtk_tree_view_get_model(clist)),
            GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
            GTK_SORT_ASCENDING);
    }

    g_free(key);

    if (row_to_select != -1)
        mainwin_tree_view_select_int(clist, MAINWIN_JTF_COL_PLAYLIST_POS, row_to_select);
    else
        mainwin_tree_view_select_first(clist);
}

static void mainwin_jump_to_file_edit_cb(GtkWidget *widget, gpointer userdata)
{
    mainwin_jump_to_file_edit_real(widget, userdata);
}

static void mainwin_jump_to_file_clist_refresh(GtkWidget *widget, gpointer userdata)
{
    gfloat adjustment;
    GtkTreeView *clist = GTK_TREE_VIEW(userdata);
    adjustment = gtk_adjustment_get_value(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(clist)));
    mainwin_jump_to_file_edit_real(widget, userdata);
    gtk_adjustment_set_value(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(clist)), adjustment);
}

static void mainwin_jump_to_file_queue_toggle(gint pos, gpointer userdata)
{
    GtkWidget **data = (GtkWidget **)userdata;
    GtkEditable *edit = GTK_EDITABLE(data[0]);
    GtkTreeView *clist = GTK_TREE_VIEW(data[1]);
    gpointer qlist = data[2];

    playlist_queue_position(pos);
    if (strlen(gtk_entry_get_text(GTK_ENTRY(edit))) > 0) {
        /*
         * XXX: It might be a good idea to add a setting such as
         *		cfg.reset_search_on_queue or something and check for it here.
         */
        gtk_editable_delete_text(GTK_EDITABLE(edit), 0, -1);
    } else {
        mainwin_jump_to_file_clist_refresh(GTK_WIDGET(edit), clist);
        if (qlist != NULL)
            mainwin_queue_manager_queue_refresh(GTK_WIDGET(edit), GTK_TREE_VIEW(qlist));
    }
}

static void mainwin_jump_to_file_queue_cb(GtkButton *widget, gpointer userdata)
{
    GtkWidget **data = (GtkWidget **)userdata;
    GtkTreeView *clist = GTK_TREE_VIEW(data[1]);
    gint pos;

    if (mainwin_tree_view_get_selected_int(clist, MAINWIN_JTF_COL_PLAYLIST_POS, &pos))
        mainwin_jump_to_file_queue_toggle(pos, userdata);
    mainwin_jtf_set_qbtn_label(clist, GTK_WIDGET(widget));
}

static void mainwin_jump_to_file_cleanup(GtkWidget *window, gpointer userdata)
{
    gtk_widget_destroyed(window, &window);
    /* Free memory allocated for edit_clist_qlist_and_queue
       in mainwin_jump_to_file  */
    g_free(userdata);
}

static void mainwin_jump_to_file(void)
{
    GtkWidget *vbox, *scrollwin, *clist, *sep, *bbox, *jump, *queue, *cancel, *edit, *search_label,
        *hbox;
    /*
     * This little bugger is because I need all these widgets in some of
     * the signal handlers. It will be freed when the window
     * is destroyed. Better solutions are very much welcomed.
     */
    GtkWidget **edit_clist_qlist_and_queue = g_malloc(sizeof(GtkWidget *) * 4);

    mainwin_jtf = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mainwin_jtf), _("Jump to file"));
    gtk_window_set_transient_for(GTK_WINDOW(mainwin_jtf), GTK_WINDOW(mainwin));
    g_signal_connect(G_OBJECT(mainwin_jtf), "destroy", G_CALLBACK(mainwin_jump_to_file_cleanup),
                     edit_clist_qlist_and_queue);
    gtk_container_set_border_width(GTK_CONTAINER(mainwin_jtf), 10);
    gtk_window_set_default_size(GTK_WINDOW(mainwin_jtf), 300, 350);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(mainwin_jtf), vbox);
    gtk_widget_show(vbox);

    queue = gtk_button_new_with_label(_("Queue"));

    clist = mainwin_create_jump_tree_view();
    /* GTK3: select_row→row-activated + cursor-changed for label updates */
    g_signal_connect(G_OBJECT(clist), "row-activated",
                     G_CALLBACK(mainwin_jump_to_file_select_row_cb), queue);
    g_signal_connect(G_OBJECT(clist), "cursor-changed",
                     G_CALLBACK(mainwin_jump_to_file_cursor_changed_cb), queue);
    g_signal_connect(G_OBJECT(clist), "key_press_event",
                     G_CALLBACK(mainwin_jump_to_file_keypress_cb), NULL);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
    gtk_widget_show(hbox);

    search_label = gtk_label_new(_("Search: "));
    gtk_box_pack_start(GTK_BOX(hbox), search_label, FALSE, FALSE, 0);
    gtk_widget_show(search_label);

    edit = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(edit), TRUE);
    g_signal_connect(G_OBJECT(edit), "changed", G_CALLBACK(mainwin_jump_to_file_edit_cb), clist);
    g_signal_connect(G_OBJECT(edit), "key_press_event",
                     G_CALLBACK(mainwin_jump_to_file_entry_keypress_cb), clist);
    gtk_box_pack_start(GTK_BOX(hbox), edit, TRUE, TRUE, 3);
    gtk_widget_show(edit);

    /* looks messy putting them down here, but edit isn't
       defined until just above */
    edit_clist_qlist_and_queue[0] = edit;
    edit_clist_qlist_and_queue[1] = clist;
    edit_clist_qlist_and_queue[2] = NULL;
    edit_clist_qlist_and_queue[3] = queue;

    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrollwin), clist);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(vbox), scrollwin, TRUE, TRUE, 0);
    gtk_widget_set_size_request(scrollwin, 330, 200);
    gtk_widget_show(clist);
    gtk_widget_show(scrollwin);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
    gtk_widget_show(sep);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    jump = gtk_button_new_with_label(_("Jump"));
    gtk_box_pack_start(GTK_BOX(bbox), jump, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(jump), "clicked", G_CALLBACK(mainwin_jump_to_file_jump_cb), clist);
    gtk_widget_set_can_default(jump, TRUE);
    gtk_widget_show(jump);
    gtk_widget_grab_default(jump);

    gtk_box_pack_start(GTK_BOX(bbox), queue, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(queue), "clicked", G_CALLBACK(mainwin_jump_to_file_queue_cb),
                     edit_clist_qlist_and_queue);
    gtk_widget_set_can_default(queue, TRUE);
    gtk_widget_show(queue);

    cancel = gtk_button_new_with_label(_("Close"));
    gtk_box_pack_start(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(mainwin_jtf));
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_widget_show(cancel);
    gtk_widget_show(bbox);

    /* Initial read of playlist (will take care of locking by itself) */
    mainwin_jump_to_file_edit_cb(GTK_WIDGET(edit), clist);

    gtk_window_set_modal(GTK_WINDOW(mainwin_jtf), 1);
    gtk_widget_show(mainwin_jtf);
    gtk_widget_grab_focus(edit);
}

static void mainwin_queue_manager_select_row_cb(GtkTreeView *widget, GtkTreePath *path,
                                                GtkTreeViewColumn *col, gpointer cb_data)
{
    /* GTK3: row-activated fires on double-click/Enter; single-click handled by cursor-changed */
}

static gboolean mainwin_queue_manager_entry_keypress_cb(GtkWidget *widget, GdkEventKey *event,
                                                        gpointer userdata)
{
    GtkWidget **data = (GtkWidget **)userdata;
    GtkTreeView *clist = GTK_TREE_VIEW(data[1]);
    gboolean stop = FALSE;

    if (!event)
        return FALSE;

    switch (event->keyval) {
    case GDK_KEY_Return:
    {
        gint pos;
        if (mainwin_tree_view_get_selected_int(clist, MAINWIN_JTF_COL_PLAYLIST_POS, &pos))
            mainwin_jump_to_file_queue_toggle(pos, userdata);
        break;
    }
    case GDK_KEY_Escape:
        gtk_widget_destroy(mainwin_qm);
        break;
    case GDK_KEY_Up:
    case GDK_KEY_Down:
    case GDK_KEY_Page_Up:
    case GDK_KEY_Page_Down:
        gtk_widget_event(GTK_WIDGET(clist), (GdkEvent *)event);
        /* Stop the signal or we might lose focus */
        stop = TRUE;
        break;
    case GDK_KEY_BackSpace:
    case GDK_KEY_Delete:
        if (strlen(gtk_entry_get_text(GTK_ENTRY(widget))) == 0)
            /* Optimization: Ignore delete keys if
             * the string already is empty */
            stop = TRUE;
        break;
    default:
        return FALSE;
    }

    if (stop)
        g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

    return TRUE;
}

static void mainwin_queue_manager_queue_refresh(GtkWidget *widget, gpointer userdata)
{
    GtkTreeView *qlist = GTK_TREE_VIEW(userdata);
    GList *queue;
    int pos = 0;
    gint row_to_select = -1;
    gint prev_focus;
    gfloat adjustment;

    PL_LOCK();

    adjustment = gtk_adjustment_get_value(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(qlist)));

    prev_focus = mainwin_tree_view_get_cursor_int(qlist, MAINWIN_QM_COL_QUEUE_POS);

    gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(qlist)));

    for (queue = get_queue(); queue; queue = queue->next) {
        GtkTreeIter iter;
        char *title, *filename, *tmp_buf, *desc_buf[2];

        title = ((PlaylistEntry *)queue->data)->title;
        filename = ((PlaylistEntry *)queue->data)->filename;
        if (title)
            desc_buf[1] = title;
        else if (strchr(filename, '/'))
            desc_buf[1] = strrchr(filename, '/') + 1;
        else
            desc_buf[1] = filename;

        tmp_buf = g_strdup_printf("%i", pos + 1);
        desc_buf[0] = tmp_buf;
        gtk_list_store_append(GTK_LIST_STORE(gtk_tree_view_get_model(qlist)), &iter);
        gtk_list_store_set(GTK_LIST_STORE(gtk_tree_view_get_model(qlist)), &iter,
                           MAINWIN_QM_COL_QUEUE, desc_buf[0], MAINWIN_QM_COL_FILE, desc_buf[1],
                           MAINWIN_QM_COL_QUEUE_POS, pos, -1);
        g_free(tmp_buf);

        if (prev_focus == pos)
            row_to_select = pos;

        ++pos;
    }

    PL_UNLOCK();

    if (row_to_select != -1)
        mainwin_tree_view_select_int(qlist, MAINWIN_QM_COL_QUEUE_POS, row_to_select);
    else
        mainwin_tree_view_select_first(qlist);
    /* make sure we don't scroll out of the list */
    if (adjustment +
            gtk_adjustment_get_page_size(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(qlist))) >
        gtk_adjustment_get_upper(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(qlist))))
        adjustment =
            gtk_adjustment_get_upper(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(qlist))) -
            gtk_adjustment_get_page_size(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(qlist)));
    gtk_adjustment_set_value(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(qlist)), adjustment);
}

static void mainwin_queue_manager_reorder_real(GtkTreeView *widget, gint arg1, gint arg2,
                                               gpointer userdata)
{
    GtkWidget **data = (GtkWidget **)userdata;
    GtkEditable *edit = GTK_EDITABLE(data[0]);
    GtkTreeView *clist = GTK_TREE_VIEW(data[1]);
    GtkTreeView *qlist = GTK_TREE_VIEW(data[2]);

    /* move and adjust qlist selection accordingly */
    playlist_queue_move(arg1, arg2);
    /* refresh qlist */
    mainwin_queue_manager_queue_refresh(GTK_WIDGET(widget), qlist);
    mainwin_tree_view_select_int(qlist, MAINWIN_QM_COL_QUEUE_POS, arg2);
    /* refresh clist */
    mainwin_jump_to_file_clist_refresh(GTK_WIDGET(edit), clist);
}

static void mainwin_queue_manager_reorder(GtkTreeView *widget, gint arg1, gint arg2,
                                          gpointer userdata)
{
    mainwin_queue_manager_reorder_real(widget, arg1, arg2, userdata);
    /* stop the signal to prevent it from reordering our newly reread queue. */
    g_signal_stop_emission_by_name(G_OBJECT(widget), "row-move");
}

static void mainwin_queue_manager_move_up_cb(GtkWidget *widget, gpointer userdata)
{
    GtkWidget **data = (GtkWidget **)userdata;
    GtkTreeView *qlist = GTK_TREE_VIEW(data[2]);
    gint pos;

    if (mainwin_tree_view_get_selected_int(qlist, MAINWIN_QM_COL_QUEUE_POS, &pos) && pos > 0)
        mainwin_queue_manager_reorder_real(qlist, pos, pos - 1, userdata);
}

static void mainwin_queue_manager_move_down_cb(GtkWidget *widget, gpointer userdata)
{
    GtkWidget **data = (GtkWidget **)userdata;
    GtkTreeView *qlist = GTK_TREE_VIEW(data[2]);
    gint pos;

    if (mainwin_tree_view_get_selected_int(qlist, MAINWIN_QM_COL_QUEUE_POS, &pos) &&
        pos < get_playlist_queue_length() - 1)
        mainwin_queue_manager_reorder_real(qlist, pos, pos + 1, userdata);
}

static void mainwin_queue_manager_remove_cb(GtkWidget *widget, gpointer userdata)
{
    GtkWidget **data = (GtkWidget **)userdata;
    GtkTreeView *qlist = GTK_TREE_VIEW(data[2]);
    gint queue_pos;

    if (mainwin_tree_view_get_selected_int(qlist, MAINWIN_QM_COL_QUEUE_POS, &queue_pos)) {
        int pos = playlist_get_playlist_position_from_playqueue_position(queue_pos);
        mainwin_jump_to_file_queue_toggle(pos, userdata);
    }
}

static void mainwin_queue_manager_keypress_cb(GtkWidget *widget, GdkEventKey *event,
                                              gpointer userdata)
{
    if (event && (event->keyval == GDK_KEY_Escape))
        gtk_widget_destroy(mainwin_qm);
}

static void mainwin_queue_manager_qlist_keypress_cb(GtkWidget *widget, GdkEventKey *event,
                                                    gpointer userdata)
{
    if (event && (event->keyval == GDK_KEY_Escape))
        gtk_widget_destroy(mainwin_qm);
    /* for some reason, this is needed to avoid clist from reading the Return
       keypress from qlist (which is really confusing behaviour for the user) */
    else if (event && (event->keyval == GDK_KEY_Return))
        g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
    else if (event && (event->keyval == GDK_KEY_Delete)) {
        mainwin_queue_manager_remove_cb(widget, userdata);
        g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
    }
}

void mainwin_queue_manager(void)
{
    GtkWidget *bigbox, *lbox, *vbox, *qscrollwin, *scrollwin, *qlist, *clist, *qsep, *sep, *bbox,
        *qbox, *queue, *cancel, *edit, *queue_label, *search_label, *hbox, *move_up, *move_down,
        *remove;
    /*
     * This little bugger is because I need all these widgets in some of
     * the signal handlers. It will be freed when the window
     * is destroyed. Better solutions are very much welcomed.
     */
    GtkWidget **edit_clist_qlist_and_queue = g_malloc(sizeof(GtkWidget *) * 4);

    mainwin_qm = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mainwin_qm), _("Jump to file"));
    gtk_window_set_transient_for(GTK_WINDOW(mainwin_qm), GTK_WINDOW(mainwin));
    g_signal_connect(G_OBJECT(mainwin_qm), "destroy", G_CALLBACK(mainwin_jump_to_file_cleanup),
                     edit_clist_qlist_and_queue);
    gtk_container_set_border_width(GTK_CONTAINER(mainwin_qm), 10);
    gtk_window_set_default_size(GTK_WINDOW(mainwin_qm), 600, 350);

    bigbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(mainwin_qm), bigbox);
    gtk_widget_show(bigbox);

    lbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(bigbox), lbox);
    gtk_widget_show(lbox);

    qsep = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(bigbox), qsep, FALSE, FALSE, 0);
    gtk_widget_show(qsep);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(bigbox), vbox);
    gtk_widget_show(vbox);

    queue = gtk_button_new_with_label(_("Queue"));

    qlist = mainwin_create_queue_tree_view();
    qscrollwin = gtk_scrolled_window_new(NULL, NULL);
    queue_label = gtk_label_new(_("Queued files: "));
    gtk_box_pack_start(GTK_BOX(lbox), queue_label, FALSE, FALSE, 0);
    gtk_widget_show(queue_label);
    g_signal_connect(G_OBJECT(qlist), "key_press_event",
                     G_CALLBACK(mainwin_queue_manager_qlist_keypress_cb),
                     edit_clist_qlist_and_queue);
    g_signal_connect(G_OBJECT(qlist), "row-move", G_CALLBACK(mainwin_queue_manager_reorder),
                     edit_clist_qlist_and_queue);
    gtk_container_add(GTK_CONTAINER(qscrollwin), qlist);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(qscrollwin), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(lbox), qscrollwin, TRUE, TRUE, 0);
    gtk_widget_set_size_request(qscrollwin, 330, 200);
    gtk_widget_show(qlist);
    gtk_widget_show(qscrollwin);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(lbox), sep, FALSE, FALSE, 0);
    gtk_widget_show(sep);

    qbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(qbox), GTK_BUTTONBOX_START);
    gtk_box_set_spacing(GTK_BOX(qbox), 5);
    gtk_box_pack_start(GTK_BOX(lbox), qbox, FALSE, FALSE, 0);

    move_up = gtk_button_new_with_label(_("Move up"));
    gtk_box_pack_start(GTK_BOX(qbox), move_up, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(move_up), "clicked", G_CALLBACK(mainwin_queue_manager_move_up_cb),
                     edit_clist_qlist_and_queue);
    gtk_widget_set_can_default(move_up, TRUE);
    gtk_widget_show(move_up);

    move_down = gtk_button_new_with_label(_("Move down"));
    gtk_box_pack_start(GTK_BOX(qbox), move_down, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(move_down), "clicked", G_CALLBACK(mainwin_queue_manager_move_down_cb),
                     edit_clist_qlist_and_queue);
    gtk_widget_set_can_default(move_down, TRUE);
    gtk_widget_show(move_down);

    remove = gtk_button_new_with_label(_("Remove"));
    gtk_box_pack_start(GTK_BOX(qbox), remove, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(remove), "clicked", G_CALLBACK(mainwin_queue_manager_remove_cb),
                     edit_clist_qlist_and_queue);
    gtk_widget_set_can_default(remove, TRUE);
    gtk_widget_show(remove);

    gtk_widget_show(qbox);

    clist = mainwin_create_jump_tree_view();
    /* GTK3: select_row → row-activated */
    g_signal_connect(G_OBJECT(clist), "row-activated",
                     G_CALLBACK(mainwin_queue_manager_select_row_cb), edit_clist_qlist_and_queue);
    g_signal_connect(G_OBJECT(clist), "key_press_event",
                     G_CALLBACK(mainwin_queue_manager_keypress_cb), qlist);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
    gtk_widget_show(hbox);

    search_label = gtk_label_new(_("Search: "));
    gtk_box_pack_start(GTK_BOX(hbox), search_label, FALSE, FALSE, 0);
    gtk_widget_show(search_label);

    edit = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(edit), TRUE);
    g_signal_connect(G_OBJECT(edit), "changed", G_CALLBACK(mainwin_jump_to_file_edit_cb), clist);
    /* For updating the queue when people press enter */
    g_signal_connect(G_OBJECT(edit), "changed", G_CALLBACK(mainwin_queue_manager_queue_refresh),
                     qlist);
    g_signal_connect(G_OBJECT(edit), "key_press_event",
                     G_CALLBACK(mainwin_queue_manager_entry_keypress_cb),
                     edit_clist_qlist_and_queue);
    gtk_box_pack_start(GTK_BOX(hbox), edit, TRUE, TRUE, 3);
    gtk_widget_show(edit);

    /* looks messy putting them down here, but edit isn't
       defined until just above */
    edit_clist_qlist_and_queue[0] = edit;
    edit_clist_qlist_and_queue[1] = clist;
    edit_clist_qlist_and_queue[2] = qlist;
    edit_clist_qlist_and_queue[3] = queue;


    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrollwin), clist);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(vbox), scrollwin, TRUE, TRUE, 0);
    gtk_widget_set_size_request(scrollwin, 330, 200);
    gtk_widget_show(clist);
    gtk_widget_show(scrollwin);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
    gtk_widget_show(sep);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(bbox), queue, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(queue), "clicked", G_CALLBACK(mainwin_jump_to_file_queue_cb),
                     edit_clist_qlist_and_queue);
    gtk_widget_set_can_default(queue, TRUE);
    gtk_widget_show(queue);
    gtk_widget_grab_default(queue);

    cancel = gtk_button_new_with_label(_("Close"));
    gtk_box_pack_start(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(mainwin_qm));
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_widget_show(cancel);
    gtk_widget_show(bbox);

    /* Initial read of playlist and queue */
    mainwin_jump_to_file_edit_cb(GTK_WIDGET(edit), clist);
    mainwin_queue_manager_queue_refresh(GTK_WIDGET(clist), qlist);

    gtk_window_set_modal(GTK_WINDOW(mainwin_qm), 1);
    gtk_widget_show(mainwin_qm);
    gtk_widget_grab_focus(edit);
}

static gboolean mainwin_configure(GtkWidget *window, GdkEventConfigure *event, gpointer data)
{
    if (!gtk_widget_is_visible(window))
        return FALSE;

    if (cfg.show_wm_decorations)
        gdk_window_get_root_origin(gtk_widget_get_window(window), &cfg.player_x, &cfg.player_y);
    else
        gdk_window_get_root_origin(gtk_widget_get_window(window), &cfg.player_x, &cfg.player_y);
    return FALSE;
}

void mainwin_set_back_pixmap(void)
{
    if (cfg.doublesize)
        ;
    else
        ;
    /* TODO(#gtk3): gdk_window_set_back_pixmap removed */
    gtk_widget_queue_draw(mainwin);
}

gint mainwin_client_event(GtkWidget *w, GdkEventAny *event, gpointer data)
{
    /* TODO(#gtk3): GdkEventClient removed; stub */
    (void)event;
    return FALSE;
}

static void mainwin_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                                       GtkSelectionData *selection_data, guint info, guint time,
                                       gpointer user_data)
{
    if (gtk_selection_data_get_data(selection_data)) {
        playlist_clear();
        playlist_add_url_string((gchar *)gtk_selection_data_get_data(selection_data));
        playlist_play();
    }
}

void mainwin_add_dir_handler(gchar *dir)
{
    if (cfg.filesel_path)
        g_free(cfg.filesel_path);
    cfg.filesel_path = g_strdup(dir);
    playlist_clear();
    playlist_add_dir(dir);
    playlist_play();
}

void mainwin_show_dirbrowser(void)
{
    if (!mainwin_dir_browser) {
        mainwin_dir_browser =
            xmms_create_dir_browser(_("Select directory to add:"), cfg.filesel_path,
                                    GTK_SELECTION_SINGLE, mainwin_add_dir_handler);
        g_signal_connect(G_OBJECT(mainwin_dir_browser), "destroy", G_CALLBACK(on_widget_destroyed),
                         &mainwin_dir_browser);
        g_signal_connect(G_OBJECT(mainwin_dir_browser), "key_press_event",
                         G_CALLBACK(util_dialog_keypress_cb), NULL);
        gtk_window_set_transient_for(GTK_WINDOW(mainwin_dir_browser), GTK_WINDOW(mainwin));
    }
    if (!gtk_widget_is_visible(mainwin_dir_browser))
        gtk_widget_show(mainwin_dir_browser);
}

void mainwin_url_ok_clicked(GtkWidget *w, GtkWidget *entry)
{
    char *text = gtk_entry_get_text(GTK_ENTRY(entry));

    if (text && *text) {
        playlist_clear();
        g_strstrip(text);
        if (strstr(text, ":/") == NULL && text[0] != '/') {
            char *temp = g_strconcat("http://", text, NULL);
            playlist_add_url_string(temp);
            g_free(temp);
        } else
            playlist_add_url_string(text);
        playlist_play();
    }
    gtk_widget_destroy(mainwin_url_window);
}

void mainwin_url_enqueue_clicked(GtkWidget *w, GtkWidget *entry)
{
    gchar *text;

    text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (text && *text)
        playlist_add_url_string(text);
    gtk_widget_destroy(mainwin_url_window);
}

void mainwin_show_add_url_window(void)
{
    if (mainwin_url_window)
        return;

    mainwin_url_window =
        util_create_add_url_window(_("Enter location to play:"), G_CALLBACK(mainwin_url_ok_clicked),
                                   G_CALLBACK(mainwin_url_enqueue_clicked));
    gtk_window_set_transient_for(GTK_WINDOW(mainwin_url_window), GTK_WINDOW(mainwin));
    g_signal_connect(G_OBJECT(mainwin_url_window), "destroy", G_CALLBACK(on_widget_destroyed),
                     &mainwin_url_window);
    g_signal_connect(G_OBJECT(mainwin_url_window), "key_press_event",
                     G_CALLBACK(util_dialog_keypress_cb), NULL);
    gtk_widget_show(mainwin_url_window);
}

void mainwin_eject_pushed(void)
{
    static GtkWidget *filebrowser;
    if (filebrowser != NULL) {
        gdk_window_raise(gtk_widget_get_window(filebrowser));
        return;
    }
    filebrowser = util_create_filebrowser(TRUE);
    g_signal_connect(G_OBJECT(filebrowser), "destroy", G_CALLBACK(on_widget_destroyed),
                     &filebrowser);
    g_signal_connect(G_OBJECT(filebrowser), "key_press_event", G_CALLBACK(util_dialog_keypress_cb),
                     NULL);
}

void mainwin_play_pushed(void)
{
    if (get_input_paused()) {
        input_pause();
        return;
    }
    if (get_playlist_length())
        playlist_play();
    else
        mainwin_eject_pushed();
}

void mainwin_stop_pushed(void)
{
    mainwin_clear_song_info();
    input_stop();
    _mpris2_on_stop();
}

void mainwin_shuffle_pushed(gboolean toggled)
{
    if (mi_opt_shuffle)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi_opt_shuffle), toggled);
}

void mainwin_repeat_pushed(gboolean toggled)
{
    if (mi_opt_repeat)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi_opt_repeat), toggled);
}

void mainwin_advance_pushed(gboolean toggled)
{
    /* there isn't really an advance button, but it's easier to handle
       the advance command in the same way as the shuffle and repeat cases
       if we pretend that there is. */
    if (mi_opt_npa)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi_opt_npa), toggled);
}

void mainwin_pl_pushed(gboolean toggled)
{
    playlistwin_show(toggled);
}

gint mainwin_spos_frame_cb(gint pos)
{
    if (mainwin_sposition) {
        if (pos < 6)
            mainwin_sposition->hs_knob_nx = mainwin_sposition->hs_knob_px = 17;
        else if (pos < 9)
            mainwin_sposition->hs_knob_nx = mainwin_sposition->hs_knob_px = 20;
        else
            mainwin_sposition->hs_knob_nx = mainwin_sposition->hs_knob_px = 23;
    }
    return 1;
}

void mainwin_spos_motion_cb(gint pos)
{
    gint time;
    gchar *tmp;

    pos--;

    time = ((playlist_get_current_length() / 1000) * pos) / 12;
    if (cfg.timer_mode == TIMER_REMAINING) {
        time = (playlist_get_current_length() / 1000) - time;
        tmp = g_strdup_printf("-%2.2d", time / 60);
        textbox_set_text(mainwin_stime_min, tmp);
        g_free(tmp);
    } else {
        tmp = g_strdup_printf(" %2.2d", time / 60);
        textbox_set_text(mainwin_stime_min, tmp);
        g_free(tmp);
    }
    tmp = g_strdup_printf("%2.2d", time % 60);
    textbox_set_text(mainwin_stime_sec, tmp);
    g_free(tmp);
}

void mainwin_spos_release_cb(gint pos)
{
    input_seek(((playlist_get_current_length() / 1000) * (pos - 1)) / 12);
}

void mainwin_position_motioncb(gint pos)
{
    gint length, time;
    gchar *buf;

    length = playlist_get_current_length() / 1000;
    time = (length * pos) / 219;
    buf = g_strdup_printf(_("SEEK TO: %d:%-2.2d/%d:%-2.2d (%d%%)"), time / 60, time % 60,
                          length / 60, length % 60, (length != 0) ? (time * 100) / length : 0);
    mainwin_lock_info_text(buf);
    g_free(buf);
}

void mainwin_position_releasecb(gint pos)
{
    int length, time;

    length = playlist_get_current_length() / 1000;
    time = (length * pos) / 219;
    input_seek(time);
    mainwin_release_info_text();
}

gint mainwin_volume_framecb(gint pos)
{
    return (int)rint((pos / 52.0) * 28);
}

void mainwin_adjust_volume_motion(gint v)
{
    gchar *tmp;

    setting_volume = TRUE;
    tmp = g_strdup_printf(_("VOLUME: %d%%"), v);
    mainwin_lock_info_text(tmp);
    g_free(tmp);
    if (balance < 0)
        input_set_volume(v, (v * (100 - abs(balance))) / 100);
    else if (balance > 0)
        input_set_volume((v * (100 - abs(balance))) / 100, v);
    else
        input_set_volume(v, v);
}

void mainwin_adjust_volume_release(void)
{
    mainwin_release_info_text();
    setting_volume = FALSE;
    read_volume(VOLUME_ADJUSTED);
}

void mainwin_adjust_balance_motion(gint b)
{
    char *tmp;
    gint v, pvl, pvr;

    setting_volume = TRUE;
    balance = b;
    input_get_volume(&pvl, &pvr);
    v = MAX(pvl, pvr);
    if (b < 0) {
        tmp = g_strdup_printf(_("BALANCE: %d%% LEFT"), -b);
        input_set_volume(v, (int)rint(((100 + b) / 100.0) * v));
    } else if (b == 0) {
        tmp = g_strdup_printf(_("BALANCE: CENTER"));
        input_set_volume(v, v);
    } else /* (b > 0) */
    {
        tmp = g_strdup_printf(_("BALANCE: %d%% RIGHT"), b);
        input_set_volume((int)rint(((100 - b) / 100.0) * v), v);
    }
    mainwin_lock_info_text(tmp);
    g_free(tmp);
}

void mainwin_adjust_balance_release(void)
{
    mainwin_release_info_text();
    setting_volume = FALSE;
    read_volume(VOLUME_ADJUSTED);
}

void mainwin_set_volume_slider(gint percent)
{
    hslider_set_position(mainwin_volume, (gint)rint((percent * 51) / 100.0));
}

void mainwin_set_balance_slider(gint percent)
{
    hslider_set_position(mainwin_balance, (gint)rint(((percent * 12) / 100.0) + 12));
}

void mainwin_volume_motioncb(gint pos)
{
    gint vol = (pos * 100) / 51;
    mainwin_adjust_volume_motion(vol);
    equalizerwin_set_volume_slider(vol);
}

void mainwin_volume_releasecb(gint pos)
{
    mainwin_adjust_volume_release();
}

gint mainwin_balance_framecb(gint pos)
{
    return ((abs(pos - 12) * 28) / 13);
}

void mainwin_balance_motioncb(gint pos)
{
    gint bal = ((pos - 12) * 100) / 12;
    mainwin_adjust_balance_motion(bal);
    equalizerwin_set_balance_slider(bal);
}

void mainwin_balance_releasecb(gint pos)
{
    mainwin_adjust_volume_release();
}

void mainwin_set_volume_diff(gint diff)
{
    gint vl, vr, vol;
    input_get_volume(&vl, &vr);
    vol = MAX(vl, vr);
    vol = CLAMP(vol + diff, 0, 100);
    mainwin_adjust_volume_motion(vol);
    setting_volume = FALSE;
    mainwin_set_volume_slider(vol);
    equalizerwin_set_volume_slider(vol);
    read_volume(VOLUME_SET);
}

void mainwin_set_balance_diff(gint diff)
{
    gint b;
    b = CLAMP(balance + diff, -100, 100);
    mainwin_adjust_balance_motion(b);
    setting_volume = FALSE;
    mainwin_set_balance_slider(b);
    equalizerwin_set_balance_slider(b);
    read_volume(VOLUME_SET);
}

void mainwin_show(gboolean show)
{
    if (mi_gen_showmwin)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi_gen_showmwin), show);
}

void mainwin_real_show(void)
{
    cfg.player_visible = TRUE;

    if (cfg.player_shaded)
        vis_clear_data(active_vis);

    mainwin_vis_set_active_vis(MAINWIN_VIS_ACTIVE_MAINWIN);
    mainwin_set_shape_mask();
    if (cfg.show_wm_decorations) {
        if (!pposition_broken && cfg.player_x != -1 && cfg.save_window_position)
            dock_set_uposition(mainwin, cfg.player_x, cfg.player_y);
        gtk_widget_show(mainwin);
        if (pposition_broken && cfg.player_x != -1 && cfg.save_window_position)
            dock_set_uposition(mainwin, cfg.player_x, cfg.player_y);
        return;
    }
    if (!nullmask)
        /* Startup */
        return;
    cairo_surface_destroy(nullmask);
    nullmask = NULL;
    /* TODO(#gtk3): gdk_window_set_hints → gdk_window_set_geometry_hints */
    gdk_window_resize(gtk_widget_get_window(mainwin), PLAYER_WIDTH, PLAYER_HEIGHT);
    draw_main_window(TRUE);
    gdk_window_raise(gtk_widget_get_window(mainwin));
}

void mainwin_real_hide(void)
{
    cairo_region_t *region;
    gint width, height;

    /*  	if (!cfg.player_visible) */
    /*  		return; */

    if (cfg.player_shaded) {
        svis_clear_data(mainwin_svis);
        vis_clear_data(playlistwin_vis);
    }
    if (cfg.show_wm_decorations)
        gtk_widget_hide(mainwin);
    else {
        if (nullmask) {
            cairo_surface_destroy(nullmask);
            nullmask = NULL;
        }
        width = PLAYER_WIDTH;
        height = PLAYER_HEIGHT;
        region = mainwin_create_opaque_region(width, height, &nullmask);
        gtk_widget_shape_combine_region(mainwin, region);
        cairo_region_destroy(region);

        /* TODO(#gtk3): gdk_window_set_hints → gdk_window_set_geometry_hints */
        gdk_window_resize(gtk_widget_get_window(mainwin), 0, 0);
    }
    mainwin_vis_set_active_vis(MAINWIN_VIS_ACTIVE_PLAYLISTWIN);
    cfg.player_visible = FALSE;
}

void mainwin_songname_menu_callback(gpointer cb_data, guint action, GtkWidget *w)
{
    switch (action) {
    case MAINWIN_SONGNAME_FILEINFO:
        playlist_fileinfo_current();
        break;
    case MAINWIN_SONGNAME_JTF:
        mainwin_jump_to_file();
        break;
    case MAINWIN_SONGNAME_JTT:
        mainwin_jump_to_time();
        break;
    case MAINWIN_SONGNAME_SCROLL:
        cfg.autoscroll = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        textbox_set_scroll(mainwin_info, cfg.autoscroll);
        break;
    }
}

void mainwin_options_menu_callback(gpointer cb_data, guint action, GtkWidget *w)
{
    switch (action) {
    case MAINWIN_OPT_PREFS:
        show_prefs_window();
        break;
    case MAINWIN_OPT_SKIN:
        show_skin_window();
        break;
    case MAINWIN_OPT_RELOADSKIN:
        reload_skin();
        break;
    case MAINWIN_OPT_SHUFFLE: {
        gboolean shuffle;
        shuffle = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        tbutton_set_toggled(mainwin_shuffle, shuffle);
        playlist_set_shuffle(shuffle);
        break;
    }
    case MAINWIN_OPT_REPEAT:
        cfg.repeat = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        tbutton_set_toggled(mainwin_repeat, cfg.repeat);
        break;
    case MAINWIN_OPT_TELAPSED:
        set_timer_mode_menu_cb(TIMER_ELAPSED);
        break;
    case MAINWIN_OPT_TREMAINING:
        set_timer_mode_menu_cb(TIMER_REMAINING);
        break;
    case MAINWIN_OPT_TDISPLAY:
        cfg.timer_minutes_only = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        break;
    case MAINWIN_OPT_ALWAYS:
        mainwin_menurow->mr_always_selected =
            gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        cfg.always_on_top = mainwin_menurow->mr_always_selected;
        draw_widget(mainwin_menurow);
        hint_set_always(cfg.always_on_top);
        break;
    case MAINWIN_OPT_STICKY: {
        cfg.sticky = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        hint_set_sticky(cfg.sticky);
        break;
    }
    case MAINWIN_OPT_WS:
        mainwin_set_shade_menu_cb(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)));
        break;
    case MAINWIN_OPT_PWS:
        playlistwin_set_shade_menu_cb(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)));
        break;
    case MAINWIN_OPT_EQWS:
        equalizerwin_set_shade_menu_cb(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)));
        break;
    case MAINWIN_OPT_DOUBLESIZE:
        mainwin_menurow->mr_doublesize_selected =
            gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        draw_widget(mainwin_menurow);
        set_doublesize(mainwin_menurow->mr_doublesize_selected);
        /* gdk_flush() no-op in GTK3 */
        break;
    case MAINWIN_OPT_EASY_MOVE:
        cfg.easy_move = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        break;
    case MAINWIN_OPT_NPA:
        cfg.no_playlist_advance = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
    }
}

void mainwin_vis_menu_callback(gpointer cb_data, guint action, GtkWidget *w)
{
    switch (action) {
    case MAINWIN_VIS_ANALYZER:
    case MAINWIN_VIS_SCOPE:
    case MAINWIN_VIS_OFF:
        mainwin_vis_set_type_menu_cb(action - MAINWIN_VIS_ANALYZER);
        break;
    case MAINWIN_VIS_ANALYZER_NORMAL:
    case MAINWIN_VIS_ANALYZER_FIRE:
    case MAINWIN_VIS_ANALYZER_VLINES:
        mainwin_vis_set_analyzer_mode(action - MAINWIN_VIS_ANALYZER_NORMAL);
        break;
    case MAINWIN_VIS_ANALYZER_LINES:
    case MAINWIN_VIS_ANALYZER_BARS:
        mainwin_vis_set_analyzer_type(action - MAINWIN_VIS_ANALYZER_LINES);
        break;
    case MAINWIN_VIS_ANALYZER_PEAKS:
        cfg.analyzer_peaks = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));
        break;
    case MAINWIN_VIS_SCOPE_DOT:
    case MAINWIN_VIS_SCOPE_LINE:
    case MAINWIN_VIS_SCOPE_SOLID:
        cfg.scope_mode = action - MAINWIN_VIS_SCOPE_DOT;
        break;
    case MAINWIN_VIS_VU_NORMAL:
    case MAINWIN_VIS_VU_SMOOTH:
        cfg.vu_mode = action - MAINWIN_VIS_VU_NORMAL;
        break;
    case MAINWIN_VIS_REFRESH_FULL:
    case MAINWIN_VIS_REFRESH_HALF:
    case MAINWIN_VIS_REFRESH_QUARTER:
    case MAINWIN_VIS_REFRESH_EIGHTH:
        mainwin_vis_set_refresh(action - MAINWIN_VIS_REFRESH_FULL);
        break;
    case MAINWIN_VIS_AFALLOFF_SLOWEST:
    case MAINWIN_VIS_AFALLOFF_SLOW:
    case MAINWIN_VIS_AFALLOFF_MEDIUM:
    case MAINWIN_VIS_AFALLOFF_FAST:
    case MAINWIN_VIS_AFALLOFF_FASTEST:
        mainwin_vis_set_afalloff(action - MAINWIN_VIS_AFALLOFF_SLOWEST);
        break;
    case MAINWIN_VIS_PFALLOFF_SLOWEST:
    case MAINWIN_VIS_PFALLOFF_SLOW:
    case MAINWIN_VIS_PFALLOFF_MEDIUM:
    case MAINWIN_VIS_PFALLOFF_FAST:
    case MAINWIN_VIS_PFALLOFF_FASTEST:
        mainwin_vis_set_pfalloff(action - MAINWIN_VIS_PFALLOFF_SLOWEST);
        break;
    case MAINWIN_VIS_PLUGINS:
        show_prefs_window();
        prefswin_show_vis_plugins_page();
        break;
    }
}

static void mainwin_vis_plugin_menu_cb(GtkCheckMenuItem *item, gpointer data)
{
    enable_vis_plugin(GPOINTER_TO_INT(data), gtk_check_menu_item_get_active(item));
}

static void mainwin_clear_vis_plugin_menu_items(void)
{
    GList *node;

    for (node = mainwin_vis_plugin_items; node != NULL; node = node->next)
        gtk_widget_destroy(GTK_WIDGET(node->data));

    g_list_free(mainwin_vis_plugin_items);
    mainwin_vis_plugin_items = NULL;

    if (mainwin_vis_plugins_sep) {
        gtk_widget_destroy(mainwin_vis_plugins_sep);
        mainwin_vis_plugins_sep = NULL;
    }
}

void mainwin_vis_plugins_rescan(void)
{
    GList *plugins, *node;
    GList *children;
    gint i;
    gint insert_pos = -1;

    if (mainwin_vis_menu == NULL)
        return;

    mainwin_clear_vis_plugin_menu_items();

    plugins = get_vis_list();
    if (plugins == NULL)
        return;

    children = gtk_container_get_children(GTK_CONTAINER(mainwin_vis_menu));
    if (mainwin_vis_plugins_manage_item != NULL)
        insert_pos = g_list_index(children, mainwin_vis_plugins_manage_item);
    g_list_free(children);

    mainwin_vis_plugins_sep = gtk_separator_menu_item_new();
    if (insert_pos >= 0)
        gtk_menu_shell_insert(GTK_MENU_SHELL(mainwin_vis_menu), mainwin_vis_plugins_sep,
                              insert_pos++);
    else
        gtk_menu_shell_append(GTK_MENU_SHELL(mainwin_vis_menu), mainwin_vis_plugins_sep);
    gtk_widget_show(mainwin_vis_plugins_sep);

    for (node = plugins, i = 0; node != NULL; node = node->next, i++) {
        VisPlugin *plugin = node->data;
        GtkWidget *item;

        item = gtk_check_menu_item_new_with_label(plugin->description ? plugin->description : "");
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), vis_enabled(i));
        gtk_widget_set_sensitive(item, !vis_plugin_failed(i));
        g_signal_connect(item, "toggled", G_CALLBACK(mainwin_vis_plugin_menu_cb),
                         GINT_TO_POINTER(i));
        if (insert_pos >= 0)
            gtk_menu_shell_insert(GTK_MENU_SHELL(mainwin_vis_menu), item, insert_pos++);
        else
            gtk_menu_shell_append(GTK_MENU_SHELL(mainwin_vis_menu), item);
        gtk_widget_show(item);
        mainwin_vis_plugin_items = g_list_append(mainwin_vis_plugin_items, item);
    }
}

void mainwin_general_menu_callback(gpointer cb_data, guint action, GtkWidget *w)
{
    switch (action) {
    case MAINWIN_GENERAL_ABOUT:
        show_about_window();
        break;
    case MAINWIN_GENERAL_PLAYFILE:
        mainwin_eject_pushed();
        break;
    case MAINWIN_GENERAL_PLAYDIRECTORY:
        mainwin_show_dirbrowser();
        break;
    case MAINWIN_GENERAL_PLAYLOCATION:
        mainwin_show_add_url_window();
        break;
    case MAINWIN_GENERAL_FILEINFO:
        playlist_fileinfo_current();
        break;
    case MAINWIN_GENERAL_SHOWMWIN:
        if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
            mainwin_real_show();
        else
            mainwin_real_hide();
        break;
    case MAINWIN_GENERAL_SHOWPLWIN:
        if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
            playlistwin_real_show();
        else
            playlistwin_real_hide();
        break;
    case MAINWIN_GENERAL_SHOWEQWIN:
        if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
            equalizerwin_real_show();
        else
            equalizerwin_real_hide();
        break;
    case MAINWIN_GENERAL_PREV:
        playlist_prev();
        break;
    case MAINWIN_GENERAL_PLAY:
        mainwin_play_pushed();
        break;
    case MAINWIN_GENERAL_PAUSE:
        input_pause();
        break;
    case MAINWIN_GENERAL_STOP:
        mainwin_stop_pushed();
        break;
    case MAINWIN_GENERAL_NEXT:
        playlist_next();
        break;
    case MAINWIN_GENERAL_STOPFADE:
        break;
    case MAINWIN_GENERAL_BACK5SEC:
        if (get_input_playing() && playlist_get_current_length() != -1)
            input_seek((((input_get_time() / 1000) - 5 >= 0) ? (input_get_time() / 1000) - 5 : 0));
        break;
    case MAINWIN_GENERAL_FWD5SEC:
        if (get_input_playing() && playlist_get_current_length() != -1)
            input_seek(((((input_get_time() / 1000) + 5) < (playlist_get_current_length() / 1000))
                            ? ((input_get_time() / 1000) + 5)
                            : ((playlist_get_current_length() / 1000) - 1)));
        break;
    case MAINWIN_GENERAL_START:
        playlist_set_position(0);
        break;
    case MAINWIN_GENERAL_BACK10:
        playlist_set_position(
            (((get_playlist_position() - 10) >= 0) ? get_playlist_position() - 10 : 0));
        break;
    case MAINWIN_GENERAL_FWD10:
        playlist_set_position((((get_playlist_position() + 10) < get_playlist_length())
                                   ? (get_playlist_position() + 10)
                                   : (get_playlist_length() - 1)));
        break;
    case MAINWIN_GENERAL_JTT:
        mainwin_jump_to_time();
        break;
    case MAINWIN_GENERAL_JTF:
        mainwin_jump_to_file();
        break;
    case MAINWIN_GENERAL_CQUEUE:
        playlist_clear_queue();
        break;
    case MAINWIN_GENERAL_EXIT:
        mainwin_quit_cb();
        break;
    }
}

void mainwin_mr_change(MenuRowItem i)
{
    switch (i) {
    case MENUROW_NONE:
        mainwin_set_info_text();
        break;
    case MENUROW_OPTIONS:
        mainwin_lock_info_text(_("OPTIONS MENU"));
        break;
    case MENUROW_ALWAYS:
        if (!hint_always_on_top_available()) {
            if (mainwin_menurow->mr_always_selected)
                mainwin_lock_info_text(_("DISABLE ALWAYS ON TOP (N/A)"));
            else
                mainwin_lock_info_text(_("ENABLE ALWAYS ON TOP (N/A)"));
        } else if (mainwin_menurow->mr_always_selected)
            mainwin_lock_info_text(_("DISABLE ALWAYS ON TOP"));
        else
            mainwin_lock_info_text(_("ENABLE ALWAYS ON TOP"));
        break;
    case MENUROW_FILEINFOBOX:
        mainwin_lock_info_text(_("FILE INFO BOX"));
        break;
    case MENUROW_DOUBLESIZE:
        if (mainwin_menurow->mr_doublesize_selected)
            mainwin_lock_info_text(_("DISABLE DOUBLESIZE"));
        else
            mainwin_lock_info_text(_("ENABLE DOUBLESIZE"));
        break;
    case MENUROW_VISUALIZATION:
        mainwin_lock_info_text(_("VISUALIZATION MENU"));
        break;
    }
}

void mainwin_mr_release(MenuRowItem i)
{
    GtkWidget *widget;
    gint x, y;

    switch (i) {
    case MENUROW_OPTIONS:
        util_get_root_pointer(&x, &y);
        util_item_factory_popup(mainwin_options_menu, x, y, 1, GDK_CURRENT_TIME);
        break;
    case MENUROW_ALWAYS:
        if (mi_opt_always)
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi_opt_always),
                                           mainwin_menurow->mr_always_selected);
        break;
    case MENUROW_FILEINFOBOX:
        playlist_fileinfo_current();
        break;
    case MENUROW_DOUBLESIZE:
        if (mi_opt_doublesize)
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi_opt_doublesize),
                                           mainwin_menurow->mr_doublesize_selected);
        break;
    case MENUROW_VISUALIZATION:
        util_get_root_pointer(&x, &y);
        util_item_factory_popup(mainwin_vis_menu, x, y, 1, GDK_CURRENT_TIME);
        break;
    case MENUROW_NONE:
        break;
    }
    mainwin_release_info_text();
}

#define VOLSET_DISP_TIMES 5

void read_volume(gint when)
{
    int vl, vr, b, v;
    static int pvl = 0, pvr = 0, times = VOLSET_DISP_TIMES;
    static gboolean changing = FALSE;

    input_get_volume(&vl, &vr);
    if (when == VOLSET_STARTUP) {
        vl = CLAMP(vl, 0, 100);
        vr = CLAMP(vr, 0, 100);
        pvl = vl;
        pvr = vr;
        v = MAX(vl, vr);
        if (vl > vr)
            b = (gint)rint(((gdouble)vr / vl) * 100) - 100;
        else if (vl < vr)
            b = 100 - (gint)rint(((gdouble)vl / vr) * 100);
        else
            b = 0;

        balance = b;
        mainwin_set_volume_slider(v);
        equalizerwin_set_volume_slider(v);
        mainwin_set_balance_slider(b);
        equalizerwin_set_balance_slider(b);
        return;
    }

    else if (when == VOLSET_UPDATE) {
        if (vl == -1 || vr == -1)
            return;

        if (setting_volume) {
            pvl = vl;
            pvr = vr;
            return;
        } else if (pvr == vr && pvl == vl && changing) {
            if (times < VOLSET_DISP_TIMES)
                times++;
            else {
                mainwin_release_info_text();
                changing = FALSE;
            }
        } else if (pvr != vr || pvl != vl) {
            gchar *tmp;

            v = MAX(vl, vr);
            if (vl > vr)
                b = (gint)rint(((gdouble)vr / vl) * 100) - 100;
            else if (vl < vr)
                b = 100 - (gint)rint(((gdouble)vl / vr) * 100);
            else
                b = 0;

            if (MAX(vl, vr) != MAX(pvl, pvr))
                tmp = g_strdup_printf(_("VOLUME: %d%%"), v);
            else {
                if (vl > vr) {
                    tmp = g_strdup_printf(_("BALANCE: %d%% LEFT"), -b);
                } else if (vr == vl)
                    tmp = g_strdup_printf(_("BALANCE: CENTER"));
                else /* (vl < vr) */
                {
                    tmp = g_strdup_printf(_("BALANCE: %d%% RIGHT"), b);
                }
            }
            mainwin_lock_info_text(tmp);
            g_free(tmp);

            pvr = vr;
            pvl = vl;
            times = 0;
            changing = TRUE;
            mainwin_set_volume_slider(v);
            equalizerwin_set_volume_slider(v);

            /*
             * Don't change the balance slider if the volume has been
             * set to zero.  The balance can be anything, and our best
             * guess is what is was before.
             */
            if (v > 0) {
                balance = b;
                mainwin_set_balance_slider(b);
                equalizerwin_set_balance_slider(b);
            }
        }
    } else if (when == VOLUME_ADJUSTED) {
        pvl = vl;
        pvr = vr;
    } else if (when == VOLUME_SET) {
        times = 0;
        changing = TRUE;
        pvl = vl;
        pvr = vr;
    }
}

/* ---- GTK3 menu builder helpers ---- */
typedef struct {
    GCallback cb;
    guint action;
} MenuCbData;

void menu_activate_cb(GtkMenuItem *item, gpointer data)
{
    MenuCbData *cbd = data;
    if (cbd->cb)
        ((void (*)(gpointer, guint, GtkWidget *))cbd->cb)(NULL, cbd->action, GTK_WIDGET(item));
}

GtkWidget *menu_item_new(GtkWidget *m, const char *label, GCallback cb, guint action)
{
    MenuCbData *cbd = g_new0(MenuCbData, 1);
    cbd->cb = cb;
    cbd->action = action;
    GtkWidget *item = gtk_menu_item_new_with_label(_(label));
    g_signal_connect(item, "activate", G_CALLBACK(menu_activate_cb), cbd);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), item);
    gtk_widget_show(item);
    return item;
}

GtkWidget *menu_check_new(GtkWidget *m, const char *label, gboolean active, GCallback cb,
                          guint action)
{
    MenuCbData *cbd = g_new0(MenuCbData, 1);
    cbd->cb = cb;
    cbd->action = action;
    GtkWidget *item = gtk_check_menu_item_new_with_label(_(label));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), active);
    g_signal_connect(item, "toggled", G_CALLBACK(menu_activate_cb), cbd);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), item);
    gtk_widget_show(item);
    return item;
}

GtkWidget *menu_radio_new(GtkWidget *m, const char *label, GSList **grp, gboolean active,
                          GCallback cb, guint action)
{
    MenuCbData *cbd = g_new0(MenuCbData, 1);
    cbd->cb = cb;
    cbd->action = action;
    GtkWidget *item = gtk_radio_menu_item_new_with_label(*grp, _(label));
    *grp = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), active);
    g_signal_connect(item, "activate", G_CALLBACK(menu_activate_cb), cbd);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), item);
    gtk_widget_show(item);
    return item;
}

void menu_sep_new(GtkWidget *m)
{
    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(m), sep);
    gtk_widget_show(sep);
}

GtkWidget *menu_sub_new(GtkWidget *m, const char *label)
{
    GtkWidget *sub = gtk_menu_new();
    GtkWidget *item = gtk_menu_item_new_with_label(_(label));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), sub);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), item);
    gtk_widget_show(item);
    return sub;
}
/* ---- end menu helpers ---- */

void create_popups(void)
{
    GSList *grp;
    GtkWidget *sub;

    /* ---- Options menu ---- */
    mainwin_options_menu = gtk_menu_new();
    menu_item_new(mainwin_options_menu, "Preferences", G_CALLBACK(mainwin_options_menu_callback),
                  MAINWIN_OPT_PREFS);
    menu_item_new(mainwin_options_menu, "Skin Browser", G_CALLBACK(mainwin_options_menu_callback),
                  MAINWIN_OPT_SKIN);
    menu_item_new(mainwin_options_menu, "Reload skin", G_CALLBACK(mainwin_options_menu_callback),
                  MAINWIN_OPT_RELOADSKIN);
    menu_sep_new(mainwin_options_menu);
    mi_opt_repeat = menu_check_new(mainwin_options_menu, "Repeat", cfg.repeat,
                                   G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_REPEAT);
    mi_opt_shuffle = menu_check_new(mainwin_options_menu, "Shuffle", cfg.shuffle,
                                    G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_SHUFFLE);
    mi_opt_npa =
        menu_check_new(mainwin_options_menu, "No Playlist Advance", cfg.no_playlist_advance,
                       G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_NPA);
    menu_sep_new(mainwin_options_menu);
    grp = NULL;
    mi_opt_telapsed =
        menu_radio_new(mainwin_options_menu, "Time Elapsed", &grp, cfg.timer_mode == TIMER_ELAPSED,
                       G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_TELAPSED);
    mi_opt_tremaining =
        menu_radio_new(mainwin_options_menu, "Time Remaining", &grp,
                       cfg.timer_mode == TIMER_REMAINING, G_CALLBACK(mainwin_options_menu_callback),
                       MAINWIN_OPT_TREMAINING);
    mi_opt_tdisplay =
        menu_check_new(mainwin_options_menu, "Time Display (MMM:SS)", cfg.timer_minutes_only,
                       G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_TDISPLAY);
    menu_sep_new(mainwin_options_menu);
    mi_opt_always = menu_check_new(mainwin_options_menu, "Always On Top", cfg.always_on_top,
                                   G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_ALWAYS);
    mi_opt_sticky = menu_check_new(mainwin_options_menu, "Show on all desktops", cfg.sticky,
                                   G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_STICKY);
    mi_opt_ws = menu_check_new(mainwin_options_menu, "WindowShade Mode", cfg.player_shaded,
                               G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_WS);
    mi_opt_pws =
        menu_check_new(mainwin_options_menu, "Playlist WindowShade Mode", cfg.playlist_shaded,
                       G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_PWS);
    mi_opt_eqws =
        menu_check_new(mainwin_options_menu, "Equalizer WindowShade Mode", cfg.equalizer_shaded,
                       G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_EQWS);
    mi_opt_doublesize =
        menu_check_new(mainwin_options_menu, "DoubleSize", cfg.doublesize,
                       G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_DOUBLESIZE);
    mi_opt_easymove =
        menu_check_new(mainwin_options_menu, "Easy Move", cfg.easy_move,
                       G_CALLBACK(mainwin_options_menu_callback), MAINWIN_OPT_EASY_MOVE);

    /* ---- Songname menu ---- */
    mainwin_songname_menu = gtk_menu_new();
    menu_item_new(mainwin_songname_menu, "File Info", G_CALLBACK(mainwin_songname_menu_callback),
                  MAINWIN_SONGNAME_FILEINFO);
    menu_item_new(mainwin_songname_menu, "Jump To File", G_CALLBACK(mainwin_songname_menu_callback),
                  MAINWIN_SONGNAME_JTF);
    menu_item_new(mainwin_songname_menu, "Jump To Time", G_CALLBACK(mainwin_songname_menu_callback),
                  MAINWIN_SONGNAME_JTT);
    mi_sn_autoscroll =
        menu_check_new(mainwin_songname_menu, "Autoscroll Song Name", cfg.autoscroll,
                       G_CALLBACK(mainwin_songname_menu_callback), MAINWIN_SONGNAME_SCROLL);

    /* ---- Vis menu ---- */
    mainwin_vis_menu = gtk_menu_new();
    sub = menu_sub_new(mainwin_vis_menu, "Visualization Mode");
    grp = NULL;
    mi_vis_analyzer = menu_radio_new(sub, "Analyzer", &grp, cfg.vis_type == VIS_ANALYZER,
                                     G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_ANALYZER);
    mi_vis_scope = menu_radio_new(sub, "Scope", &grp, cfg.vis_type == VIS_SCOPE,
                                  G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_SCOPE);
    mi_vis_off = menu_radio_new(sub, "Off", &grp, cfg.vis_type == VIS_OFF,
                                G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_OFF);

    sub = menu_sub_new(mainwin_vis_menu, "Analyzer Mode");
    grp = NULL;
    menu_radio_new(sub, "Normal", &grp, cfg.analyzer_type == ANALYZER_NORMAL,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_ANALYZER_NORMAL);
    menu_radio_new(sub, "Fire", &grp, cfg.analyzer_type == ANALYZER_FIRE,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_ANALYZER_FIRE);
    menu_radio_new(sub, "Vertical Lines", &grp, cfg.analyzer_type == ANALYZER_VLINES,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_ANALYZER_VLINES);
    menu_sep_new(sub);
    grp = NULL;
    menu_radio_new(sub, "Lines", &grp, cfg.analyzer_type == ANALYZER_LINES,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_ANALYZER_LINES);
    menu_radio_new(sub, "Bars", &grp, cfg.analyzer_type == ANALYZER_BARS,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_ANALYZER_BARS);
    menu_sep_new(sub);
    menu_check_new(sub, "Peaks", cfg.analyzer_peaks, G_CALLBACK(mainwin_vis_menu_callback),
                   MAINWIN_VIS_ANALYZER_PEAKS);

    sub = menu_sub_new(mainwin_vis_menu, "Scope Mode");
    grp = NULL;
    menu_radio_new(sub, "Dot Scope", &grp, cfg.scope_mode == SCOPE_DOT,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_SCOPE_DOT);
    menu_radio_new(sub, "Line Scope", &grp, cfg.scope_mode == SCOPE_LINE,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_SCOPE_LINE);
    menu_radio_new(sub, "Solid Scope", &grp, cfg.scope_mode == SCOPE_SOLID,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_SCOPE_SOLID);

    sub = menu_sub_new(mainwin_vis_menu, "WindowShade VU Mode");
    grp = NULL;
    menu_radio_new(sub, "Normal", &grp, cfg.vu_mode == VU_NORMAL,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_VU_NORMAL);
    menu_radio_new(sub, "Smooth", &grp, cfg.vu_mode == VU_SMOOTH,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_VU_SMOOTH);

    sub = menu_sub_new(mainwin_vis_menu, "Refresh Rate");
    grp = NULL;
    menu_radio_new(sub, "Full (~50 fps)", &grp, cfg.vis_refresh == REFRESH_FULL,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_REFRESH_FULL);
    menu_radio_new(sub, "Half (~25 fps)", &grp, cfg.vis_refresh == REFRESH_HALF,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_REFRESH_HALF);
    menu_radio_new(sub, "Quarter (~13 fps)", &grp, cfg.vis_refresh == REFRESH_QUARTER,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_REFRESH_QUARTER);
    menu_radio_new(sub, "Eighth (~6 fps)", &grp, cfg.vis_refresh == REFRESH_EIGTH,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_REFRESH_EIGHTH);

    sub = menu_sub_new(mainwin_vis_menu, "Analyzer Falloff");
    grp = NULL;
    menu_radio_new(sub, "Slowest", &grp, cfg.analyzer_falloff == FALLOFF_SLOWEST,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_AFALLOFF_SLOWEST);
    menu_radio_new(sub, "Slow", &grp, cfg.analyzer_falloff == FALLOFF_SLOW,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_AFALLOFF_SLOW);
    menu_radio_new(sub, "Medium", &grp, cfg.analyzer_falloff == FALLOFF_MEDIUM,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_AFALLOFF_MEDIUM);
    menu_radio_new(sub, "Fast", &grp, cfg.analyzer_falloff == FALLOFF_FAST,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_AFALLOFF_FAST);
    menu_radio_new(sub, "Fastest", &grp, cfg.analyzer_falloff == FALLOFF_FASTEST,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_AFALLOFF_FASTEST);

    sub = menu_sub_new(mainwin_vis_menu, "Peak Falloff");
    grp = NULL;
    menu_radio_new(sub, "Slowest", &grp, cfg.peaks_falloff == FALLOFF_SLOWEST,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_PFALLOFF_SLOWEST);
    menu_radio_new(sub, "Slow", &grp, cfg.peaks_falloff == FALLOFF_SLOW,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_PFALLOFF_SLOW);
    menu_radio_new(sub, "Medium", &grp, cfg.peaks_falloff == FALLOFF_MEDIUM,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_PFALLOFF_MEDIUM);
    menu_radio_new(sub, "Fast", &grp, cfg.peaks_falloff == FALLOFF_FAST,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_PFALLOFF_FAST);
    menu_radio_new(sub, "Fastest", &grp, cfg.peaks_falloff == FALLOFF_FASTEST,
                   G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_PFALLOFF_FASTEST);

    mainwin_vis_plugins_rescan();
    mainwin_vis_plugins_manage_item =
        menu_item_new(mainwin_vis_menu, "Visualization Plugins",
                      G_CALLBACK(mainwin_vis_menu_callback), MAINWIN_VIS_PLUGINS);

    /* ---- General menu ---- */
    mainwin_general_menu = gtk_menu_new();
    menu_item_new(mainwin_general_menu, "About XMMS", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_ABOUT);
    menu_sep_new(mainwin_general_menu);
    menu_item_new(mainwin_general_menu, "Play File", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_PLAYFILE);
    menu_item_new(mainwin_general_menu, "Play Directory", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_PLAYDIRECTORY);
    menu_item_new(mainwin_general_menu, "Play Location", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_PLAYLOCATION);
    menu_item_new(mainwin_general_menu, "View File Info", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_FILEINFO);
    menu_sep_new(mainwin_general_menu);
    mi_gen_showmwin =
        menu_check_new(mainwin_general_menu, "Main Window", cfg.player_visible,
                       G_CALLBACK(mainwin_general_menu_callback), MAINWIN_GENERAL_SHOWMWIN);
    mi_gen_showplwin =
        menu_check_new(mainwin_general_menu, "Playlist Editor", cfg.playlist_visible,
                       G_CALLBACK(mainwin_general_menu_callback), MAINWIN_GENERAL_SHOWPLWIN);
    mi_gen_showeqwin =
        menu_check_new(mainwin_general_menu, "Graphical EQ", cfg.equalizer_visible,
                       G_CALLBACK(mainwin_general_menu_callback), MAINWIN_GENERAL_SHOWEQWIN);
    menu_sep_new(mainwin_general_menu);
    {
        GtkWidget *opts_item = gtk_menu_item_new_with_label(_("Options"));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(opts_item), mainwin_options_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(mainwin_general_menu), opts_item);
        gtk_widget_show(opts_item);
    }
    sub = menu_sub_new(mainwin_general_menu, "Playback");
    menu_item_new(sub, "Previous", G_CALLBACK(mainwin_general_menu_callback), MAINWIN_GENERAL_PREV);
    menu_item_new(sub, "Play", G_CALLBACK(mainwin_general_menu_callback), MAINWIN_GENERAL_PLAY);
    menu_item_new(sub, "Pause", G_CALLBACK(mainwin_general_menu_callback), MAINWIN_GENERAL_PAUSE);
    menu_item_new(sub, "Stop", G_CALLBACK(mainwin_general_menu_callback), MAINWIN_GENERAL_STOP);
    menu_item_new(sub, "Next", G_CALLBACK(mainwin_general_menu_callback), MAINWIN_GENERAL_NEXT);
    menu_sep_new(sub);
    menu_item_new(sub, "Back 5 Seconds", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_BACK5SEC);
    menu_item_new(sub, "Fwd 5 Seconds", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_FWD5SEC);
    menu_item_new(sub, "Start of List", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_START);
    menu_item_new(sub, "10 Tracks Back", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_BACK10);
    menu_item_new(sub, "10 Tracks Fwd", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_FWD10);
    menu_sep_new(sub);
    menu_item_new(sub, "Jump to Time", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_JTT);
    menu_item_new(sub, "Jump to File", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_JTF);
    menu_item_new(sub, "Clear Queue", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_CQUEUE);
    {
        GtkWidget *vis_item = gtk_menu_item_new_with_label(_("Visualization"));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(vis_item), mainwin_vis_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(mainwin_general_menu), vis_item);
        gtk_widget_show(vis_item);
    }
    menu_sep_new(mainwin_general_menu);
    menu_item_new(mainwin_general_menu, "Exit", G_CALLBACK(mainwin_general_menu_callback),
                  MAINWIN_GENERAL_EXIT);
}

static void mainwin_set_icon(GtkWidget *win)
{
    /* Load our installed icon directly, bypassing the GTK theme chain.
     * gtk_window_set_icon_name("xmms") would hit the Humanity theme's
     * legacy xmms.svg before reaching our hicolor PNGs. */
    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(
        SHARE_DIR "/icons/hicolor/48x48/apps/xmms.png", &err);
    if (pb) {
        gtk_window_set_icon(GTK_WINDOW(win), pb);
        g_object_unref(pb);
    } else {
        if (err) g_error_free(err);
        /* fallback: let the WM pick whatever it finds */
        gtk_window_set_icon_name(GTK_WINDOW(win), PACKAGE);
    }
}

static void mainwin_create_widgets(void)
{
    /* mainwin_bg and mainwin_cr are created in mainwin_create() */

    mainwin_menubtn = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 6, 3, 9, 9, 0, 0, 0, 9,
                                     mainwin_menubtn_cb, SKIN_TITLEBAR);
    mainwin_menubtn->pb_allow_draw = TRUE;
    mainwin_minimize = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 244, 3, 9, 9, 9, 0, 9,
                                      9, mainwin_minimize_cb, SKIN_TITLEBAR);
    mainwin_minimize->pb_allow_draw = TRUE;
    mainwin_shade = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 254, 3, 9, 9, 0,
                                   cfg.player_shaded ? 27 : 18, 9, cfg.player_shaded ? 27 : 18,
                                   mainwin_shade_toggle, SKIN_TITLEBAR);
    mainwin_shade->pb_allow_draw = TRUE;
    mainwin_close = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 264, 3, 9, 9, 18, 0, 18,
                                   9, mainwin_quit_cb, SKIN_TITLEBAR);
    mainwin_close->pb_allow_draw = TRUE;

    mainwin_rew = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 16, 88, 23, 18, 0, 0, 0,
                                 18, playlist_prev, SKIN_CBUTTONS);
    mainwin_play = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 39, 88, 23, 18, 23, 0, 23,
                                  18, mainwin_play_pushed, SKIN_CBUTTONS);
    mainwin_pause = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 62, 88, 23, 18, 46, 0,
                                   46, 18, input_pause, SKIN_CBUTTONS);
    mainwin_stop = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 85, 88, 23, 18, 69, 0, 69,
                                  18, mainwin_stop_pushed, SKIN_CBUTTONS);
    mainwin_fwd = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 108, 88, 22, 18, 92, 0, 92,
                                 18, playlist_next, SKIN_CBUTTONS);
    mainwin_eject = create_pbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 136, 89, 22, 16, 114, 0,
                                   114, 16, mainwin_eject_pushed, SKIN_CBUTTONS);

    mainwin_srew =
        create_sbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 169, 4, 8, 7, playlist_prev);
    mainwin_splay =
        create_sbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 177, 4, 10, 7, mainwin_play_pushed);
    mainwin_spause =
        create_sbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 187, 4, 10, 7, input_pause);
    mainwin_sstop =
        create_sbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 197, 4, 9, 7, mainwin_stop_pushed);
    mainwin_sfwd =
        create_sbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 206, 4, 8, 7, playlist_next);
    mainwin_seject =
        create_sbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 216, 4, 9, 7, mainwin_eject_pushed);

    mainwin_shuffle = create_tbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 164, 89, 46, 15, 28, 0,
                                     28, 15, 28, 30, 28, 45, mainwin_shuffle_pushed, SKIN_SHUFREP);
    mainwin_repeat = create_tbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 210, 89, 28, 15, 0, 0,
                                    0, 15, 0, 30, 0, 45, mainwin_repeat_pushed, SKIN_SHUFREP);

    mainwin_eq = create_tbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 219, 58, 23, 12, 0, 61, 46,
                                61, 0, 73, 46, 73, equalizerwin_show, SKIN_SHUFREP);
    tbutton_set_toggled(mainwin_eq, cfg.equalizer_visible);
    mainwin_pl = create_tbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 242, 58, 23, 12, 23, 61, 69,
                                61, 23, 73, 69, 73, mainwin_pl_pushed, SKIN_SHUFREP);
    tbutton_set_toggled(mainwin_pl, cfg.playlist_visible);

    mainwin_info =
        create_textbox(&mainwin_wlist, mainwin_bg, mainwin_cr, 112, 27, 153, 1, SKIN_TEXT);
    textbox_set_scroll(mainwin_info, cfg.autoscroll);
    textbox_set_xfont(mainwin_info, cfg.mainwin_use_xfont, cfg.mainwin_font);
    mainwin_rate_text =
        create_textbox(&mainwin_wlist, mainwin_bg, mainwin_cr, 111, 43, 15, 0, SKIN_TEXT);
    mainwin_freq_text =
        create_textbox(&mainwin_wlist, mainwin_bg, mainwin_cr, 156, 43, 10, 0, SKIN_TEXT);

    mainwin_menurow = create_menurow(&mainwin_wlist, mainwin_bg, mainwin_cr, 10, 22, 304, 0, 304,
                                     44, mainwin_mr_change, mainwin_mr_release, SKIN_TITLEBAR);
    mainwin_menurow->mr_doublesize_selected = cfg.doublesize;
    mainwin_menurow->mr_always_selected = cfg.always_on_top;

    mainwin_volume = create_hslider(&mainwin_wlist, mainwin_bg, mainwin_cr, 107, 57, 68, 13, 15,
                                    422, 0, 422, 14, 11, 15, 0, 0, 51, mainwin_volume_framecb,
                                    mainwin_volume_motioncb, mainwin_volume_releasecb, SKIN_VOLUME);
    mainwin_balance =
        create_hslider(&mainwin_wlist, mainwin_bg, mainwin_cr, 177, 57, 38, 13, 15, 422, 0, 422, 14,
                       11, 15, 9, 0, 24, mainwin_balance_framecb, mainwin_balance_motioncb,
                       mainwin_balance_releasecb, SKIN_BALANCE);

    mainwin_monostereo =
        create_monostereo(&mainwin_wlist, mainwin_bg, mainwin_cr, 212, 41, SKIN_MONOSTEREO);

    mainwin_playstatus = create_playstatus(&mainwin_wlist, mainwin_bg, mainwin_cr, 24, 28);

    mainwin_minus_num = create_number(&mainwin_wlist, mainwin_bg, mainwin_cr, 36, 26, SKIN_NUMBERS);
    hide_widget(mainwin_minus_num);
    mainwin_10min_num = create_number(&mainwin_wlist, mainwin_bg, mainwin_cr, 48, 26, SKIN_NUMBERS);
    hide_widget(mainwin_10min_num);
    mainwin_min_num = create_number(&mainwin_wlist, mainwin_bg, mainwin_cr, 60, 26, SKIN_NUMBERS);
    hide_widget(mainwin_min_num);
    mainwin_10sec_num = create_number(&mainwin_wlist, mainwin_bg, mainwin_cr, 78, 26, SKIN_NUMBERS);
    hide_widget(mainwin_10sec_num);
    mainwin_sec_num = create_number(&mainwin_wlist, mainwin_bg, mainwin_cr, 90, 26, SKIN_NUMBERS);
    hide_widget(mainwin_sec_num);

    mainwin_about =
        create_sbutton(&mainwin_wlist, mainwin_bg, mainwin_cr, 247, 83, 20, 25, show_about_window);

    mainwin_vis = create_vis(&mainwin_wlist, mainwin_bg, gtk_widget_get_window(mainwin), mainwin_cr,
                             24, 43, 76, cfg.doublesize);
    mainwin_svis = create_svis(&mainwin_wlist, mainwin_bg, mainwin_cr, 79, 5);
    active_vis = mainwin_vis;

    mainwin_position =
        create_hslider(&mainwin_wlist, mainwin_bg, mainwin_cr, 16, 72, 248, 10, 248, 0, 278, 0, 29,
                       10, 10, 0, 0, 219, NULL, mainwin_position_motioncb,
                       mainwin_position_releasecb, SKIN_POSBAR);
    hide_widget(mainwin_position);

    mainwin_sposition =
        create_hslider(&mainwin_wlist, mainwin_bg, mainwin_cr, 226, 4, 17, 7, 17, 36, 17, 36, 3, 7,
                       36, 0, 1, 13, mainwin_spos_frame_cb, mainwin_spos_motion_cb,
                       mainwin_spos_release_cb, SKIN_TITLEBAR);
    hide_widget(mainwin_sposition);

    mainwin_stime_min =
        create_textbox(&mainwin_wlist, mainwin_bg, mainwin_cr, 130, 4, 15, FALSE, SKIN_TEXT);
    mainwin_stime_sec =
        create_textbox(&mainwin_wlist, mainwin_bg, mainwin_cr, 147, 4, 10, FALSE, SKIN_TEXT);

    if (!cfg.player_shaded) {
        hide_widget(mainwin_svis);
        hide_widget(mainwin_srew);
        hide_widget(mainwin_splay);
        hide_widget(mainwin_spause);
        hide_widget(mainwin_sstop);
        hide_widget(mainwin_sfwd);
        hide_widget(mainwin_seject);
        hide_widget(mainwin_stime_min);
        hide_widget(mainwin_stime_sec);
    }
}

/* GTK3: blit backing surface to window on every expose/draw */
/* -------------------------------------------------------------------------
 * Snap vtable implementation — allows vis plugin DSOs (which cannot link
 * directly against main) to join the XMMS dock/snap group via libxmms.
 *
 * Five thin wrappers forward into dock_move_press / motion / release and
 * manage the shared dock_window_list.  Registered once in mainwin_create_gtk()
 * so the vtable is live before any vis plugin window can be created.
 * Refs #34
 * ----------------------------------------------------------------------- */
static void snap_register_impl(GtkWidget *w)
{
    dock_window_list = dock_add_window(dock_window_list, w);
}

static void snap_unregister_impl(GtkWidget *w)
{
    dock_window_list = g_list_remove(dock_window_list, w);
}

static void snap_move_press_impl(GtkWidget *w, GdkEventButton *ev)
{
    dock_move_press(dock_window_list, w, ev, FALSE);
}

static void snap_move_motion_impl(GtkWidget *w, GdkEventMotion *ev)
{
    dock_move_motion(w, ev);
}

static void snap_move_release_impl(GtkWidget *w)
{
    dock_move_release(w);
}

static void vis_snap_init(void)
{
    xmms_snap.register_window = snap_register_impl;
    xmms_snap.unregister_window = snap_unregister_impl;
    xmms_snap.move_press = snap_move_press_impl;
    xmms_snap.move_motion = snap_move_motion_impl;
    xmms_snap.move_release = snap_move_release_impl;
}

static gboolean mainwin_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cairo_surface_t *surface;

    (void)widget;
    (void)data;
    surface = (cfg.doublesize && mainwin_bg_dblsize) ? mainwin_bg_dblsize : mainwin_bg;
    if (surface) {
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_paint(cr);
    }
    return FALSE;
}

static void mainwin_create_gtk(void)
{
    mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    dock_window_list = dock_add_window(dock_window_list, mainwin);
    /* Register snap vtable for vis plugin DSOs — Refs #34 */
    vis_snap_init();
    gtk_widget_set_app_paintable(mainwin, TRUE);
    gtk_window_set_title(GTK_WINDOW(mainwin), "XMMS");
    gtk_window_set_resizable(GTK_WINDOW(mainwin), FALSE); /* TODO(#gtk3): was set_policy */
    gtk_window_set_wmclass(GTK_WINDOW(mainwin), "XMMS_Player", "xmms");

    gtk_widget_set_events(mainwin, GDK_FOCUS_CHANGE_MASK | GDK_BUTTON_MOTION_MASK |
                                       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                       GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK |
                                       GDK_STRUCTURE_MASK);
    if (cfg.player_x != -1 && cfg.save_window_position)
        dock_set_uposition(mainwin, cfg.player_x, cfg.player_y);
    gtk_widget_realize(mainwin);

    mainwin_set_icon(mainwin);
    util_set_cursor(mainwin);

    if (cfg.doublesize)
        gtk_widget_set_size_request(mainwin, 550, cfg.player_shaded ? 28 : 232);
    else
        gtk_widget_set_size_request(mainwin, 275, cfg.player_shaded ? 14 : 116);
    if (!cfg.show_wm_decorations)
        gdk_window_set_decorations(gtk_widget_get_window(mainwin), 0);
    gtk_window_add_accel_group(GTK_WINDOW(mainwin), mainwin_accel);

    g_signal_connect(G_OBJECT(mainwin), "destroy", G_CALLBACK(mainwin_destroy), NULL);
    g_signal_connect(G_OBJECT(mainwin), "button_press_event", G_CALLBACK(mainwin_press), NULL);
    g_signal_connect(G_OBJECT(mainwin), "button_release_event", G_CALLBACK(mainwin_release), NULL);
    g_signal_connect(G_OBJECT(mainwin), "motion_notify_event", G_CALLBACK(mainwin_motion), NULL);
    g_signal_connect(G_OBJECT(mainwin), "focus_in_event", G_CALLBACK(mainwin_focus_in), NULL);
    g_signal_connect(G_OBJECT(mainwin), "focus_out_event", G_CALLBACK(mainwin_focus_out), NULL);
    g_signal_connect(G_OBJECT(mainwin), "configure_event", G_CALLBACK(mainwin_configure), NULL);
    /* GTK3: client_event signal removed - X11 ClientMessage handled differently */
    xmms_drag_dest_set(mainwin);
    g_signal_connect(G_OBJECT(mainwin), "drag-data-received",
                     G_CALLBACK(mainwin_drag_data_received), NULL);
    g_signal_connect(G_OBJECT(mainwin), "key-press-event", G_CALLBACK(mainwin_keypress), NULL);
    /* GTK3: scroll-event delivers mouse-wheel input; replaces button 4/5 in press handler */
    g_signal_connect(G_OBJECT(mainwin), "scroll-event", G_CALLBACK(mainwin_scroll_cb), NULL);
    /* GTK3: blit backing surface on every redraw */
    g_signal_connect(G_OBJECT(mainwin), "draw", G_CALLBACK(mainwin_draw_cb), NULL);

    gdk_window_set_group(gtk_widget_get_window(mainwin), gtk_widget_get_window(mainwin));
    mainwin_set_back_pixmap();
}

void mainwin_create(void)
{
    gint win_h = cfg.player_shaded ? 14 : 116;
    mainwin_bg = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 275, win_h);
    mainwin_cr = cairo_create(mainwin_bg);
    mainwin_bg_dblsize = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 550, 232);
    mainwin_create_gtk();
    mainwin_create_widgets();
}

void mainwin_recreate(void)
{
    g_signal_handlers_disconnect_by_func(G_OBJECT(mainwin), mainwin_destroy, NULL);
    dock_window_list = g_list_remove(dock_window_list, mainwin);
    gtk_widget_destroy(mainwin);
    mainwin_create_gtk();
    vis_set_window(mainwin_vis, gtk_widget_get_window(mainwin));
    mainwin_set_shape_mask();
    draw_main_window(TRUE);
}


void set_timer_mode(TimerMode mode)
{
    GtkWidget *mi = (mode == TIMER_ELAPSED) ? mi_opt_telapsed : mi_opt_tremaining;
    if (mi)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), TRUE);
}

static void set_timer_mode_menu_cb(TimerMode mode)
{
    cfg.timer_mode = mode;
}

static void output_failed(void)
{
    static GtkWidget *infobox;
    if (!infobox) {
        infobox = xmms_show_message(_("Couldn't open audio"),
                                    _("Please check that:\n\n"
                                      "Your soundcard is configured properly\n"
                                      "You have the correct output plugin selected\n"
                                      "No other program is blocking the soundcard"),
                                    _("OK"), FALSE, NULL, NULL);
        g_signal_connect(G_OBJECT(infobox), "destroy", G_CALLBACK(on_widget_destroyed), &infobox);
    } else
        gdk_window_raise(gtk_widget_get_window(infobox));
}

gint idle_func(gpointer data)
{
    gint time, t, length;
    gchar stime_prefix, *tmp;
    static gboolean waiting = FALSE;
    static gint count = 0;

    static GTimer *pause_timer = NULL;

    if (get_input_playing()) {
        GDK_THREADS_ENTER();
        vis_playback_start();
        GDK_THREADS_LEAVE();
        time = input_get_time();
        if (time == -1) {
            if (cfg.pause_between_songs) {
                gint timeleft;
                if (!waiting) {
                    if (!pause_timer)
                        pause_timer = g_timer_new();
                    else
                        g_timer_reset(pause_timer);
                    waiting = TRUE;
                }
                timeleft = cfg.pause_between_songs_time - (gint)g_timer_elapsed(pause_timer, NULL);
                number_set_number(mainwin_10min_num, timeleft / 600);
                number_set_number(mainwin_min_num, (timeleft / 60) % 10);
                number_set_number(mainwin_10sec_num, (timeleft / 10) % 6);
                number_set_number(mainwin_sec_num, timeleft % 10);
                if (!mainwin_sposition->hs_pressed) {
                    char temp[3];

                    sprintf(temp, "%2.2d", timeleft / 60);
                    textbox_set_text(mainwin_stime_min, temp);
                    sprintf(temp, "%2.2d", timeleft % 60);
                    textbox_set_text(mainwin_stime_sec, temp);
                }

                playlistwin_set_time(timeleft * 1000, 0, TIMER_ELAPSED);
            }

            if (!cfg.pause_between_songs ||
                g_timer_elapsed(pause_timer, NULL) >= cfg.pause_between_songs_time) {
                GDK_THREADS_ENTER();
                playlist_eof_reached();
                GDK_THREADS_LEAVE();
                waiting = FALSE;
            }
        } else if (time == -2) {
            GDK_THREADS_ENTER();
            output_failed();
            mainwin_stop_pushed();
            GDK_THREADS_LEAVE();
        } else {
            length = playlist_get_current_length();
            playlistwin_set_time(time, length, cfg.timer_mode);
            input_update_vis(time);

            if (cfg.timer_mode == TIMER_REMAINING) {
                if (length != -1) {
                    number_set_number(mainwin_minus_num, 11);
                    t = length - time;
                    stime_prefix = '-';
                } else {
                    number_set_number(mainwin_minus_num, 10);
                    t = time;
                    stime_prefix = ' ';
                }
            } else {
                number_set_number(mainwin_minus_num, 10);
                t = time;
                stime_prefix = ' ';
            }
            t /= 1000;

            if (!cfg.timer_minutes_only) {
                /*
                 * Show the time in the format HH:MM when we
                 * have more than 100 minutes.
                 */
                if (t >= 100 * 60)
                    t /= 60;
                number_set_number(mainwin_10min_num, t / 600);
                number_set_number(mainwin_min_num, (t / 60) % 10);
                number_set_number(mainwin_10sec_num, (t / 10) % 6);
                number_set_number(mainwin_sec_num, t % 10);
            } else {
                /*
                 * Show the time in the format MMM:SS when
                 * cfg.timer_minutes_only = TRUE.
                 */
                if (((t / (100 * 60)) % 10) > 0) {
                    number_set_number(mainwin_minus_num, (t / (100 * 60)) % 10);
                }
                number_set_number(mainwin_10min_num, (t / (10 * 60)) % 10);
                number_set_number(mainwin_min_num, (t / 60) % 10);
                number_set_number(mainwin_10sec_num, (t / 10) % 6);
                number_set_number(mainwin_sec_num, t % 10);
            }

            if (!mainwin_sposition->hs_pressed) {
                tmp = g_strdup_printf("%c%2.2d", stime_prefix, t / 60);
                textbox_set_text(mainwin_stime_min, tmp);
                g_free(tmp);

                tmp = g_strdup_printf("%2.2d", t % 60);
                textbox_set_text(mainwin_stime_sec, tmp);
                g_free(tmp);
            }

            time /= 1000;
            length /= 1000;
            if (length > 0) {
                if (time > length) {
                    hslider_set_position(mainwin_position, 219);
                    hslider_set_position(mainwin_sposition, 13);
                } else {
                    hslider_set_position(mainwin_position, (time * 219) / length);
                    hslider_set_position(mainwin_sposition, ((time * 12) / length) + 1);
                }
            } else {
                hslider_set_position(mainwin_position, 0);
                hslider_set_position(mainwin_sposition, 1);
            }
        }

        if (time != -1)
            waiting = FALSE;
    } else {
        GDK_THREADS_ENTER();
        vis_playback_stop();
        GDK_THREADS_LEAVE();
    }


    GDK_THREADS_ENTER();
    check_ctrlsocket();

    if (!count) {
        read_volume(VOLSET_UPDATE);
        count = 10;
    } else
        count--;

    if (mainwin_title_text) {
        pthread_mutex_lock(&title_mutex);
        mainwin_update_song_info();
        gtk_window_set_title(GTK_WINDOW(mainwin), mainwin_title_text);
        g_free(mainwin_title_text);
        mainwin_title_text = NULL;
        pthread_mutex_unlock(&title_mutex);
        playlistwin_update_list();
    }

    GDK_THREADS_LEAVE();

    return TRUE;
}

/*
 * GTK3 frame-tick callback — fires once per vsync frame, synchronized with the
 * compositor / GdkFrameClock.  This replaces the GTK2 pattern of:
 *   draw_widget_list() -> skin_draw_pixmap(GdkPixmap) -> gdk_window_clear()
 *                      -> gdk_flush()
 * which was a server-side XClearWindow + XFlush: instantaneous because the
 * backing store lived in the X server (GdkPixmap = XPixmap).
 *
 * In GTK3 the backing store is a CPU-side cairo_surface_t (IMAGE format).
 * gtk_widget_queue_draw() marks the GDK window dirty; the compositor blits
 * the surface on the next frame-clock tick.  Calling queue_draw() only when
 * widgets are dirty (the old idle_func behaviour) means repaints only happen
 * when widget state changes — once per second for the time counter.
 *
 * By moving the draw calls here and issuing queue_draw() unconditionally on
 * every vsync frame we guarantee the latest backing surface is always
 * presented, giving smooth frame-rate visual updates equivalent to GTK2.
 */
static gboolean mainwin_frame_tick(GtkWidget *widget, GdkFrameClock *frame_clock,
                                   gpointer user_data)
{
    (void)widget;
    (void)frame_clock;
    (void)user_data;

    draw_main_window(mainwin_force_redraw);
    mainwin_force_redraw = FALSE;
    draw_playlist_window(FALSE);
    draw_equalizer_window(FALSE);

    /* Unconditional queue_draw: ensures both backing surfaces are blitted to
     * screen this frame regardless of whether any widget was marked dirty.
     * GTK coalesces multiple queue_draw calls into a single repaint pass. */
    gtk_widget_queue_draw(mainwin);
    if (cfg.equalizer_visible)
        gtk_widget_queue_draw(equalizerwin);
    if (cfg.playlist_visible)
        gtk_widget_queue_draw(playlistwin);

    return G_SOURCE_CONTINUE;
}

static struct option long_options[] = {{"help", 0, NULL, 'h'},
                                       {"session", 1, NULL, 'n'},
                                       {"rew", 0, NULL, 'r'},
                                       {"play", 0, NULL, 'p'},
                                       {"pause", 0, NULL, 'u'},
                                       {"play-pause", 0, NULL, 't'},
                                       {"stop", 0, NULL, 's'},
                                       {"fwd", 0, NULL, 'f'},
                                       {"enqueue", 0, NULL, 'e'},
                                       {"queue", 0, NULL, 'Q'},
                                       {"toggle-shuffle", 2, NULL, 'S'},
                                       {"toggle-repeat", 2, NULL, 'R'},
                                       {"toggle-advance", 2, NULL, 'A'},
                                       {"show-main-window", 0, NULL, 'm'},
                                       {"version", 0, NULL, 'v'},
                                       {"quit", 0, NULL, 'q'},
                                       {"sm-client-id", 1, NULL, 'i'},
                                       {0, 0, 0, 0}};

void display_usage(void)
{
    fprintf(stderr, _("Usage: xmms [options] [files] ...\n\n"
                      "Options:\n"
                      "--------\n"));
    fprintf(stderr, "\n  -h, --help			");
    /* I18N: -h, --help switch */
    fprintf(stderr, _("Display this text and exit."));
    fprintf(stderr, "\n  -n, --session			");
    /* I18N: -n, --session switch */
    fprintf(stderr, _("Select XMMS session (Default: 0)"));
    fprintf(stderr, "\n  -r, --rew			");
    /* I18N: -r, --rew switch */
    fprintf(stderr, _("Skip backwards in playlist"));
    fprintf(stderr, "\n  -p, --play			");
    /* I18N: -p, --play switch */
    fprintf(stderr, _("Start playing current playlist"));
    fprintf(stderr, "\n  -u, --pause			");
    /* I18N: -u, --pause switch */
    fprintf(stderr, _("Pause current song"));
    fprintf(stderr, "\n  -s, --stop			");
    /* I18N: -s, --stop switch */
    fprintf(stderr, _("Stop current song"));
    fprintf(stderr, "\n  -t, --play-pause		");
    /* I18N: -t, --play-pause switch */
    fprintf(stderr, _("Pause if playing, play otherwise"));
    fprintf(stderr, "\n  -f, --fwd			");
    /* I18N: -f, --fwd switch */
    fprintf(stderr, _("Skip forward in playlist"));
    fprintf(stderr, "\n  -e, --enqueue			");
    /* I18N: -e, --enqueue switch */
    fprintf(stderr, _("Don't clear the playlist"));
    fprintf(stderr, "\n  -Q, --queue			");
    /* I18N: -Q, --queue switch */
    fprintf(stderr, _("Add file(s) to playlist and queue"));
    fprintf(stderr, "\n\n  -S, --toggle-shuffle%-10s", _("[=SWITCH]"));
    /* I18N: -S, --toggle-shuffle switch */
    fprintf(stderr, _("Toggle the 'shuffle' flag."));
    /* I18N: Only "SWITCH" may be translated */
    fprintf(stderr, "\n  -R, --toggle-repeat%-11s", _("[=SWITCH]"));
    /* I18N: -R, --toggle-repeat switch */
    fprintf(stderr, _("Toggle the 'repeat' flag."));
    /* I18N: Only "SWITCH" may be translated */
    fprintf(stderr, "\n  -A, --toggle-advance%-10s", _("[=SWITCH]"));
    /* I18N: -A, --toggle-advance switch */
    fprintf(stderr, _("Toggle the 'no playlist advance' flag."));
    fprintf(stderr, "\n\n  ");
    /* I18N: "on" and "off" is not translated. */
    fprintf(stderr, _("SWITCH may be either 'on' or 'off'\n"));
    fprintf(stderr, "\n  -m, --show-main-window	");
    /* I18N: -m, --show-main-window switch */
    fprintf(stderr, _("Show the main window."));
    fprintf(stderr, "\n  -i, --sm-client-id		");
    /* I18N: -i, --sm-client-id switch */
    fprintf(stderr, _("Previous session ID"));
    fprintf(stderr, "\n  -v, --version			");
    /* I18N: -v, --version switch */
    fprintf(stderr, _("Print version number and exit."));
    fprintf(stderr, "\n  -q, --quit			");
    /* I18N: -q, --quit switch */
    fprintf(stderr, _("Close remote session."));
    fprintf(stderr, "\n\n");
    fprintf(stderr, "Report bugs to http://bugs.xmms.org\n");

    exit(0);
}

struct cmdlineopt {
    GList *filenames;
    int session;
    gboolean play, stop, pause, fwd, rew, play_pause;
    gboolean toggle_shuffle, toggle_repeat, toggle_advance;
    gboolean enqueue, queue, mainwin, remote, quit;
    char *shuffle, *repeat, *advance;
    char *previous_session_id;
};

void parse_cmd_line(int argc, char **argv, struct cmdlineopt *opt)
{
    int c, i;
    char *filename;

    memset(opt, 0, sizeof(struct cmdlineopt));
    opt->session = -1;
    while ((c = getopt_long(argc, argv, "hn:rpusfeQS::R::A::mi:vtq", long_options, NULL)) != -1) {
        switch (c) {
        case 'h':
            display_usage();
            break;
        case 'n':
            opt->session = atoi(optarg);
            break;
        case 'r':
            opt->rew = TRUE;
            break;
        case 'p':
            opt->play = TRUE;
            break;
        case 'u':
            opt->pause = TRUE;
            break;
        case 's':
            opt->stop = TRUE;
            break;
        case 'f':
            opt->fwd = TRUE;
            break;
        case 't':
            opt->play_pause = TRUE;
            break;
        case 'S':
            opt->toggle_shuffle = TRUE;
            if (optarg)
                opt->shuffle = g_strdup(optarg);
            break;
        case 'R':
            opt->toggle_repeat = TRUE;
            if (optarg)
                opt->repeat = g_strdup(optarg);
            break;
            break;
        case 'A':
            opt->toggle_advance = TRUE;
            if (optarg)
                opt->advance = g_strdup(optarg);
            break;
            break;
        case 'm':
            opt->mainwin = TRUE;
            break;
        case 'e':
            opt->enqueue = TRUE;
            break;
        case 'Q':
            opt->queue = TRUE;
            break;
        case 'v':
            printf("%s %s\n", PACKAGE, VERSION);
            exit(0);
            break;
        case 'i':
            opt->previous_session_id = g_strdup(optarg);
            break;
        case 'q':
            opt->quit = TRUE;
            break;
        }
    }
    for (i = optind; i < argc; i++) {
        if (argv[i][0] == '/' || strstr(argv[i], "://"))
            filename = g_strdup(argv[i]);
        else {
            char *tmp = g_get_current_dir();
            filename = g_strconcat(tmp, "/", argv[i], NULL);
            g_free(tmp);
        }
        opt->filenames = g_list_prepend(opt->filenames, filename);
    }
    opt->filenames = g_list_reverse(opt->filenames);
}

void handle_cmd_line_options(struct cmdlineopt *opt, gboolean remote)
{
    int i;

    if (opt->session == -1) {
        if (!remote)
            opt->session = ctrlsocket_get_session_id();
        else
            opt->session = 0;
    }

    if (opt->filenames != NULL) {
        GList *node;
        int pos = 0;

        if ((opt->enqueue && opt->play) || (opt->queue && !opt->play))
            pos = xmms_remote_get_playlist_length(opt->session);
        if (!opt->enqueue && !opt->queue)
            xmms_remote_playlist_clear(opt->session);
        xmms_remote_playlist_add(opt->session, opt->filenames);
        node = opt->filenames;
        while (node) {
            g_free(node->data);
            node = g_list_next(node);
        }
        g_list_free(opt->filenames);
        if (opt->queue && !opt->play && xmms_remote_get_playlist_length(opt->session) > pos) {
            for (i = pos; i < xmms_remote_get_playlist_length(opt->session); i++)
                xmms_remote_playqueue_add(opt->session, i);
        }
        if (opt->enqueue && opt->play && xmms_remote_get_playlist_length(opt->session) > pos)
            xmms_remote_set_playlist_pos(opt->session, pos);
        if (!opt->enqueue && !opt->queue)
            xmms_remote_play(opt->session);
    }
    if (opt->rew)
        xmms_remote_playlist_prev(opt->session);
    if (opt->play)
        xmms_remote_play(opt->session);
    if (opt->pause)
        xmms_remote_pause(opt->session);
    if (opt->stop)
        xmms_remote_stop(opt->session);
    if (opt->fwd)
        xmms_remote_playlist_next(opt->session);
    if (opt->play_pause)
        xmms_remote_play_pause(opt->session);
    if (opt->toggle_shuffle) {
        if (opt->shuffle) {
            if (g_strcasecmp(opt->shuffle, "on") == 0) {
                if (!xmms_remote_is_shuffle(opt->session))
                    xmms_remote_toggle_shuffle(opt->session);
            } else if (g_strcasecmp(opt->shuffle, "off") == 0) {
                if (xmms_remote_is_shuffle(opt->session))
                    xmms_remote_toggle_shuffle(opt->session);
            } else
                /*
                 * I18N: "on" and "off" is not
                 * translated.
                 */
                fprintf(stderr,
                        _("Value '%s' not understood, "
                          "must be either 'on' or "
                          "'off'.\n"),
                        opt->shuffle);
        } else {
            xmms_remote_toggle_shuffle(opt->session);
        }
    }
    if (opt->toggle_repeat) {
        if (opt->repeat) {
            if (g_strcasecmp(opt->repeat, "on") == 0) {
                if (!xmms_remote_is_repeat(opt->session))
                    xmms_remote_toggle_repeat(opt->session);
            } else if (g_strcasecmp(opt->repeat, "off") == 0) {
                if (xmms_remote_is_repeat(opt->session))
                    xmms_remote_toggle_repeat(opt->session);
            } else
                fprintf(stderr,
                        _("Value '%s' not understood, "
                          "must be either 'on' or "
                          "'off'.\n"),
                        opt->repeat);
        } else {
            xmms_remote_toggle_repeat(opt->session);
        }
    }
    if (opt->toggle_advance) {
        if (opt->advance) {
            if (g_strcasecmp(opt->advance, "on") == 0) {
                if (!xmms_remote_is_advance(opt->session))
                    xmms_remote_toggle_advance(opt->session);
            } else if (g_strcasecmp(opt->advance, "off") == 0) {
                if (xmms_remote_is_advance(opt->session))
                    xmms_remote_toggle_advance(opt->session);
            } else
                fprintf(stderr, _("Value '%s' not understood, must be either 'on' or 'off'.\n"),
                        opt->advance);
        } else {
            xmms_remote_toggle_advance(opt->session);
        }
    }
    if (opt->mainwin)
        xmms_remote_main_win_toggle(opt->session, TRUE);
    if (opt->quit)
        xmms_remote_quit(opt->session);
}

void segfault_handler(int sig)
{
    /* TODO(#signal-safety): replace with async-signal-safe write() when refactoring signal handlers
     */
    /* NOLINTBEGIN(bugprone-signal-handler,cert-msc54-cpp,cert-sig30-c) */
    printf(_("\nSegmentation fault\n\n"
             "You've probably found a bug in XMMS, please visit\n"
             "http://bugs.xmms.org and fill out a bug report.\n\n"));
    exit(1);
    /* NOLINTEND(bugprone-signal-handler,cert-msc54-cpp,cert-sig30-c) */
}

static gboolean pposition_configure(GtkWidget *w, GdkEventConfigure *event, gpointer data)
{
    gint x, y;
    gdk_window_get_root_origin(gtk_widget_get_window(w), &x, &y);
    if (x != 0 || y != 0)
        pposition_broken = TRUE;
    gtk_widget_destroy(w);

    return FALSE;
}

void check_pposition(void)
{
    GtkWidget *window;
    cairo_region_t *region;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_wmclass(GTK_WINDOW(window), "XMMS_Player", "xmms");
    g_signal_connect(G_OBJECT(window), "configure_event", G_CALLBACK(pposition_configure), NULL);
    gtk_window_move(GTK_WINDOW(window), 0, 0) /* TODO(#gtk3): was set_uposition */;
    gtk_widget_realize(window);

    gtk_widget_set_size_request(window, 1, 1);
    gdk_window_set_decorations(gtk_widget_get_window(window), 0);

    region = mainwin_create_opaque_region(1, 1, NULL);
    gtk_widget_shape_combine_region(window, region);
    cairo_region_destroy(region);

    gtk_widget_show(window);

    while (g_main_iteration(FALSE))
        ;
}

#if 0

static GdkFilterReturn save_yourself_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	Atom save_yourself, protocols;

	save_yourself = gdk_atom_intern("WM_SAVE_YOURSELF", FALSE);
	protocols = gdk_atom_intern("WM_PROTOCOLS", FALSE);

	if (((XEvent*)xevent)->type == ClientMessage)
	{
		XClientMessageEvent *cme = (XClientMessageEvent*) xevent;
		if (cme->message_type == protocols &&
		    (Atom) cme->data.l[0] == save_yourself)
		{
			save_config();
			XSetCommand(gdk_x11_get_default_xdisplay(),
				    gdk_x11_window_get_xid(gtk_widget_get_window(mainwin)),
				    restart_argv, restart_argc);
			return GDK_FILTER_REMOVE;
		}
	}

	return GDK_FILTER_CONTINUE;
}
#endif

static void enable_x11r5_session_management(int argc, char **argv)
{
    /*
     * X11R5 Session Management
     *
     * Most of xmms' options does not make sense when we are
     * restarted, so we drop them all.
     */

    /* GdkAtom save_yourself; */
    /* Atom *temp, *temp2; */
    /* int i, n; */

    restart_argc = 1;
    restart_argv = g_malloc(sizeof(char *));
    restart_argv[0] = g_strdup(argv[0]);

    XSetCommand(gdk_x11_get_default_xdisplay(),
                gdk_x11_window_get_xid(gtk_widget_get_window(mainwin)), restart_argv, restart_argc);
#if 0
	save_yourself = gdk_atom_intern("WM_SAVE_YOURSELF", FALSE);

	/*
	 * It would be easier if we could call gdk_add_client_message_filter()
	 * here but GDK only allows one filter per message and wants to
	 * filter "WM_PROTOCOLS" itself.
	 */
	gdk_window_add_filter(gtk_widget_get_window(mainwin), save_yourself_filter, NULL);
	if (XGetWMProtocols(gdk_x11_get_default_xdisplay(), gdk_x11_window_get_xid(gtk_widget_get_window(mainwin)), &temp, &n))
	{
		for (i = 0; i < n; i++)
			if (temp[i] == save_yourself)
				return;
		temp2 = g_new(Atom, n + 1);
		for (i = 0; i < n; i++)
			temp2[i] = temp[i];
		temp2[i] = save_yourself;
		XSetWMProtocols(gdk_x11_get_default_xdisplay(), gdk_x11_window_get_xid(gtk_widget_get_window(mainwin)), temp2, n + 1);
		if (n > 0)
			XFree(temp);
		g_free(temp2);
	}
	else
		XSetWMProtocols(gdk_x11_get_default_xdisplay(), gdk_x11_window_get_xid(gtk_widget_get_window(mainwin)), &save_yourself, 1);
#endif
}

int main(int argc, char **argv)
{
    char *filename;
    const char *sm_client_id;
    struct cmdlineopt options;
#if defined(HAVE_SCHED_SETSCHEDULER) && defined(HAVE_SCHED_GET_PRIORITY_MAX)
    struct sched_param sparam;
#endif

#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    signal(SIGPIPE, SIG_IGN); /* for controlsocket.c */
    signal(SIGSEGV, segfault_handler);
    if (!g_thread_supported())
        g_error(_("GLib does not support threads."));

    parse_cmd_line(argc, argv, &options);

#ifdef HAVE_SRANDOMDEV
    srandomdev();
#else
    {
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd != -1) {
            char randtable[RANDTABLE_SIZE];
            read(fd, randtable, RANDTABLE_SIZE);
            initstate(time(NULL), randtable, RANDTABLE_SIZE);
            close(fd);
        } else
            srandom(time(NULL));
    }
#endif

    read_config();

#if defined(HAVE_SCHED_SETSCHEDULER) && defined(HAVE_SCHED_GET_PRIORITY_MAX)
    if (cfg.use_realtime) {
        sparam.sched_priority = sched_get_priority_max(SCHED_RR);
        sched_setscheduler(0, SCHED_RR, &sparam);
    }
#endif
    if (geteuid() == 0)
        setuid(getuid());

    if (!gtk_init_check(&argc, &argv)) {
        if (argc < 2)
            g_log(NULL, G_LOG_LEVEL_CRITICAL, "Unable to open display");
        handle_cmd_line_options(&options, TRUE);
        exit(0);
    }

    make_xmms_dir();
    if (options.session != -1 || !setup_ctrlsocket()) {
        handle_cmd_line_options(&options, TRUE);
        exit(0);
    }

    if (options.quit)
        return 0;

    if (gtk_major_version == 1 &&
        (gtk_minor_version < 2 || (gtk_minor_version == 2 && gtk_micro_version < 2))) {
        printf(_("Sorry, your GTK+ version (%d.%d.%d) doesn't work with XMMS.\n"
                 "Please use GTK+ %s or newer.\n"),
               gtk_major_version, gtk_minor_version, gtk_micro_version, "1.2.2");
        exit(0);
    }

    check_wm_hints();
    check_pposition();
    mainwin_accel = gtk_accel_group_new();

    sm_client_id = sm_init(argc, argv, options.previous_session_id);
    (void)sm_client_id;
    mainwin_create();

    /* Load per-user CSS override (may not exist; silently ignored). */
    filename = g_strconcat(g_get_home_dir(), "/.xmms/gtkrc", NULL);
    {
        GtkCssProvider *user_css = gtk_css_provider_new();
        gtk_css_provider_load_from_path(user_css, filename, NULL);
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                  GTK_STYLE_PROVIDER(user_css),
                                                  GTK_STYLE_PROVIDER_PRIORITY_USER);
        g_object_unref(user_css);
    }
    g_free(filename);

    /* Built-in menu CSS: override system theme so menus render as compact
     * charcoal popups matching the XMMS skin, not a large solid-black box.
     *
     * Without this, the menu GdkWindow inherits the application's opaque
     * dark background and paints a full window-sized black rectangle behind
     * the items.  Setting an explicit background-color on menu and menuitem
     * scopes the background to only the drawn widgets. */
    {
        static const gchar *menu_css = "menu, menu decoration {\n"
                                       "  background-color: #2a2a2e;\n"
                                       "  border: 1px solid #555566;\n"
                                       "  padding: 1px;\n"
                                       "  margin: 0px;\n"
                                       "}\n"
                                       "menuitem {\n"
                                       "  background-color: transparent;\n"
                                       "  color: #c8c8d8;\n"
                                       "  padding: 1px 6px;\n"
                                       "  min-height: 0px;\n"
                                       "}\n"
                                       "menuitem:hover, menuitem:selected {\n"
                                       "  background-color: #0a3a6e;\n"
                                       "  color: #ffffff;\n"
                                       "}\n"
                                       "menuitem:disabled, menuitem:insensitive {\n"
                                       "  color: #606070;\n"
                                       "}\n"
                                       "separator, menuitem > separator {\n"
                                       "  background-color: #444455;\n"
                                       "  min-height: 1px;\n"
                                       "  margin: 2px 0px;\n"
                                       "}\n";
        GtkCssProvider *menu_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(menu_provider, menu_css, -1, NULL);
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                  GTK_STYLE_PROVIDER(menu_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(menu_provider);
    }

    /* Plugins might start threads that can call gtk */
    GDK_THREADS_ENTER();

    init_plugins();

    playlistwin_create();
    equalizerwin_create();
    init_skins();
    load_skin(cfg.skin);
    create_popups();
    create_prefs_window();
    util_read_menu_rc();
    read_volume(VOLSET_STARTUP);

    filename = g_strconcat(g_get_home_dir(), "/.xmms/xmms.m3u", NULL);
    playlist_load(filename);
    g_free(filename);
    playlist_set_position(cfg.playlist_position);
    GDK_THREADS_LEAVE();
    start_ctrlsocket();
    handle_cmd_line_options(&options, FALSE);
    GDK_THREADS_ENTER();
    mainwin_set_info_text();

    gtk_widget_show(mainwin);
    if (pposition_broken && cfg.player_x != -1 && cfg.save_window_position)
        dock_set_uposition(mainwin, cfg.player_x, cfg.player_y);

    if (!cfg.player_visible && (cfg.playlist_visible || cfg.equalizer_visible))
        mainwin_real_hide();
    else
        mainwin_show(TRUE);
    if (cfg.playlist_visible)
        playlistwin_show(TRUE);
    if (cfg.equalizer_visible)
        equalizerwin_show(TRUE);

    draw_main_window(TRUE);

    mainwin_timeout_tag = g_timeout_add(10, idle_func, NULL);
    /* Register the per-frame drawing tick, synchronized with the compositor
     * frame clock.  Logic (time tracking, input, volume) stays in idle_func
     * at 100 Hz; visual presentation happens here at vsync rate (~60 Hz). */
    mainwin_tick_id =
        gtk_widget_add_tick_callback(mainwin, (GtkTickCallback)mainwin_frame_tick, NULL, NULL);
    playlist_start_get_info_thread();

    mpris2_init(); /* Refs #37: register MPRIS2 D-Bus service */
    enable_x11r5_session_management(argc, argv);
    gtk_main();
    GDK_THREADS_LEAVE();

    return 0;
}

#if 0
/* xmms.desktop comment */
char *comment = _("X Multimedia System");
#endif
