#!/usr/bin/env python3
"""GTK3 migration pass 4 — fixes remaining files."""

import re
import os

BASE = "/data/src/dev-env/xmms"


def apply_common_gtk2(content):
    """Apply all standard GTK2→GTK3 patterns."""
    # gtk_signal_connect(GTK_OBJECT(...), ..., GTK_SIGNAL_FUNC(...), ...)
    content = re.sub(
        r'\bgtk_signal_connect\s*\(\s*GTK_OBJECT\s*\(', 'g_signal_connect(G_OBJECT(', content
    )
    # gtk_signal_connect_object(GTK_OBJECT(...), ..., GTK_SIGNAL_FUNC(...), GTK_OBJECT(...))
    content = re.sub(
        r'\bgtk_signal_connect_object\s*\(\s*GTK_OBJECT\s*\(', 'g_signal_connect_swapped(G_OBJECT(', content
    )
    # Trailing GTK_SIGNAL_FUNC( → G_CALLBACK(
    content = content.replace('GTK_SIGNAL_FUNC(', 'G_CALLBACK(')
    # Trailing GTK_OBJECT( used as last arg of g_signal_connect_swapped → G_OBJECT(
    content = re.sub(
        r'(g_signal_connect_swapped\([^;]+,\s*)GTK_OBJECT\s*\(([^)]+)\)',
        r'\1G_OBJECT(\2)',
        content
    )
    # GTK_WIDGET_SET_FLAGS(x, GTK_CAN_DEFAULT) → gtk_widget_set_can_default(x, TRUE)
    content = re.sub(
        r'GTK_WIDGET_SET_FLAGS\s*\(([^,]+),\s*GTK_CAN_DEFAULT\)',
        r'gtk_widget_set_can_default(\1, TRUE)',
        content
    )
    # GTK_WINDOW_DIALOG → GTK_WINDOW_TOPLEVEL
    content = content.replace('GTK_WINDOW_DIALOG', 'GTK_WINDOW_TOPLEVEL')
    # gtk_window_set_policy(win, ...) → remove
    content = re.sub(r'\s*gtk_window_set_policy\s*\([^;]+\)\s*;', '', content)
    # gtk_container_border_width → gtk_container_set_border_width
    content = content.replace('gtk_container_border_width(', 'gtk_container_set_border_width(')
    # gtk_widget_set_usize → gtk_widget_set_size_request
    content = content.replace('gtk_widget_set_usize(', 'gtk_widget_set_size_request(')
    # gtk_hbox_new(x, y) → gtk_box_new(GTK_ORIENTATION_HORIZONTAL, y)
    content = re.sub(
        r'\bgtk_hbox_new\s*\([^,]+,\s*([^)]+)\)',
        r'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, \1)',
        content
    )
    # gtk_vbox_new(x, y) → gtk_box_new(GTK_ORIENTATION_VERTICAL, y)
    content = re.sub(
        r'\bgtk_vbox_new\s*\([^,]+,\s*([^)]+)\)',
        r'gtk_box_new(GTK_ORIENTATION_VERTICAL, \1)',
        content
    )
    # gtk_hbutton_box_new() → gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)
    content = content.replace('gtk_hbutton_box_new()', 'gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)')
    # GTK_DIALOG(x)->vbox → gtk_dialog_get_content_area(GTK_DIALOG(x))
    content = re.sub(
        r'GTK_DIALOG\((\w+)\)->vbox\b',
        r'gtk_dialog_get_content_area(GTK_DIALOG(\1))',
        content
    )
    # GTK_DIALOG(x)->action_area → gtk_dialog_get_action_area(GTK_DIALOG(x))
    content = re.sub(
        r'GTK_DIALOG\((\w+)\)->action_area\b',
        r'gtk_dialog_get_action_area(GTK_DIALOG(\1))',
        content
    )
    # GTK_ADJUSTMENT(x)->value → gtk_adjustment_get_value(GTK_ADJUSTMENT(x))
    content = re.sub(
        r'GTK_ADJUSTMENT\s*\(([^)]+)\)->value\b',
        r'gtk_adjustment_get_value(GTK_ADJUSTMENT(\1))',
        content
    )
    # GtkObject * <adj_var> → GtkAdjustment * <adj_var>
    content = re.sub(r'\bGtkObject\b(\s+\*)', r'GtkAdjustment\1', content)
    # gtk_timeout_remove → g_source_remove
    content = content.replace('gtk_timeout_remove(', 'g_source_remove(')
    # gtk_timeout_add → g_timeout_add
    content = content.replace('gtk_timeout_add(', 'g_timeout_add(')
    # gtk_radio_button_group → gtk_radio_button_get_group
    content = content.replace('gtk_radio_button_group(', 'gtk_radio_button_get_group(')
    # GDK_Escape → GDK_KEY_Escape
    content = content.replace('GDK_Escape', 'GDK_KEY_Escape')
    # gtk_box_pack_start_defaults(box, widget) → gtk_box_pack_start(box, widget, TRUE, TRUE, 0)
    content = re.sub(
        r'gtk_box_pack_start_defaults\s*\(([^,]+),\s*([^)]+)\)',
        r'gtk_box_pack_start(\1, \2, TRUE, TRUE, 0)',
        content
    )
    # gdk_window_raise(widget->window) → gdk_window_raise(gtk_widget_get_window(widget))
    content = re.sub(
        r'gdk_window_raise\s*\((\w+)->window\)',
        r'gdk_window_raise(gtk_widget_get_window(\1))',
        content
    )
    # widget->window → gtk_widget_get_window(widget) (only in direct field access)
    content = re.sub(
        r'(\b\w+)->window\b(?!\s*=)',
        r'gtk_widget_get_window(\1)',
        content
    )
    return content


def fix_echo_gui(content):
    """Fix Effect/echo_plugin/gui.c specific issues."""
    content = apply_common_gtk2(content)
    return content


def fix_stereo(content):
    """Fix Effect/stereo_plugin/stereo.c specific issues."""
    content = apply_common_gtk2(content)
    return content


def fix_voice_about(content):
    """Fix Effect/voice/about.c."""
    content = apply_common_gtk2(content)
    return content


def fix_tonegen(content):
    """Fix Input/tonegen/tonegen.c.
    Line 58: gtk_signal_connect(GTK_OBJECT(box), "destroy", gtk_widget_destroyed, &box);
    Note: no GTK_SIGNAL_FUNC here, just a bare function pointer."""
    content = apply_common_gtk2(content)
    # fix remaining gtk_signal_connect with just bare function (no GTK_SIGNAL_FUNC)
    content = re.sub(
        r'\bgtk_signal_connect\s*\(\s*GTK_OBJECT\s*\(', 'g_signal_connect(G_OBJECT(', content
    )
    return content


def fix_mpg123_http(content):
    """Fix Input/mpg123/http.c."""
    content = apply_common_gtk2(content)
    return content


def fix_l2tables_h(content):
    """Fix Input/mpg123/l2tables.h - add mpg123.h include for struct al_table."""
    if '#include "mpg123.h"' not in content:
        # Add after the comment block at the start
        content = re.sub(
            r'(/\*[^*]*\*/\s*\n)',
            r'\1#include "mpg123.h"\n\n',
            content,
            count=1
        )
    return content


def fix_vorbis_h(content):
    """Fix Input/vorbis/vorbis.h - add glib include."""
    if '#include <glib.h>' not in content:
        content = re.sub(
            r'(#ifndef __VORBIS_H__\n#define __VORBIS_H__\n)',
            r'\1\n#include <glib.h>\n',
            content
        )
    return content


def fix_vorbis_http_h(content):
    """Fix Input/vorbis/http.h - add glib include."""
    if '#include <glib.h>' not in content:
        content = re.sub(
            r'(#ifndef __HTTP_H__\n#define __HTTP_H__\n)',
            r'\1\n#include <glib.h>\n',
            content
        )
    return content


def fix_vorbis_c(content):
    """Fix Input/vorbis/vorbis.c."""
    content = apply_common_gtk2(content)
    return content


def fix_vorbis_http_c(content):
    """Fix Input/vorbis/http.c."""
    content = apply_common_gtk2(content)
    return content


def fix_vorbis_configure(content):
    """Fix Input/vorbis/configure.c."""
    content = apply_common_gtk2(content)
    return content


def fix_vorbis_fileinfo(content):
    """Fix Input/vorbis/fileinfo.c."""
    content = apply_common_gtk2(content)

    # GTK_COMBO(genre_combo)->entry → gtk_bin_get_child(GTK_BIN(genre_combo))
    content = content.replace(
        'GTK_ENTRY(GTK_COMBO(genre_combo)->entry)',
        'GTK_ENTRY(gtk_bin_get_child(GTK_BIN(genre_combo)))'
    )

    # gtk_combo_new() → gtk_combo_box_text_new_with_entry()
    content = content.replace('gtk_combo_new()', 'gtk_combo_box_text_new_with_entry()')

    # gtk_combo_set_popdown_strings(GTK_COMBO(genre_combo), genre_list)
    # → loop over genre_list with gtk_combo_box_text_append_text
    content = re.sub(
        r'gtk_combo_set_popdown_strings\s*\(GTK_COMBO\(genre_combo\),\s*genre_list\)\s*;',
        r'{ GList *_gl; for (_gl = genre_list; _gl; _gl = _gl->next) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(genre_combo), (const gchar *)_gl->data); }',
        content
    )

    # gtk_entry_new_with_max_length(4) → two-liner (for tracknumber_entry)
    content = re.sub(
        r'(tracknumber_entry\s*=\s*)gtk_entry_new_with_max_length\s*\((\d+)\)\s*;',
        r'\1gtk_entry_new();\n        gtk_entry_set_max_length(GTK_ENTRY(tracknumber_entry), \2);',
        content
    )

    # gtk_signal_connect_object last arg GTK_OBJECT → G_OBJECT (remaining)
    content = re.sub(
        r'(g_signal_connect_swapped\([^;]+),\s*GTK_OBJECT\s*\((\w+)\)\s*\)',
        r'\1, G_OBJECT(\2))',
        content
    )

    return content


def fix_cdaudio_h(content):
    """Fix Input/cdaudio/cdaudio.h — add Linux fallback for linux/cdrom.h and DEV_MIXER."""
    # Fix the conditional include: add __linux__ fallback
    old = '#ifdef HAVE_LINUX_CDROM_H\n#    include <linux/cdrom.h>\n#elif defined HAVE_SYS_CDIO_H'
    new = ('#ifdef HAVE_LINUX_CDROM_H\n#    include <linux/cdrom.h>\n'
           '#elif defined(__linux__)\n#    include <linux/cdrom.h>\n'
           '#elif defined HAVE_SYS_CDIO_H')
    content = content.replace(old, new)

    # Add DEV_MIXER fallback near end of guard block (before #endif)
    if 'DEV_MIXER' not in content:
        last_endif_pos = content.rfind('#endif')
        if last_endif_pos != -1:
            insert = '\n/* GTK3: DEV_MIXER fallback */\n#ifndef DEV_MIXER\n#    define DEV_MIXER "/dev/mixer"\n#endif\n\n'
            content = content[:last_endif_pos] + insert + content[last_endif_pos:]
    return content


def fix_cdaudio_c(content):
    """Fix Input/cdaudio/cdaudio.c — gtk_timeout → g_source/g_timeout."""
    content = apply_common_gtk2(content)
    return content


def fix_cddb_c(content):
    """Fix Input/cdaudio/cddb.c — GtkCList → GtkTreeView/GtkTextView + common GTK2 patterns."""
    content = apply_common_gtk2(content)

    # Replace server_clist global declaration
    content = content.replace(
        'static GtkWidget *debug_window, *debug_clist;',
        'static GtkWidget *debug_window;\nstatic GtkTextView *debug_textview_widget;'
    )

    # Fix cddb_server_dialog_ok_cb: GTK_CLIST(server_clist)->selection
    old_ok_cb = '''\
static void cddb_server_dialog_ok_cb(GtkWidget *w, gpointer data)
{
    char *text;
    int pos;
    GtkEntry *entry = GTK_ENTRY(data);

    if (!GTK_CLIST(server_clist)->selection)
        return;
    pos = GPOINTER_TO_INT(GTK_CLIST(server_clist)->selection->data);
    gtk_clist_get_text(GTK_CLIST(server_clist), pos, 0, &text);
    cdda_cddb_set_server(text);
    gtk_entry_set_text(entry, text);
    gtk_widget_destroy(server_dialog);
}'''
    new_ok_cb = '''\
static void cddb_server_dialog_ok_cb(GtkWidget *w, gpointer data)
{
    char *text = NULL;
    GtkEntry *entry = GTK_ENTRY(data);
    GtkTreeSelection *sel;
    GtkTreeModel *model;
    GtkTreeIter iter;

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(server_clist));
    if (!gtk_tree_selection_get_selected(sel, &model, &iter))
        return;
    gtk_tree_model_get(model, &iter, 0, &text, -1);
    if (!text)
        return;
    cdda_cddb_set_server(text);
    gtk_entry_set_text(entry, text);
    g_free(text);
    gtk_widget_destroy(server_dialog);
}'''
    content = content.replace(old_ok_cb, new_ok_cb)

    # Fix cddb_server_dialog_select callback signature (now row-activated)
    old_select = '''\
static void cddb_server_dialog_select(GtkWidget *w, gint row, gint column, GdkEvent *event,
                                      gpointer data)
{
    if (event->type == GDK_2BUTTON_PRESS)
        cddb_server_dialog_ok_cb(NULL, data);
}'''
    new_select = '''\
static void cddb_server_dialog_select(GtkTreeView *treeview, GtkTreePath *path,
                                      GtkTreeViewColumn *col, gpointer data)
{
    cddb_server_dialog_ok_cb(NULL, data);
}'''
    content = content.replace(old_select, new_select)

    # Replace server_clist creation block
    old_clist_create = '''\
    server_clist = gtk_clist_new_with_titles(4, titles);
    gtk_signal_connect(GTK_OBJECT(server_clist), "select-row", cddb_server_dialog_select, data);
    gtk_box_pack_start(GTK_BOX(vbox), server_clist, TRUE, TRUE, 0);'''
    new_clist_create = '''\
    {
        GtkListStore *_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
        const char *_col_names[] = {titles[0], titles[1], titles[2], titles[3]};
        int _ci;
        server_clist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_store));
        g_object_unref(_store);
        for (_ci = 0; _ci < 4; _ci++) {
            GtkCellRenderer *_r = gtk_cell_renderer_text_new();
            GtkTreeViewColumn *_c = gtk_tree_view_column_new_with_attributes(_col_names[_ci], _r, "text", _ci, NULL);
            gtk_tree_view_column_set_resizable(_c, TRUE);
            gtk_tree_view_append_column(GTK_TREE_VIEW(server_clist), _c);
        }
        g_signal_connect(G_OBJECT(server_clist), "row-activated", G_CALLBACK(cddb_server_dialog_select), data);
    }
    gtk_box_pack_start(GTK_BOX(vbox), server_clist, TRUE, TRUE, 0);'''
    content = content.replace(old_clist_create, new_clist_create)

    # Replace the while(servers) append block in cdda_cddb_show_server_dialog
    old_append = '''\
    while (servers) {
        char *row[4];
        int i;

        row[0] = g_strdup(((char **)servers->data)[0]);
        row[1] = cddb_position_string(((char **)servers->data)[4]);
        row[2] = cddb_position_string(((char **)servers->data)[5]);
        row[3] = g_strdup(((char **)servers->data)[6]);
        gtk_clist_append(GTK_CLIST(server_clist), row);
        for (i = 0; i < 4; i++)
            g_free(row[i]);
        g_strfreev(servers->data);
        servers = g_list_next(servers);
    }
    g_list_free(servers);
    gtk_clist_columns_autosize(GTK_CLIST(server_clist));'''
    new_append = '''\
    {
        GtkListStore *_store2 = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(server_clist)));
        while (servers) {
            GtkTreeIter _iter;
            char *_r0 = g_strdup(((char **)servers->data)[0]);
            char *_r1 = cddb_position_string(((char **)servers->data)[4]);
            char *_r2 = cddb_position_string(((char **)servers->data)[5]);
            char *_r3 = g_strdup(((char **)servers->data)[6]);
            gtk_list_store_append(_store2, &_iter);
            gtk_list_store_set(_store2, &_iter, 0, _r0, 1, _r1, 2, _r2, 3, _r3, -1);
            g_free(_r0); g_free(_r1); g_free(_r2); g_free(_r3);
            g_strfreev(servers->data);
            servers = g_list_next(servers);
        }
        g_list_free(servers);
    }'''
    content = content.replace(old_append, new_append)

    # Fix cddb_update_log_window: replace clist freeze/append/thaw/moveto with textview inserts
    old_update = '''\
        GDK_THREADS_ENTER();
        gtk_clist_freeze(GTK_CLIST(debug_clist));
        for (temp = temp_messages; temp; temp = temp->next) {
            char *text = temp->data;
            gtk_clist_append(GTK_CLIST(debug_clist), &text);
            g_free(text);
        }
        gtk_clist_columns_autosize(GTK_CLIST(debug_clist));
        gtk_clist_thaw(GTK_CLIST(debug_clist));
        gtk_clist_moveto(GTK_CLIST(debug_clist), GTK_CLIST(debug_clist)->rows - 1, -1, 0.5, 0);
        GDK_THREADS_LEAVE();'''
    new_update = '''\
        GDK_THREADS_ENTER();
        {
            GtkTextBuffer *_buf = gtk_text_view_get_buffer(debug_textview_widget);
            GtkTextIter _end;
            for (temp = temp_messages; temp; temp = temp->next) {
                char *text = temp->data;
                gtk_text_buffer_get_end_iter(_buf, &_end);
                gtk_text_buffer_insert(_buf, &_end, text, -1);
                gtk_text_buffer_insert(_buf, &_end, "\\n", -1);
                g_free(text);
            }
            gtk_text_buffer_get_end_iter(_buf, &_end);
            gtk_text_view_scroll_to_iter(debug_textview_widget, &_end, 0.0, FALSE, 0.0, 1.0);
        }
        GDK_THREADS_LEAVE();'''
    content = content.replace(old_update, new_update)

    # Fix cdda_cddb_show_network_window: replace debug_clist creation
    old_debug = '''\
    scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    debug_clist = gtk_clist_new(1);
    gtk_container_add(GTK_CONTAINER(scroll_win), debug_clist);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_win, TRUE, TRUE, 0);

    temp = debug_messages;
    while (temp) {
        gtk_clist_prepend(GTK_CLIST(debug_clist), (gchar **)&temp->data);
        temp = g_list_next(temp);
    }'''
    new_debug = '''\
    scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    {
        GtkWidget *_tv = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(_tv), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(_tv), FALSE);
        debug_textview_widget = GTK_TEXT_VIEW(_tv);
        gtk_container_add(GTK_CONTAINER(scroll_win), _tv);
    }
    gtk_box_pack_start(GTK_BOX(vbox), scroll_win, TRUE, TRUE, 0);

    temp = debug_messages;
    {
        GtkTextBuffer *_buf2 = gtk_text_view_get_buffer(debug_textview_widget);
        GtkTextIter _end2;
        while (temp) {
            gtk_text_buffer_get_end_iter(_buf2, &_end2);
            gtk_text_buffer_insert(_buf2, &_end2, (gchar *)temp->data, -1);
            gtk_text_buffer_insert(_buf2, &_end2, "\\n", -1);
            temp = g_list_next(temp);
        }
    }'''
    content = content.replace(old_debug, new_debug)

    # Fix end of debug window: 3 lines after close button
    old_end = '''\
    gtk_clist_columns_autosize(GTK_CLIST(debug_clist));
    gtk_clist_set_button_actions(GTK_CLIST(debug_clist), 0, GTK_BUTTON_IGNORED);
    gtk_clist_moveto(GTK_CLIST(debug_clist), GTK_CLIST(debug_clist)->rows - 1, -1, 0, 0);

    cddb_timeout_id = gtk_timeout_add(500, cddb_update_log_window, NULL);'''
    new_end = '''\
    /* GTK3: GtkTextView replaces GtkCList; no column autosize or moveto needed here */
    cddb_timeout_id = g_timeout_add(500, cddb_update_log_window, NULL);'''
    content = content.replace(old_end, new_end)

    # cddb_quit: gtk_timeout_remove → g_source_remove (already done by apply_common_gtk2)

    return content


def fix_cdaudio_configure(content):
    """Fix Input/cdaudio/configure.c."""
    content = apply_common_gtk2(content)
    return content


# -----------------------------------------------------------------------
# File registry
# -----------------------------------------------------------------------
FILES = {
    "Effect/echo_plugin/gui.c": fix_echo_gui,
    "Effect/stereo_plugin/stereo.c": fix_stereo,
    "Effect/voice/about.c": fix_voice_about,
    "Input/tonegen/tonegen.c": fix_tonegen,
    "Input/mpg123/http.c": fix_mpg123_http,
    "Input/mpg123/l2tables.h": fix_l2tables_h,
    "Input/vorbis/vorbis.h": fix_vorbis_h,
    "Input/vorbis/http.h": fix_vorbis_http_h,
    "Input/vorbis/vorbis.c": fix_vorbis_c,
    "Input/vorbis/http.c": fix_vorbis_http_c,
    "Input/vorbis/configure.c": fix_vorbis_configure,
    "Input/vorbis/fileinfo.c": fix_vorbis_fileinfo,
    "Input/cdaudio/cdaudio.h": fix_cdaudio_h,
    "Input/cdaudio/cdaudio.c": fix_cdaudio_c,
    "Input/cdaudio/cddb.c": fix_cddb_c,
    "Input/cdaudio/configure.c": fix_cdaudio_configure,
}

for rel, fixer in FILES.items():
    path = os.path.join(BASE, rel)
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            original = f.read()
        fixed = fixer(original)
        if fixed != original:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(fixed)
            print(f"FIXED: {rel}")
        else:
            print(f"NO CHANGE: {rel}")
    except Exception as e:
        print(f"ERROR in {rel}: {e}")
