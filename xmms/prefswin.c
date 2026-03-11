/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front
 * Technologies
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
 * GTK3 migration of the preferences dialog.
 * Original GTK2 source preserved in prefswin.c.gtk2-bak.
 *
 * Key API changes from GTK2:
 *   GtkCList           -> GtkTreeView + GtkListStore
 *   GtkOptionMenu      -> GtkComboBoxText
 *   GtkTooltips        -> gtk_widget_set_tooltip_text()
 *   GtkFontSelection   -> GtkFontChooserDialog
 *   GtkTable           -> GtkGrid
 *   gtk_signal_connect -> g_signal_connect
 *   GTK_WIDGET_VISIBLE -> gtk_widget_get_visible
 *   GDK_THREADS_*      -> removed
 */

#include "prefswin.h"

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "libxmms/titlestring.h"
#include "libxmms/util.h"
#include "xmms.h"
#include "effect.h"
#include "equalizer.h"
#include "general.h"
#include "hints.h"
#include "input.h"
#include "main.h"
#include "menurow.h"
#include "output.h"
#include "pbutton.h"
#include "playlist.h"
#include "playlist_list.h"
#include "playlistwin.h"
#include "textbox.h"
#include "visualization.h"

/* ---- module-external variables used by apply_changes ---- */

extern MenuRow *mainwin_menurow;
extern PButton *playlistwin_shade, *playlistwin_close, *equalizerwin_close;
extern PButton *mainwin_menubtn, *mainwin_minimize, *mainwin_shade, *mainwin_close;
extern TextBox *mainwin_info;
extern gboolean mainwin_focus, equalizerwin_focus, playlistwin_focus;

/* ---- module state ---- */

static GtkWidget *prefswin = NULL;
static GtkWidget *prefswin_notebook = NULL;
static gint prefswin_vis_page_idx = 3;

/* Input plugins */
static GtkWidget *prefswin_audio_ilist_view = NULL;
static GtkWidget *prefswin_audio_ie_cbox = NULL;
static GtkWidget *prefswin_audio_iconfig = NULL;
static GtkWidget *prefswin_audio_iabout = NULL;

/* Output plugin */
static GtkWidget *prefswin_audio_ocombo = NULL;
static GtkWidget *prefswin_audio_oconfig = NULL;
static GtkWidget *prefswin_audio_oabout = NULL;

/* Effect plugins */
static GtkWidget *prefswin_eplugins_list = NULL;
static GtkWidget *prefswin_eplugins_config = NULL;
static GtkWidget *prefswin_eplugins_about = NULL;
static GtkWidget *prefswin_eplugins_use_cbox = NULL;

/* General plugins */
static GtkWidget *prefswin_gplugins_list = NULL;
static GtkWidget *prefswin_gplugins_config = NULL;
static GtkWidget *prefswin_gplugins_about = NULL;
static GtkWidget *prefswin_gplugins_use_cbox = NULL;

/* Visualization plugins */
static GtkWidget *prefswin_vplugins_list = NULL;
static GtkWidget *prefswin_vplugins_config = NULL;
static GtkWidget *prefswin_vplugins_about = NULL;
static GtkWidget *prefswin_vplugins_use_cbox = NULL;

/* Options page */
static GtkWidget *prefswin_options_sd_entry = NULL;
static GtkWidget *prefswin_options_pbs_entry = NULL;
static GtkWidget *prefswin_options_mouse_spin = NULL;

/* Fonts page */
static GtkWidget *prefswin_options_font_entry = NULL;
static GtkWidget *prefswin_mainwin_font_entry = NULL;

/* Title page */
static GtkWidget *prefswin_title_entry = NULL;

/* OK button — grab-default target */
static GtkWidget *prefswin_ok = NULL;

static gboolean updating_ilist = FALSE;
static gboolean updating_glist = FALSE;
static gboolean updating_vlist = FALSE;
static gboolean updating_elist = FALSE;
static gboolean is_opening = FALSE;
static gint selected_oplugin = 0;

/* List of struct option_info* tracking all checkboxes/radios */
static GList *option_list = NULL;

/* ---- utility helpers ---- */

static gint prefswin_get_selected_row(GtkTreeView *view)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;
    gint idx = -1;

    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        path = gtk_tree_model_get_path(model, &iter);
        idx = gtk_tree_path_get_indices(path)[0];
        gtk_tree_path_free(path);
    }
    return idx;
}

/* Build "Description   [basename]" display string; caller must g_free(). */
static gchar *gen_module_description(const gchar *filename, const gchar *desc)
{
    return g_strdup_printf("%s   [%s]", desc, g_basename(filename));
}

/* Create scrolled window + empty single-column GtkTreeView.
 * *view_out receives the GtkTreeView. */
static GtkWidget *make_plugin_listview(GtkWidget **view_out)
{
    GtkListStore *store;
    GtkWidget *view, *sw;

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_widget_set_size_request(sw, -1, 80);

    store = gtk_list_store_new(1, G_TYPE_STRING);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
                                GTK_SELECTION_SINGLE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view),
        gtk_tree_view_column_new_with_attributes(
            "", gtk_cell_renderer_text_new(), "text", 0, NULL));
    gtk_container_add(GTK_CONTAINER(sw), view);
    gtk_widget_show(view);
    gtk_widget_show(sw);

    if (view_out)
        *view_out = view;
    return sw;
}

/* ---- checkbox / radio option tracking ---- */

static GtkWidget *prefswin_option_new(gboolean *cfg_field)
{
    struct option_info *info = g_malloc(sizeof(struct option_info));
    info->button = gtk_check_button_new();
    info->cfg = cfg_field;
    option_list = g_list_prepend(option_list, info);
    return info->button;
}

static GtkWidget *prefswin_option_new_with_label(gboolean *cfg_field, const gchar *label)
{
    GtkWidget *btn = prefswin_option_new(cfg_field);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_container_add(GTK_CONTAINER(btn), lbl);
    return btn;
}

static GtkWidget *prefswin_radio_new(gboolean *cfg_field, GtkRadioButton *group)
{
    GtkWidget *btn = gtk_radio_button_new_from_widget(group);
    if (cfg_field != NULL) {
        struct option_info *info = g_malloc(sizeof(struct option_info));
        info->button = btn;
        info->cfg = cfg_field;
        option_list = g_list_prepend(option_list, info);
    }
    return btn;
}

static GtkWidget *prefswin_radio_new_with_label(gboolean *cfg_field, GtkRadioButton *group,
                                                const gchar *label)
{
    GtkWidget *btn = prefswin_radio_new(cfg_field, group);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_container_add(GTK_CONTAINER(btn), lbl);
    return btn;
}

static void prefswin_options_read_data(void)
{
    GList *node;
    for (node = option_list; node; node = node->next) {
        struct option_info *info = node->data;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(info->button), *(info->cfg));
    }
}

static void prefswin_options_write_data(void)
{
    GList *node;
    for (node = option_list; node; node = node->next) {
        struct option_info *info = node->data;
        *(info->cfg) = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(info->button));
    }
}

/* ---- populate plugin lists ---- */

static void prefswin_plugins_set_buttons_insensitive(GtkWidget *config,
                                                     GtkWidget *about,
                                                     GtkWidget *use_cbox)
{
    if (config)
        gtk_widget_set_sensitive(config, FALSE);
    if (about)
        gtk_widget_set_sensitive(about, FALSE);
    if (use_cbox)
        gtk_widget_set_sensitive(use_cbox, FALSE);
}

static void add_input_plugins(GtkTreeView *view)
{
    GList *ilist = get_input_list();
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(view));

    gtk_list_store_clear(store);
    while (ilist) {
        InputPlugin *ip = (InputPlugin *)ilist->data;
        gchar *desc = gen_module_description(ip->filename, ip->description);
        if (g_list_find(disabled_iplugins, ip)) {
            gchar *tmp = g_strconcat(desc, _(" (disabled)"), NULL);
            g_free(desc);
            desc = tmp;
        }
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, desc, -1);
        g_free(desc);
        ilist = ilist->next;
    }
    prefswin_plugins_set_buttons_insensitive(prefswin_audio_iconfig,
                                             prefswin_audio_iabout,
                                             prefswin_audio_ie_cbox);
}

static void add_effect_plugins(GtkTreeView *view)
{
    GList *glist = get_effect_list();
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    gint i = 0;

    gtk_list_store_clear(store);
    while (glist) {
        EffectPlugin *ep = (EffectPlugin *)glist->data;
        gchar *desc = gen_module_description(ep->filename, ep->description);
        if (effect_enabled(i)) {
            gchar *tmp = g_strconcat(desc, _(" (enabled)"), NULL);
            g_free(desc);
            desc = tmp;
        }
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, desc, -1);
        g_free(desc);
        glist = glist->next;
        i++;
    }
    prefswin_plugins_set_buttons_insensitive(prefswin_eplugins_config,
                                             prefswin_eplugins_about,
                                             prefswin_eplugins_use_cbox);
}

static void add_general_plugins(GtkTreeView *view)
{
    GList *glist = get_general_list();
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    gint i = 0;

    gtk_list_store_clear(store);
    while (glist) {
        GeneralPlugin *gp = (GeneralPlugin *)glist->data;
        gchar *desc = gen_module_description(gp->filename, gp->description);
        if (general_enabled(i)) {
            gchar *tmp = g_strconcat(desc, _(" (enabled)"), NULL);
            g_free(desc);
            desc = tmp;
        }
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, desc, -1);
        g_free(desc);
        glist = glist->next;
        i++;
    }
    prefswin_plugins_set_buttons_insensitive(prefswin_gplugins_config,
                                             prefswin_gplugins_about,
                                             prefswin_gplugins_use_cbox);
}

static void add_vis_plugins(GtkTreeView *view)
{
    GList *glist = get_vis_list();
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    gint i = 0;

    gtk_list_store_clear(store);
    while (glist) {
        VisPlugin *vp = (VisPlugin *)glist->data;
        gchar *desc = gen_module_description(vp->filename, vp->description);
        if (vis_enabled(i)) {
            gchar *tmp = g_strconcat(desc, _(" (enabled)"), NULL);
            g_free(desc);
            desc = tmp;
        }
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, desc, -1);
        g_free(desc);
        glist = glist->next;
        i++;
    }
    prefswin_plugins_set_buttons_insensitive(prefswin_vplugins_config,
                                             prefswin_vplugins_about,
                                             prefswin_vplugins_use_cbox);
}

static void add_output_plugins(GtkComboBoxText *combo)
{
    GList *olist = get_output_list();
    OutputPlugin *cp = get_current_output_plugin();
    gint i = 0;

    gtk_combo_box_text_remove_all(combo);

    if (!olist) {
        gtk_widget_set_sensitive(GTK_WIDGET(combo), FALSE);
        gtk_widget_set_sensitive(prefswin_audio_oconfig, FALSE);
        gtk_widget_set_sensitive(prefswin_audio_oabout, FALSE);
        return;
    }

    while (olist) {
        OutputPlugin *op = (OutputPlugin *)olist->data;
        if (olist->data == cp)
            selected_oplugin = i;
        gchar *desc = gen_module_description(op->filename, op->description);
        gtk_combo_box_text_append_text(combo, desc);
        g_free(desc);
        olist = olist->next;
        i++;
    }

    g_signal_handlers_block_matched(G_OBJECT(combo), G_SIGNAL_MATCH_FUNC,
                                    0, 0, NULL, NULL, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), selected_oplugin);
    g_signal_handlers_unblock_matched(G_OBJECT(combo), G_SIGNAL_MATCH_FUNC,
                                      0, 0, NULL, NULL, NULL);

    if (cp) {
        gtk_widget_set_sensitive(prefswin_audio_oconfig, cp->configure != NULL);
        gtk_widget_set_sensitive(prefswin_audio_oabout, cp->about != NULL);
    }
}

/* ---- output-plugin combo callback ---- */

static void prefswin_output_changed_cb(GtkComboBox *combo, gpointer data)
{
    gint idx = gtk_combo_box_get_active(combo);
    GList *output;
    OutputPlugin *cp;

    if (idx < 0)
        return;
    selected_oplugin = idx;
    output = get_output_list();
    cp = (OutputPlugin *)g_list_nth(output, idx)->data;
    gtk_widget_set_sensitive(prefswin_audio_oconfig, cp->configure != NULL);
    gtk_widget_set_sensitive(prefswin_audio_oabout, cp->about != NULL);
}

/* ---- Plugin selection-changed callbacks ---- */

static void prefswin_ilist_changed(GtkTreeSelection *sel, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(
        gtk_tree_selection_get_tree_view(sel));
    gint idx = prefswin_get_selected_row(view);
    GList *iplist;
    InputPlugin *ip;

    if (idx < 0) {
        gtk_widget_set_sensitive(prefswin_audio_iconfig, FALSE);
        gtk_widget_set_sensitive(prefswin_audio_iabout, FALSE);
        gtk_widget_set_sensitive(prefswin_audio_ie_cbox, FALSE);
        return;
    }

    iplist = get_input_list();
    ip = (InputPlugin *)g_list_nth(iplist, idx)->data;

    gtk_widget_set_sensitive(prefswin_audio_ie_cbox, TRUE);
    updating_ilist = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefswin_audio_ie_cbox),
                                 g_list_find(disabled_iplugins, ip) == NULL);
    updating_ilist = FALSE;

    gtk_widget_set_sensitive(prefswin_audio_iconfig, ip->configure != NULL);
    gtk_widget_set_sensitive(prefswin_audio_iabout, ip->about != NULL);
}

static void prefswin_elist_changed(GtkTreeSelection *sel, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(gtk_tree_selection_get_tree_view(sel));
    gint idx = prefswin_get_selected_row(view);
    GList *eplist;
    EffectPlugin *ep;

    if (idx < 0) {
        gtk_widget_set_sensitive(prefswin_eplugins_config, FALSE);
        gtk_widget_set_sensitive(prefswin_eplugins_about, FALSE);
        gtk_widget_set_sensitive(prefswin_eplugins_use_cbox, FALSE);
        return;
    }

    eplist = get_effect_list();
    ep = (EffectPlugin *)g_list_nth(eplist, idx)->data;

    gtk_widget_set_sensitive(prefswin_eplugins_use_cbox, TRUE);
    updating_elist = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefswin_eplugins_use_cbox),
                                 effect_enabled(idx));
    updating_elist = FALSE;

    gtk_widget_set_sensitive(prefswin_eplugins_config, ep->configure != NULL);
    gtk_widget_set_sensitive(prefswin_eplugins_about, ep->about != NULL);
}

static void prefswin_glist_changed(GtkTreeSelection *sel, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(gtk_tree_selection_get_tree_view(sel));
    gint idx = prefswin_get_selected_row(view);
    GList *gplist;
    GeneralPlugin *gp;

    if (idx < 0) {
        gtk_widget_set_sensitive(prefswin_gplugins_config, FALSE);
        gtk_widget_set_sensitive(prefswin_gplugins_about, FALSE);
        gtk_widget_set_sensitive(prefswin_gplugins_use_cbox, FALSE);
        return;
    }

    gplist = get_general_list();
    gp = (GeneralPlugin *)g_list_nth(gplist, idx)->data;

    gtk_widget_set_sensitive(prefswin_gplugins_use_cbox, TRUE);
    updating_glist = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefswin_gplugins_use_cbox),
                                 general_enabled(idx));
    updating_glist = FALSE;

    gtk_widget_set_sensitive(prefswin_gplugins_config, gp && gp->configure != NULL);
    gtk_widget_set_sensitive(prefswin_gplugins_about, gp && gp->about != NULL);
}

static void prefswin_vlist_changed(GtkTreeSelection *sel, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(gtk_tree_selection_get_tree_view(sel));
    gint idx = prefswin_get_selected_row(view);
    GList *vplist;
    VisPlugin *vp;

    if (idx < 0) {
        gtk_widget_set_sensitive(prefswin_vplugins_config, FALSE);
        gtk_widget_set_sensitive(prefswin_vplugins_about, FALSE);
        gtk_widget_set_sensitive(prefswin_vplugins_use_cbox, FALSE);
        return;
    }

    vplist = get_vis_list();
    vp = (VisPlugin *)g_list_nth(vplist, idx)->data;

    gtk_widget_set_sensitive(prefswin_vplugins_use_cbox, TRUE);
    updating_vlist = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefswin_vplugins_use_cbox),
                                 vis_enabled(idx));
    updating_vlist = FALSE;

    gtk_widget_set_sensitive(prefswin_vplugins_config, vp && vp->configure != NULL);
    gtk_widget_set_sensitive(prefswin_vplugins_about, vp && vp->about != NULL);
}

/* ---- Configure / About / Enable toggle callbacks ---- */

static void prefswin_iconfigure(GtkButton *w, gpointer data)
{
    gint idx = prefswin_get_selected_row(GTK_TREE_VIEW(data));
    if (idx >= 0)
        input_configure(idx);
}

static void prefswin_iabout(GtkButton *w, gpointer data)
{
    gint idx = prefswin_get_selected_row(GTK_TREE_VIEW(data));
    if (idx >= 0)
        input_about(idx);
}

static void prefswin_ip_toggled(GtkToggleButton *w, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(data);
    gint sel;
    InputPlugin *ip;
    GtkAdjustment *adj;
    gdouble pos;
    GtkTreePath *path;

    if (updating_ilist)
        return;
    sel = prefswin_get_selected_row(view);
    if (sel < 0)
        return;

    ip = (InputPlugin *)g_list_nth(get_input_list(), sel)->data;
    if (!gtk_toggle_button_get_active(w))
        disabled_iplugins = g_list_append(disabled_iplugins, ip);
    else if (g_list_find(disabled_iplugins, ip))
        disabled_iplugins = g_list_remove(disabled_iplugins, ip);

    adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
    pos = gtk_adjustment_get_value(adj);
    add_input_plugins(view);
    gtk_adjustment_set_value(adj, pos);
    path = gtk_tree_path_new_from_indices(sel, -1);
    gtk_tree_selection_select_path(gtk_tree_view_get_selection(view), path);
    gtk_tree_path_free(path);
}

static void prefswin_oconfigure(GtkWidget *w, gpointer data)
{
    output_configure(selected_oplugin);
}

static void prefswin_oabout(GtkWidget *w, gpointer data)
{
    output_about(selected_oplugin);
}

static void prefswin_econfigure(GtkButton *w, gpointer data)
{
    gint idx = prefswin_get_selected_row(GTK_TREE_VIEW(data));
    if (idx >= 0)
        effect_configure(idx);
}

static void prefswin_eabout(GtkButton *w, gpointer data)
{
    gint idx = prefswin_get_selected_row(GTK_TREE_VIEW(data));
    if (idx >= 0)
        effect_about(idx);
}

static void prefswin_eplugins_use_cb(GtkToggleButton *w, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(data);
    gint sel;
    GtkAdjustment *adj;
    gdouble pos;
    GtkTreePath *path;

    if (updating_elist)
        return;
    sel = prefswin_get_selected_row(view);
    if (sel < 0)
        return;
    enable_effect_plugin(sel, gtk_toggle_button_get_active(w));
    adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
    pos = gtk_adjustment_get_value(adj);
    add_effect_plugins(view);
    gtk_adjustment_set_value(adj, pos);
    path = gtk_tree_path_new_from_indices(sel, -1);
    gtk_tree_selection_select_path(gtk_tree_view_get_selection(view), path);
    gtk_tree_path_free(path);
}

static void prefswin_gconfigure(GtkButton *w, gpointer data)
{
    gint idx = prefswin_get_selected_row(GTK_TREE_VIEW(data));
    if (idx >= 0)
        general_configure(idx);
}

static void prefswin_gabout(GtkButton *w, gpointer data)
{
    gint idx = prefswin_get_selected_row(GTK_TREE_VIEW(data));
    if (idx >= 0)
        general_about(idx);
}

static void prefswin_gplugins_use_cb(GtkToggleButton *w, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(data);
    gint sel;
    GtkAdjustment *adj;
    gdouble pos;
    GtkTreePath *path;

    if (updating_glist)
        return;
    sel = prefswin_get_selected_row(view);
    if (sel < 0)
        return;
    enable_general_plugin(sel, gtk_toggle_button_get_active(w));
    adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
    pos = gtk_adjustment_get_value(adj);
    add_general_plugins(view);
    gtk_adjustment_set_value(adj, pos);
    path = gtk_tree_path_new_from_indices(sel, -1);
    gtk_tree_selection_select_path(gtk_tree_view_get_selection(view), path);
    gtk_tree_path_free(path);
}

static void prefswin_vconfigure(GtkButton *w, gpointer data)
{
    gint idx = prefswin_get_selected_row(GTK_TREE_VIEW(data));
    if (idx >= 0)
        vis_configure(idx);
}

static void prefswin_vabout(GtkButton *w, gpointer data)
{
    gint idx = prefswin_get_selected_row(GTK_TREE_VIEW(data));
    if (idx >= 0)
        vis_about(idx);
}

static void prefswin_vplugins_use_cb(GtkToggleButton *w, gpointer data)
{
    GtkTreeView *view = GTK_TREE_VIEW(data);
    gint sel;
    GtkAdjustment *adj;
    gdouble pos;
    GtkTreePath *path;

    if (updating_vlist)
        return;
    sel = prefswin_get_selected_row(view);
    if (sel < 0)
        return;
    enable_vis_plugin(sel, gtk_toggle_button_get_active(w));
    adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
    pos = gtk_adjustment_get_value(adj);
    add_vis_plugins(view);
    gtk_adjustment_set_value(adj, pos);
    path = gtk_tree_path_new_from_indices(sel, -1);
    gtk_tree_selection_select_path(gtk_tree_view_get_selection(view), path);
    gtk_tree_path_free(path);
}

/* ---- Options page callbacks ---- */

static void prefswin_rt_callback(GtkToggleButton *w, gpointer data)
{
    if (!gtk_toggle_button_get_active(w) || is_opening)
        return;

    xmms_show_message(_("Warning"),
                      _("Realtime priority is a way for XMMS to get a higher\n"
                        "priority  for CPU time.  This might give less \"skips\".\n\n"
                        "This requires that XMMS is run with root privileges and\n"
                        "may, although it's very unusual, lock up your computer.\n"
                        "Running XMMS with root privileges might also have other\n"
                        "security implications.\n\n"
                        "Using this feature is not recommended.\n"
                        "To activate this you need to restart XMMS."),
                      _("OK"), FALSE, NULL, NULL);
}

/* ---- Font browser callbacks ---- */

static void prefswin_font_browse_cb(GtkWidget *w, gpointer data)
{
    GtkWidget *dlg = gtk_font_chooser_dialog_new(_("Select playlist font:"),
                                                  GTK_WINDOW(prefswin));
    if (prefswin_options_font_entry) {
        const gchar *current = gtk_entry_get_text(GTK_ENTRY(prefswin_options_font_entry));
        if (current && *current)
            gtk_font_chooser_set_font(GTK_FONT_CHOOSER(dlg), current);
    }
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        gchar *fontname = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(dlg));
        if (fontname)
            gtk_entry_set_text(GTK_ENTRY(prefswin_options_font_entry), fontname);
        g_free(fontname);
    }
    gtk_widget_destroy(dlg);
}

static void prefswin_mainwin_font_browse_cb(GtkWidget *w, gpointer data)
{
    GtkWidget *dlg = gtk_font_chooser_dialog_new(_("Select main window font:"),
                                                  GTK_WINDOW(prefswin));
    if (prefswin_mainwin_font_entry) {
        const gchar *current = gtk_entry_get_text(GTK_ENTRY(prefswin_mainwin_font_entry));
        if (current && *current)
            gtk_font_chooser_set_font(GTK_FONT_CHOOSER(dlg), current);
    }
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        gchar *fontname = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(dlg));
        if (fontname)
            gtk_entry_set_text(GTK_ENTRY(prefswin_mainwin_font_entry), fontname);
        g_free(fontname);
    }
    gtk_widget_destroy(dlg);
}

/* ---- Apply / OK / Cancel ---- */

static void prefswin_toggle_wm_decorations(void)
{
    gboolean hide_player = !cfg.player_visible;
    if (hide_player)
        mainwin_real_show();
    gtk_widget_hide(mainwin);
    if (cfg.playlist_visible)
        gtk_widget_hide(playlistwin);
    if (cfg.equalizer_visible)
        gtk_widget_hide(equalizerwin);
    mainwin_recreate();
    equalizerwin_recreate();
    playlistwin_recreate();
    gtk_widget_show(mainwin);
    if (hide_player)
        mainwin_real_hide();
    if (cfg.playlist_visible)
        gtk_widget_show(playlistwin);
    if (cfg.equalizer_visible)
        gtk_widget_show(equalizerwin);
    hint_set_always(cfg.always_on_top);
    hint_set_sticky(cfg.sticky);
    hint_set_skip_winlist(equalizerwin);
    hint_set_skip_winlist(playlistwin);
    gtk_window_set_transient_for(GTK_WINDOW(prefswin), GTK_WINDOW(mainwin));
}

static void prefswin_apply_changes(void)
{
    gboolean show_wm_old = cfg.show_wm_decorations;

    g_free(cfg.playlist_font);
    g_free(cfg.mainwin_font);
    g_free(cfg.gentitle_format);

    prefswin_options_write_data();

    cfg.snap_distance = (guint)CLAMP(
        atoi(gtk_entry_get_text(GTK_ENTRY(prefswin_options_sd_entry))), 0, 1000);
    cfg.playlist_font =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(prefswin_options_font_entry)));
    cfg.mainwin_font =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(prefswin_mainwin_font_entry)));
    cfg.gentitle_format =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(prefswin_title_entry)));
    cfg.pause_between_songs_time = (guint)CLAMP(
        atoi(gtk_entry_get_text(GTK_ENTRY(prefswin_options_pbs_entry))), 0, 1000);
    cfg.mouse_change =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prefswin_options_mouse_spin));

    set_current_output_plugin(selected_oplugin);
    equalizerwin_set_doublesize(cfg.doublesize && cfg.eq_doublesize_linked);

    if (cfg.dim_titlebar) {
        mainwin_menubtn->pb_allow_draw = mainwin_focus;
        mainwin_minimize->pb_allow_draw = mainwin_focus;
        mainwin_shade->pb_allow_draw = mainwin_focus;
        mainwin_close->pb_allow_draw = mainwin_focus;
        equalizerwin_close->pb_allow_draw = equalizerwin_focus;
        playlistwin_shade->pb_allow_draw = playlistwin_focus;
        playlistwin_close->pb_allow_draw = playlistwin_focus;
    } else {
        mainwin_menubtn->pb_allow_draw = TRUE;
        mainwin_minimize->pb_allow_draw = TRUE;
        mainwin_shade->pb_allow_draw = TRUE;
        mainwin_close->pb_allow_draw = TRUE;
        equalizerwin_close->pb_allow_draw = TRUE;
        playlistwin_shade->pb_allow_draw = TRUE;
        playlistwin_close->pb_allow_draw = TRUE;
    }

    if (cfg.get_info_on_load)
        playlist_start_get_info_scan();

    if (mainwin_info->tb_timeout_tag) {
        textbox_set_scroll(mainwin_info, FALSE);
        textbox_set_scroll(mainwin_info, TRUE);
    }

    if (show_wm_old != cfg.show_wm_decorations)
        prefswin_toggle_wm_decorations();

    textbox_set_xfont(mainwin_info, cfg.mainwin_use_xfont, cfg.mainwin_font);
    playlist_list_set_font(cfg.playlist_font);
    playlistwin_update_list();
    mainwin_set_info_text();

    draw_main_window(TRUE);
    draw_playlist_window(TRUE);
    draw_equalizer_window(TRUE);

    save_config();
}

static void prefswin_ok_cb(GtkWidget *w, gpointer data)
{
    prefswin_apply_changes();
    gtk_widget_hide(prefswin);
}

static void prefswin_cancel_cb(GtkWidget *w, gpointer data)
{
    gtk_widget_hide(prefswin);
}

static void prefswin_apply_cb(GtkWidget *w, gpointer data)
{
    prefswin_apply_changes();
}

static gboolean prefswin_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_widget_hide(prefswin);
    return TRUE;
}

/* ---- Helper: one plugin page with frame + scrolled list + buttons ---- */

static GtkWidget *make_plugin_page(const gchar *frame_label,
                                   GtkWidget **view_out,
                                   GtkWidget **config_out,
                                   GtkWidget **about_out,
                                   GtkWidget **use_cbox_out,
                                   GCallback changed_cb,
                                   GCallback config_cb,
                                   GCallback about_cb,
                                   GCallback use_cb)
{
    GtkWidget *frame, *vbox, *sw, *view, *hbox, *hbbox;
    GtkWidget *config_btn, *about_btn, *use_cbox;

    frame = gtk_frame_new(frame_label);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 5);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    sw = make_plugin_listview(&view);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    hbbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_START);
    gtk_box_set_spacing(GTK_BOX(hbbox), 10);
    gtk_box_pack_start(GTK_BOX(hbox), hbbox, TRUE, TRUE, 0);

    config_btn = gtk_button_new_with_label(_("Configure"));
    g_signal_connect(G_OBJECT(config_btn), "clicked", G_CALLBACK(config_cb), view);
    gtk_widget_set_sensitive(config_btn, FALSE);
    gtk_box_pack_start(GTK_BOX(hbbox), config_btn, FALSE, FALSE, 0);

    about_btn = gtk_button_new_with_label(_("About"));
    g_signal_connect(G_OBJECT(about_btn), "clicked", G_CALLBACK(about_cb), view);
    gtk_widget_set_sensitive(about_btn, FALSE);
    gtk_box_pack_start(GTK_BOX(hbbox), about_btn, FALSE, FALSE, 0);

    use_cbox = gtk_check_button_new_with_label(_("Enable plugin"));
    gtk_widget_set_sensitive(use_cbox, FALSE);
    g_signal_connect(G_OBJECT(use_cbox), "toggled", G_CALLBACK(use_cb), view);
    gtk_box_pack_start(GTK_BOX(hbox), use_cbox, FALSE, FALSE, 10);

    g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(view))),
                     "changed", G_CALLBACK(changed_cb), NULL);

    gtk_widget_show_all(frame);

    if (view_out)
        *view_out = view;
    if (config_out)
        *config_out = config_btn;
    if (about_out)
        *about_out = about_btn;
    if (use_cbox_out)
        *use_cbox_out = use_cbox;

    return frame;
}

/* ---- create_prefs_window ---- */

void create_prefs_window(void)
{
    GtkWidget *vbox, *hbox, *frame, *grid, *sw;
    GtkWidget *ok_btn, *cancel_btn, *apply_btn;
    GtkWidget *widget, *label, *btn, *entry;
    GtkAdjustment *adj;

    if (prefswin != NULL)
        return;

    prefswin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(prefswin), _("Preferences"));
    gtk_window_set_resizable(GTK_WINDOW(prefswin), FALSE);
    gtk_window_set_transient_for(GTK_WINDOW(prefswin), GTK_WINDOW(mainwin));
    gtk_container_set_border_width(GTK_CONTAINER(prefswin), 10);
    g_signal_connect(G_OBJECT(prefswin), "delete_event",
                     G_CALLBACK(prefswin_delete_event), NULL);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(prefswin), vbox);

    prefswin_notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), prefswin_notebook, TRUE, TRUE, 0);

    /* ============================================================
     * Tab 0: Audio I/O Plugins
     * ============================================================ */
    {
        GtkWidget *audio_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        /* Input plugins */
        frame = gtk_frame_new(_("Input Plugins"));
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
        gtk_box_pack_start(GTK_BOX(audio_vbox), frame, TRUE, TRUE, 0);

        GtkWidget *ivbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(ivbox), 5);
        gtk_container_add(GTK_CONTAINER(frame), ivbox);

        sw = make_plugin_listview(&prefswin_audio_ilist_view);
        gtk_box_pack_start(GTK_BOX(ivbox), sw, TRUE, TRUE, 0);

        GtkWidget *ihbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(ivbox), ihbox, FALSE, FALSE, 5);

        GtkWidget *ihbbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(ihbbox), GTK_BUTTONBOX_START);
        gtk_box_set_spacing(GTK_BOX(ihbbox), 10);
        gtk_box_pack_start(GTK_BOX(ihbox), ihbbox, TRUE, TRUE, 0);

        prefswin_audio_iconfig = gtk_button_new_with_label(_("Configure"));
        gtk_widget_set_sensitive(prefswin_audio_iconfig, FALSE);
        g_signal_connect(G_OBJECT(prefswin_audio_iconfig), "clicked",
                         G_CALLBACK(prefswin_iconfigure), prefswin_audio_ilist_view);
        gtk_box_pack_start(GTK_BOX(ihbbox), prefswin_audio_iconfig, FALSE, FALSE, 0);

        prefswin_audio_iabout = gtk_button_new_with_label(_("About"));
        gtk_widget_set_sensitive(prefswin_audio_iabout, FALSE);
        g_signal_connect(G_OBJECT(prefswin_audio_iabout), "clicked",
                         G_CALLBACK(prefswin_iabout), prefswin_audio_ilist_view);
        gtk_box_pack_start(GTK_BOX(ihbbox), prefswin_audio_iabout, FALSE, FALSE, 0);

        prefswin_audio_ie_cbox = gtk_check_button_new_with_label(_("Enable plugin"));
        gtk_widget_set_sensitive(prefswin_audio_ie_cbox, FALSE);
        g_signal_connect(G_OBJECT(prefswin_audio_ie_cbox), "toggled",
                         G_CALLBACK(prefswin_ip_toggled), prefswin_audio_ilist_view);
        gtk_box_pack_start(GTK_BOX(ihbox), prefswin_audio_ie_cbox, FALSE, FALSE, 10);

        g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(
                             GTK_TREE_VIEW(prefswin_audio_ilist_view))),
                         "changed", G_CALLBACK(prefswin_ilist_changed), NULL);

        /* Output plugin */
        frame = gtk_frame_new(_("Output Plugin"));
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
        gtk_box_pack_start(GTK_BOX(audio_vbox), frame, FALSE, FALSE, 0);

        GtkWidget *ovbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(ovbox), 5);
        gtk_container_add(GTK_CONTAINER(frame), ovbox);

        prefswin_audio_ocombo = gtk_combo_box_text_new();
        g_signal_connect(G_OBJECT(prefswin_audio_ocombo), "changed",
                         G_CALLBACK(prefswin_output_changed_cb), NULL);
        gtk_box_pack_start(GTK_BOX(ovbox), prefswin_audio_ocombo, TRUE, TRUE, 0);

        GtkWidget *ohbbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(ohbbox), GTK_BUTTONBOX_START);
        gtk_box_set_spacing(GTK_BOX(ohbbox), 10);
        gtk_box_pack_start(GTK_BOX(ovbox), ohbbox, FALSE, FALSE, 6);

        prefswin_audio_oconfig = gtk_button_new_with_label(_("Configure"));
        gtk_widget_set_sensitive(prefswin_audio_oconfig, FALSE);
        g_signal_connect(G_OBJECT(prefswin_audio_oconfig), "clicked",
                         G_CALLBACK(prefswin_oconfigure), NULL);
        gtk_box_pack_start(GTK_BOX(ohbbox), prefswin_audio_oconfig, FALSE, FALSE, 0);

        prefswin_audio_oabout = gtk_button_new_with_label(_("About"));
        gtk_widget_set_sensitive(prefswin_audio_oabout, FALSE);
        g_signal_connect(G_OBJECT(prefswin_audio_oabout), "clicked",
                         G_CALLBACK(prefswin_oabout), NULL);
        gtk_box_pack_start(GTK_BOX(ohbbox), prefswin_audio_oabout, FALSE, FALSE, 0);

        gtk_notebook_append_page(GTK_NOTEBOOK(prefswin_notebook), audio_vbox,
                                 gtk_label_new(_("Audio I/O Plugins")));
    }

    /* ============================================================
     * Tab 1: Effect Plugins
     * ============================================================ */
    {
        GtkWidget *ef_page = make_plugin_page(
            _("Effect Plugins"),
            &prefswin_eplugins_list, &prefswin_eplugins_config,
            &prefswin_eplugins_about, &prefswin_eplugins_use_cbox,
            G_CALLBACK(prefswin_elist_changed),
            G_CALLBACK(prefswin_econfigure),
            G_CALLBACK(prefswin_eabout),
            G_CALLBACK(prefswin_eplugins_use_cb));
        gtk_notebook_append_page(GTK_NOTEBOOK(prefswin_notebook), ef_page,
                                 gtk_label_new(_("Effect Plugins")));
    }

    /* ============================================================
     * Tab 2: General Plugins
     * ============================================================ */
    {
        GtkWidget *gp_page = make_plugin_page(
            _("General Plugins"),
            &prefswin_gplugins_list, &prefswin_gplugins_config,
            &prefswin_gplugins_about, &prefswin_gplugins_use_cbox,
            G_CALLBACK(prefswin_glist_changed),
            G_CALLBACK(prefswin_gconfigure),
            G_CALLBACK(prefswin_gabout),
            G_CALLBACK(prefswin_gplugins_use_cb));
        gtk_notebook_append_page(GTK_NOTEBOOK(prefswin_notebook), gp_page,
                                 gtk_label_new(_("General Plugins")));
    }

    /* ============================================================
     * Tab 3: Visualization Plugins
     * ============================================================ */
    {
        GtkWidget *vp_page = make_plugin_page(
            _("Visualization Plugins"),
            &prefswin_vplugins_list, &prefswin_vplugins_config,
            &prefswin_vplugins_about, &prefswin_vplugins_use_cbox,
            G_CALLBACK(prefswin_vlist_changed),
            G_CALLBACK(prefswin_vconfigure),
            G_CALLBACK(prefswin_vabout),
            G_CALLBACK(prefswin_vplugins_use_cb));
        prefswin_vis_page_idx = gtk_notebook_get_n_pages(GTK_NOTEBOOK(prefswin_notebook));
        gtk_notebook_append_page(GTK_NOTEBOOK(prefswin_notebook), vp_page,
                                 gtk_label_new(_("Visualization Plugins")));
    }

    /* ============================================================
     * Tab 4: Options
     * ============================================================ */
    {
        GtkWidget *opt_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        frame = gtk_frame_new(_("Options"));
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
        gtk_box_pack_start(GTK_BOX(opt_vbox), frame, FALSE, FALSE, 0);

        grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
        gtk_container_set_border_width(GTK_CONTAINER(grid), 5);
        gtk_container_add(GTK_CONTAINER(frame), grid);

        /* Row 0: Read info on ... | Allow multiple instances */
        GtkWidget *gi_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
        gtk_box_pack_start(GTK_BOX(gi_box), gtk_label_new(_("Read info on")),
                           FALSE, FALSE, 0);
        GtkWidget *options_giop =
            prefswin_radio_new_with_label(NULL, NULL, _("play"));
        gtk_widget_set_tooltip_text(options_giop,
            _("Read song title and length only when starting to play"));
        gtk_box_pack_start(GTK_BOX(gi_box), options_giop, FALSE, FALSE, 0);
        GtkWidget *options_giod =
            prefswin_radio_new_with_label(&cfg.get_info_on_demand,
                                          GTK_RADIO_BUTTON(options_giop), _("demand"));
        gtk_widget_set_tooltip_text(options_giod,
            _("Read song title and length when the song is visible in the playlist"));
        gtk_box_pack_start(GTK_BOX(gi_box), options_giod, FALSE, FALSE, 0);
        GtkWidget *options_giol =
            prefswin_radio_new_with_label(&cfg.get_info_on_load,
                                          GTK_RADIO_BUTTON(options_giop), _("load"));
        gtk_widget_set_tooltip_text(options_giol,
            _("Read song title and length as soon as the song is loaded to the playlist"));
        gtk_box_pack_start(GTK_BOX(gi_box), options_giol, FALSE, FALSE, 0);
        gtk_grid_attach(GTK_GRID(grid), gi_box, 0, 0, 1, 1);

        widget = prefswin_option_new_with_label(&cfg.allow_multiple_instances,
                                                _("Allow multiple instances"));
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 0, 1, 1);

        /* Row 1 */
        widget = prefswin_option_new_with_label(&cfg.convert_twenty,
                                                _("Convert %20 to space"));
        gtk_grid_attach(GTK_GRID(grid), widget, 0, 1, 1, 1);
        widget = prefswin_option_new_with_label(&cfg.always_show_cb,
                                                _("Always show clutterbar"));
        gtk_widget_set_tooltip_text(widget,
            _("The \"clutterbar\" is the row of buttons at the left side of the main window"));
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 1, 1, 1);

        /* Row 2 */
        widget = prefswin_option_new_with_label(&cfg.convert_underscore,
                                                _("Convert underscore to space"));
        gtk_grid_attach(GTK_GRID(grid), widget, 0, 2, 1, 1);
        widget = prefswin_option_new_with_label(&cfg.save_window_position,
                                                _("Save window positions"));
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 2, 1, 1);

        /* Row 3 */
        widget = prefswin_option_new_with_label(&cfg.dim_titlebar,
                                                _("Dim titlebar when inactive"));
        gtk_grid_attach(GTK_GRID(grid), widget, 0, 3, 1, 1);
        widget = prefswin_option_new_with_label(&cfg.show_numbers_in_pl,
                                                _("Show numbers in playlist"));
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 3, 1, 1);

        /* Row 4 */
        widget = prefswin_option_new_with_label(&cfg.sort_jump_to_file,
                                                _("Sort \"Jump to file\" alphabetically"));
        gtk_grid_attach(GTK_GRID(grid), widget, 0, 4, 1, 1);
        widget = prefswin_option_new_with_label(&cfg.eq_doublesize_linked,
                                                _("Equalizer doublesize linked"));
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 4, 1, 1);

        /* Row 5 */
        GtkWidget *options_rt =
            prefswin_option_new_with_label(&cfg.use_realtime,
                                           _("Use realtime priority when available"));
        gtk_widget_set_tooltip_text(options_rt,
                                    _("Run XMMS with higher priority (not recommended)"));
        g_signal_connect(G_OBJECT(options_rt), "toggled",
                         G_CALLBACK(prefswin_rt_callback), NULL);
        gtk_grid_attach(GTK_GRID(grid), options_rt, 0, 5, 1, 1);
        widget = prefswin_option_new_with_label(&cfg.smooth_title_scroll,
                                                _("Smooth title scroll"));
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 5, 1, 1);

        /* Row 6: pause between songs | snap distance */
        {
            struct option_info *oi = g_malloc(sizeof(struct option_info));
            GtkWidget *chk = gtk_check_button_new();
            GtkWidget *pbs_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
            oi->button = chk;
            oi->cfg = &cfg.pause_between_songs;
            option_list = g_list_prepend(option_list, oi);
            gtk_box_pack_start(GTK_BOX(pbs_box),
                               gtk_label_new(_("Pause between songs for")),
                               FALSE, FALSE, 0);
            prefswin_options_pbs_entry = gtk_entry_new();
            gtk_entry_set_max_length(GTK_ENTRY(prefswin_options_pbs_entry), 3);
            gtk_widget_set_size_request(prefswin_options_pbs_entry, 30, -1);
            gtk_box_pack_start(GTK_BOX(pbs_box), prefswin_options_pbs_entry,
                               FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(pbs_box), gtk_label_new(_("seconds")),
                               FALSE, FALSE, 0);
            gtk_container_add(GTK_CONTAINER(chk), pbs_box);
            gtk_grid_attach(GTK_GRID(grid), chk, 0, 6, 1, 1);
        }
        {
            struct option_info *oi = g_malloc(sizeof(struct option_info));
            GtkWidget *chk = gtk_check_button_new();
            GtkWidget *sw_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
            oi->button = chk;
            oi->cfg = &cfg.snap_windows;
            option_list = g_list_prepend(option_list, oi);
            gtk_widget_set_tooltip_text(chk,
                _("When moving windows around, snap them together, "
                  "and towards screen edges at this distance"));
            gtk_box_pack_start(GTK_BOX(sw_box),
                               gtk_label_new(_("Snap windows at")), FALSE, FALSE, 0);
            prefswin_options_sd_entry = gtk_entry_new();
            gtk_entry_set_max_length(GTK_ENTRY(prefswin_options_sd_entry), 3);
            gtk_widget_set_size_request(prefswin_options_sd_entry, 30, -1);
            gtk_box_pack_start(GTK_BOX(sw_box), prefswin_options_sd_entry,
                               FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(sw_box), gtk_label_new(_("pixels")),
                               FALSE, FALSE, 0);
            gtk_container_add(GTK_CONTAINER(chk), sw_box);
            gtk_grid_attach(GTK_GRID(grid), chk, 1, 6, 1, 1);
        }

        /* Row 7 */
        widget = prefswin_option_new_with_label(&cfg.show_wm_decorations,
                                                _("Show window manager decorations"));
        gtk_grid_attach(GTK_GRID(grid), widget, 0, 7, 1, 1);
        widget = prefswin_option_new_with_label(&cfg.use_backslash_as_dir_delimiter,
                                                _("Use '\\' as a directory delimiter"));
        gtk_widget_set_tooltip_text(widget,
            _("Recommended if you want to load playlists that were created in MS Windows"));
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 7, 1, 1);

        /* Row 8: mouse wheel percentage | use pl metadata */
        {
            GtkWidget *mouse_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
            label = gtk_label_new(_("Mouse Wheel adjusts Volume by (%)"));
            gtk_box_pack_start(GTK_BOX(mouse_box), label, FALSE, FALSE, 0);
            adj = gtk_adjustment_new((gdouble)cfg.mouse_change, 1, 100, 1, 1, 1);
            prefswin_options_mouse_spin = gtk_spin_button_new(adj, 1, 0);
            gtk_widget_set_size_request(prefswin_options_mouse_spin, 45, -1);
            gtk_box_pack_start(GTK_BOX(mouse_box), prefswin_options_mouse_spin,
                               FALSE, FALSE, 0);
            gtk_grid_attach(GTK_GRID(grid), mouse_box, 0, 8, 1, 1);
        }
        widget = prefswin_option_new_with_label(&cfg.use_pl_metadata,
                                                _("Use meta-data in playlists"));
        gtk_widget_set_tooltip_text(widget,
            _("Store information such as song title and length to playlists"));
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 8, 1, 1);

        gtk_widget_show_all(opt_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(prefswin_notebook), opt_vbox,
                                 gtk_label_new(_("Options")));
    }

    /* ============================================================
     * Tab 5: Fonts
     * ============================================================ */
    {
        GtkWidget *fonts_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        frame = gtk_frame_new(_("Options"));
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
        gtk_box_pack_start(GTK_BOX(fonts_vbox), frame, FALSE, FALSE, 0);
        GtkWidget *fo_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(frame), fo_vbox);
        widget = prefswin_option_new_with_label(
            &cfg.use_fontsets,
            _("Use fontsets (Enable for multi-byte charset support)"));
        gtk_box_pack_start(GTK_BOX(fo_vbox), widget, TRUE, TRUE, 0);

        frame = gtk_frame_new(_("Playlist"));
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
        gtk_box_pack_start(GTK_BOX(fonts_vbox), frame, FALSE, FALSE, 0);
        GtkWidget *pf_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(pf_vbox), 5);
        gtk_container_add(GTK_CONTAINER(frame), pf_vbox);
        GtkWidget *pf_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(pf_vbox), pf_hbox, TRUE, TRUE, 0);
        prefswin_options_font_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(pf_hbox), prefswin_options_font_entry,
                           TRUE, TRUE, 0);
        btn = gtk_button_new_with_label(_("Browse"));
        gtk_widget_set_size_request(btn, 85, -1);
        g_signal_connect(G_OBJECT(btn), "clicked",
                         G_CALLBACK(prefswin_font_browse_cb), NULL);
        gtk_box_pack_start(GTK_BOX(pf_hbox), btn, FALSE, TRUE, 0);

        frame = gtk_frame_new(_("Main Window"));
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
        gtk_box_pack_start(GTK_BOX(fonts_vbox), frame, FALSE, FALSE, 0);
        GtkWidget *mf_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(mf_vbox), 5);
        gtk_container_add(GTK_CONTAINER(frame), mf_vbox);
        widget = prefswin_option_new_with_label(&cfg.mainwin_use_xfont, _("Use X font"));
        gtk_box_pack_start(GTK_BOX(mf_vbox), widget, TRUE, TRUE, 0);
        GtkWidget *mf_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(mf_vbox), mf_hbox, TRUE, TRUE, 0);
        prefswin_mainwin_font_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(mf_hbox), prefswin_mainwin_font_entry,
                           TRUE, TRUE, 0);
        btn = gtk_button_new_with_label(_("Browse"));
        gtk_widget_set_size_request(btn, 85, -1);
        g_signal_connect(G_OBJECT(btn), "clicked",
                         G_CALLBACK(prefswin_mainwin_font_browse_cb), NULL);
        gtk_box_pack_start(GTK_BOX(mf_hbox), btn, FALSE, TRUE, 0);

        gtk_widget_show_all(fonts_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(prefswin_notebook), fonts_vbox,
                                 gtk_label_new(_("Fonts")));
    }

    /* ============================================================
     * Tab 6: Title
     * ============================================================ */
    {
        GtkWidget *title_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        frame = gtk_frame_new(_("Title"));
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
        gtk_box_pack_start(GTK_BOX(title_vbox), frame, FALSE, FALSE, 0);
        GtkWidget *tv2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(tv2), 5);
        gtk_container_add(GTK_CONTAINER(frame), tv2);
        GtkWidget *th = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(tv2), th, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(th), gtk_label_new(_("Title format:")),
                           FALSE, FALSE, 0);
        prefswin_title_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(th), prefswin_title_entry, TRUE, TRUE, 0);
        GtkWidget *desc_widget = xmms_titlestring_descriptions("pagfFetndyc", 2);
        gtk_box_pack_start(GTK_BOX(tv2), desc_widget, FALSE, FALSE, 0);

        frame = gtk_frame_new(_("Advanced Title Options"));
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
        gtk_box_pack_start(GTK_BOX(title_vbox), frame, FALSE, FALSE, 0);
        GtkWidget *adv_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(adv_vbox), 5);
        gtk_container_add(GTK_CONTAINER(frame), adv_vbox);
        GtkWidget *more_lbl =
            gtk_label_new(_("%0.2n - Display a 0 padded 2 char long tracknumber\n"
                            "\n"
                            "For more details, please read the included README or "
                            "http://www.xmms.org/docs/readme.php"));
        gtk_label_set_justify(GTK_LABEL(more_lbl), GTK_JUSTIFY_LEFT);
        gtk_label_set_xalign(GTK_LABEL(more_lbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(adv_vbox), more_lbl, FALSE, FALSE, 0);

        gtk_widget_show_all(title_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(prefswin_notebook), title_vbox,
                                 gtk_label_new(_("Title")));
    }

    /* ============================================================
     * OK / Cancel / Apply
     * ============================================================ */
    hbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    ok_btn = gtk_button_new_with_label(_("OK"));
    g_signal_connect(G_OBJECT(ok_btn), "clicked", G_CALLBACK(prefswin_ok_cb), NULL);
    gtk_widget_set_can_default(ok_btn, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), ok_btn, TRUE, TRUE, 0);
    prefswin_ok = ok_btn;

    cancel_btn = gtk_button_new_with_label(_("Cancel"));
    g_signal_connect(G_OBJECT(cancel_btn), "clicked", G_CALLBACK(prefswin_cancel_cb), NULL);
    gtk_widget_set_can_default(cancel_btn, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), cancel_btn, TRUE, TRUE, 0);

    apply_btn = gtk_button_new_with_label(_("Apply"));
    g_signal_connect(G_OBJECT(apply_btn), "clicked", G_CALLBACK(prefswin_apply_cb), NULL);
    gtk_widget_set_can_default(apply_btn, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), apply_btn, TRUE, TRUE, 0);

    /* Populate all plugin lists */
    add_input_plugins(GTK_TREE_VIEW(prefswin_audio_ilist_view));
    add_output_plugins(GTK_COMBO_BOX_TEXT(prefswin_audio_ocombo));
    add_effect_plugins(GTK_TREE_VIEW(prefswin_eplugins_list));
    add_general_plugins(GTK_TREE_VIEW(prefswin_gplugins_list));
    add_vis_plugins(GTK_TREE_VIEW(prefswin_vplugins_list));

    /* suppress unused-variable warnings */
    (void)entry;
    (void)sw;
}

/* ---- show_prefs_window ---- */

void show_prefs_window(void)
{
    gchar temp[16];

    if (prefswin != NULL && gtk_widget_get_visible(prefswin)) {
        gtk_window_present(GTK_WINDOW(prefswin));
        return;
    }

    if (prefswin == NULL)
        create_prefs_window();

    is_opening = TRUE;

    gtk_entry_set_text(GTK_ENTRY(prefswin_options_font_entry), cfg.playlist_font);
    gtk_entry_set_text(GTK_ENTRY(prefswin_mainwin_font_entry), cfg.mainwin_font);
    gtk_entry_set_text(GTK_ENTRY(prefswin_title_entry), cfg.gentitle_format);

    g_snprintf(temp, sizeof(temp), "%u", (guint)cfg.snap_distance);
    gtk_entry_set_text(GTK_ENTRY(prefswin_options_sd_entry), temp);

    prefswin_options_read_data();

    g_snprintf(temp, sizeof(temp), "%u", (guint)cfg.pause_between_songs_time);
    gtk_entry_set_text(GTK_ENTRY(prefswin_options_pbs_entry), temp);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(prefswin_options_mouse_spin),
                              (gdouble)cfg.mouse_change);

    gtk_widget_show_all(prefswin);
    gtk_widget_grab_default(prefswin_ok);

    is_opening = FALSE;
}

/* ---- prefswin_vplugins_rescan ---- */

void prefswin_vplugins_rescan(void)
{
    GtkTreeView *view;
    GtkAdjustment *adj;
    gdouble pos;
    gint sel;

    if (prefswin == NULL || prefswin_vplugins_list == NULL)
        return;

    view = GTK_TREE_VIEW(prefswin_vplugins_list);
    sel = prefswin_get_selected_row(view);
    adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
    pos = gtk_adjustment_get_value(adj);

    add_vis_plugins(view);

    gtk_adjustment_set_value(adj, pos);
    if (sel >= 0) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(sel, -1);
        gtk_tree_selection_select_path(gtk_tree_view_get_selection(view), path);
        gtk_tree_path_free(path);
    }
}

/* ---- prefswin_show_vis_plugins_page ---- */

void prefswin_show_vis_plugins_page(void)
{
    show_prefs_window();
    if (prefswin_notebook)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(prefswin_notebook),
                                      prefswin_vis_page_idx);
}
