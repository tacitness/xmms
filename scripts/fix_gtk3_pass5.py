#!/usr/bin/env python3
"""GTK3 migration pass 5 — fixes remaining issues."""

import re
import os
import glob

BASE = "/data/src/dev-env/xmms"


def wrap_bare_callbacks(content):
    """Wrap bare callback identifiers in g_signal_connect calls with G_CALLBACK()."""
    # Match g_signal_connect(G_OBJECT(...), "signal", SomeIdentifier, data)
    # where SomeIdentifier is NOT already G_CALLBACK(...)
    content = re.sub(
        r'(g_signal_connect(?:_swapped)?\s*\([^,]+,\s*"[^"]+",\s*)(?!G_CALLBACK\()(\w+)',
        r'\1G_CALLBACK(\2)',
        content
    )
    return content


def apply_common_gtk2(content):
    """Apply all standard GTK2→GTK3 patterns."""
    content = re.sub(
        r'\bgtk_signal_connect\s*\(\s*GTK_OBJECT\s*\(', 'g_signal_connect(G_OBJECT(', content
    )
    content = re.sub(
        r'\bgtk_signal_connect_object\s*\(\s*GTK_OBJECT\s*\(', 'g_signal_connect_swapped(G_OBJECT(', content
    )
    content = content.replace('GTK_SIGNAL_FUNC(', 'G_CALLBACK(')
    content = re.sub(
        r'\bGTK_WIDGET_SET_FLAGS\s*\(([^,]+),\s*GTK_CAN_DEFAULT\)',
        r'gtk_widget_set_can_default(\1, TRUE)',
        content
    )
    content = content.replace('GTK_WINDOW_DIALOG', 'GTK_WINDOW_TOPLEVEL')
    content = re.sub(r'\s*gtk_window_set_policy\s*\([^;]+\)\s*;', '', content)
    content = content.replace('gtk_container_border_width(', 'gtk_container_set_border_width(')
    content = content.replace('gtk_widget_set_usize(', 'gtk_widget_set_size_request(')
    content = re.sub(r'\bgtk_hbox_new\s*\([^,]+,\s*([^)]+)\)',
                     r'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, \1)', content)
    content = re.sub(r'\bgtk_vbox_new\s*\([^,]+,\s*([^)]+)\)',
                     r'gtk_box_new(GTK_ORIENTATION_VERTICAL, \1)', content)
    content = content.replace('gtk_hbutton_box_new()', 'gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)')
    content = re.sub(r'GTK_DIALOG\((\w+)\)->vbox\b',
                     r'gtk_dialog_get_content_area(GTK_DIALOG(\1))', content)
    content = re.sub(r'GTK_DIALOG\((\w+)\)->action_area\b',
                     r'gtk_dialog_get_action_area(GTK_DIALOG(\1))', content)
    content = content.replace('gtk_timeout_remove(', 'g_source_remove(')
    content = content.replace('gtk_timeout_add(', 'g_timeout_add(')
    content = content.replace('gtk_radio_button_group(', 'gtk_radio_button_get_group(')
    content = content.replace('GDK_Escape', 'GDK_KEY_Escape')
    content = re.sub(r'gtk_box_pack_start_defaults\s*\(([^,]+),\s*([^)]+)\)',
                     r'gtk_box_pack_start(\1, \2, TRUE, TRUE, 0)', content)
    content = re.sub(r'gdk_window_raise\s*\((\w+)->window\)',
                     r'gdk_window_raise(gtk_widget_get_window(\1))', content)
    # Any remaining gtk_signal_connect without GTK_OBJECT (shouldn't be any but be safe)
    content = re.sub(r'\bgtk_signal_connect\s*\(', 'g_signal_connect(', content)
    content = re.sub(r'\bgtk_signal_connect_object\s*\(', 'g_signal_connect_swapped(', content)
    # gtk_object_get_data → g_object_get_data
    content = re.sub(r'\bgtk_object_get_data\s*\(\s*GTK_OBJECT\s*\(',
                     'g_object_get_data(G_OBJECT(', content)
    # gtk_object_set_data → g_object_set_data
    content = re.sub(r'\bgtk_object_set_data\s*\(\s*GTK_OBJECT\s*\(',
                     'g_object_set_data(G_OBJECT(', content)
    # Remaining GTK_OBJECT( → G_OBJECT( (e.g. last parameter of g_signal_connect_swapped)
    content = re.sub(r'\bGTK_OBJECT\s*\(', 'G_OBJECT(', content)
    # Wrap bare callbacks in g_signal_connect
    content = wrap_bare_callbacks(content)
    return content


def fix_general_files(content):
    """Apply common GTK3 fixes to General/* files."""
    return apply_common_gtk2(content)


def fix_joystick_configure(content):
    """Fix General/joystick/configure.c — GtkPacker, GtkOptionMenu, GTK_SIDE_*."""
    content = apply_common_gtk2(content)

    # Remove the pack_pos array with GTK_SIDE_* (not needed with Grid)
    content = re.sub(
        r'static gint pack_pos\s*\[4\]\s*=[^,].*?GTK_SIDE_RIGHT\}',
        '/* GTK3: GtkPacker removed; directionals use GtkGrid now */',
        content,
        flags=re.DOTALL
    )
    # Replace hist_val[4] on same line if it was combined
    # After above, we might have: "/* ... */, hist_val[4];" — fix
    content = re.sub(r'/\* GTK3[^*]*\*/,(\s*hist_val\[4\])', r'\1', content)

    # Fix menu creation: gtk_menu_new() loop → gtk_combo_box_text_new()
    old_menu_create = '''\
        joy_menus[j] = gtk_menu_new();
            for (i = 0; i < num_menu_txt; i++) {
                item = gtk_menu_item_new_with_label(_(menu_txt[i]));
                gtk_object_set_data(GTK_OBJECT(item), "cmd", (gpointer)i);
                gtk_widget_show(item);
                gtk_menu_append(GTK_MENU(joy_menus[j]), item);
            }'''
    new_menu_create = '''\
        joy_menus[j] = gtk_combo_box_text_new();
            for (i = 0; i < num_menu_txt; i++) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(joy_menus[j]), _(menu_txt[i]));
            }'''
    if old_menu_create in content:
        content = content.replace(old_menu_create, new_menu_create)
    else:
        # Try with different whitespace
        content = re.sub(
            r'joy_menus\[j\]\s*=\s*gtk_menu_new\(\)\s*;(\s*)for\s*\(i\s*=\s*0[^{]*\)\s*\{'
            r'([^}]*gtk_menu_item_new_with_label[^}]*gtk_menu_append[^}]*)\}',
            ('joy_menus[j] = gtk_combo_box_text_new();\\1for (i = 0; i < num_menu_txt; i++) {\n'
             '                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(joy_menus[j]), _(menu_txt[i]));\n'
             '            }'),
            content,
            flags=re.DOTALL
        )

    # Fix set_joy_config: gtk_object/gtk_menu_get_active pattern → gtk_combo_box_get_active
    content = re.sub(
        r'\(gint\)g_object_get_data\s*\(G_OBJECT\s*\(gtk_menu_get_active'
        r'\s*\(GTK_MENU\s*\(joy_menus\[(\d+)\]\)\s*\)\s*\)\s*,\s*"cmd"\s*\)',
        r'gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[\1]))',
        content
    )
    # For the loop version: button_cmd[i] extraction
    content = re.sub(
        r'\(gint\)g_object_get_data\s*\(G_OBJECT\s*\(gtk_menu_get_active'
        r'\s*\(GTK_MENU\s*\(joy_menus\[([^\]]+)\]\)\s*\)\s*\)\s*,\s*"cmd"\s*\)',
        r'gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[\1]))',
        content
    )

    # Replace Directionals GtkPacker section (first occurrence, joy_menus[0..3])
    old_dir1 = '''\
        frame = gtk_frame_new(_("Directionals:"));
        gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);
        dir_pack = gtk_packer_new();
        gtk_container_set_border_width(GTK_CONTAINER(dir_pack), 5);
        gtk_container_add(GTK_CONTAINER(frame), dir_pack);
        hist_val[0] = joy_cfg.up;
        hist_val[1] = joy_cfg.down;
        hist_val[2] = joy_cfg.left;
        hist_val[3] = joy_cfg.right;

        for (i = 0; i < 4; i++) {
            blist = gtk_option_menu_new();
            gtk_widget_set_usize(blist, 120, -1);
            gtk_packer_add(GTK_PACKER(dir_pack), blist, pack_pos[i], GTK_ANCHOR_CENTER, 0, 0, 5, 5,
                           0, 0);
            gtk_option_menu_remove_menu(GTK_OPTION_MENU(blist));
            gtk_option_menu_set_menu(GTK_OPTION_MENU(blist), joy_menus[i]);
            gtk_option_menu_set_history(GTK_OPTION_MENU(blist), hist_val[i]);
            gtk_widget_show(blist);
        }

        gtk_widget_show(dir_pack);
        gtk_widget_show(frame);'''
    new_dir1 = '''\
        frame = gtk_frame_new(_("Directionals:"));
        gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);
        dir_pack = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(dir_pack), 5);
        gtk_grid_set_column_spacing(GTK_GRID(dir_pack), 5);
        gtk_container_set_border_width(GTK_CONTAINER(dir_pack), 5);
        gtk_container_add(GTK_CONTAINER(frame), dir_pack);
        hist_val[0] = joy_cfg.up;
        hist_val[1] = joy_cfg.down;
        hist_val[2] = joy_cfg.left;
        hist_val[3] = joy_cfg.right;

        /* GTK3: replaced GtkPacker+GtkOptionMenu with GtkGrid+GtkComboBoxText */
        {
            static const char *_dlbl[] = {N_("Up:"), N_("Down:"), N_("Left:"), N_("Right:")};
            for (i = 0; i < 4; i++) {
                GtkWidget *_l = gtk_label_new(_(_dlbl[i]));
                gtk_widget_set_halign(_l, GTK_ALIGN_END);
                gtk_grid_attach(GTK_GRID(dir_pack), _l, 0, i, 1, 1);
                gtk_widget_show(_l);
                blist = joy_menus[i];
                gtk_widget_set_size_request(blist, 120, -1);
                gtk_grid_attach(GTK_GRID(dir_pack), blist, 1, i, 1, 1);
                gtk_combo_box_set_active(GTK_COMBO_BOX(blist), hist_val[i]);
                gtk_widget_show(blist);
            }
        }

        gtk_widget_show(dir_pack);
        gtk_widget_show(frame);'''
    content = content.replace(old_dir1, new_dir1)

    # Replace Directionals (alternate) GtkPacker section (joy_menus[4..7])
    old_dir2 = '''\
        frame = gtk_frame_new(_("Directionals (alternate):"));
        gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);
        dir_pack = gtk_packer_new();
        gtk_container_set_border_width(GTK_CONTAINER(dir_pack), 5);
        gtk_container_add(GTK_CONTAINER(frame), dir_pack);
        hist_val[0] = joy_cfg.alt_up;
        hist_val[1] = joy_cfg.alt_down;
        hist_val[2] = joy_cfg.alt_left;
        hist_val[3] = joy_cfg.alt_right;

        for (i = 0; i < 4; i++) {
            blist = gtk_option_menu_new();
            gtk_widget_set_usize(blist, 120, -1);
            gtk_packer_add(GTK_PACKER(dir_pack), blist, pack_pos[i], GTK_ANCHOR_CENTER, 0, 0, 5, 5,
                           0, 0);
            gtk_option_menu_remove_menu(GTK_OPTION_MENU(blist));
            gtk_option_menu_set_menu(GTK_OPTION_MENU(blist), joy_menus[4 + i]);
            gtk_option_menu_set_history(GTK_OPTION_MENU(blist), hist_val[i]);
            gtk_widget_show(blist);
        }

        gtk_widget_show(dir_pack);
        gtk_widget_show(frame);'''
    new_dir2 = '''\
        frame = gtk_frame_new(_("Directionals (alternate):"));
        gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);
        dir_pack = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(dir_pack), 5);
        gtk_grid_set_column_spacing(GTK_GRID(dir_pack), 5);
        gtk_container_set_border_width(GTK_CONTAINER(dir_pack), 5);
        gtk_container_add(GTK_CONTAINER(frame), dir_pack);
        hist_val[0] = joy_cfg.alt_up;
        hist_val[1] = joy_cfg.alt_down;
        hist_val[2] = joy_cfg.alt_left;
        hist_val[3] = joy_cfg.alt_right;

        /* GTK3: replaced GtkPacker+GtkOptionMenu with GtkGrid+GtkComboBoxText */
        {
            static const char *_dlbl2[] = {N_("Alt Up:"), N_("Alt Down:"), N_("Alt Left:"), N_("Alt Right:")};
            for (i = 0; i < 4; i++) {
                GtkWidget *_l = gtk_label_new(_(_dlbl2[i]));
                gtk_widget_set_halign(_l, GTK_ALIGN_END);
                gtk_grid_attach(GTK_GRID(dir_pack), _l, 0, i, 1, 1);
                gtk_widget_show(_l);
                blist = joy_menus[4 + i];
                gtk_widget_set_size_request(blist, 120, -1);
                gtk_grid_attach(GTK_GRID(dir_pack), blist, 1, i, 1, 1);
                gtk_combo_box_set_active(GTK_COMBO_BOX(blist), hist_val[i]);
                gtk_widget_show(blist);
            }
        }

        gtk_widget_show(dir_pack);
        gtk_widget_show(frame);'''
    content = content.replace(old_dir2, new_dir2)

    # Replace Buttons section: gtk_option_menu_* with GtkComboBoxText
    old_btn = '''\
            blist = gtk_option_menu_new();
            gtk_widget_set_usize(blist, 120, -1);
            gtk_table_attach_defaults(GTK_TABLE(table), blist, 1, 2, i, i + 1);
            gtk_option_menu_remove_menu(GTK_OPTION_MENU(blist));
            gtk_option_menu_set_menu(GTK_OPTION_MENU(blist), joy_menus[8 + i]);
            gtk_option_menu_set_history(GTK_OPTION_MENU(blist), joy_cfg.button_cmd[i]);
            gtk_widget_show(blist);'''
    new_btn = '''\
            blist = joy_menus[8 + i];
            gtk_widget_set_size_request(blist, 120, -1);
            gtk_table_attach_defaults(GTK_TABLE(table), blist, 1, 2, i, i + 1);
            gtk_combo_box_set_active(GTK_COMBO_BOX(blist), joy_cfg.button_cmd[i]);
            gtk_widget_show(blist);'''
    content = content.replace(old_btn, new_btn)

    # gtk_menu_append → gtk_menu_shell_append (remaining uses)
    content = content.replace('gtk_menu_append(', 'gtk_menu_shell_append(')

    # Remove any remaining gtk_option_menu_* calls
    content = re.sub(r'\s*gtk_option_menu_\w+\s*\([^;]+\)\s*;', '', content)

    return content


def fix_stereo_c(content):
    """Fix Effect/stereo_plugin/stereo.c — adjustment->value address issue."""
    content = apply_common_gtk2(content)
    # Fix callbacks: *(gfloat *)data → (gfloat)gtk_adjustment_get_value(GTK_ADJUSTMENT(data))
    content = content.replace(
        'value = *(gfloat *)data;',
        'value = (gfloat)gtk_adjustment_get_value(GTK_ADJUSTMENT(data));'
    )
    # Fix g_signal_connect calls that pass &gtk_adjustment_get_value(...) — can't take address
    content = re.sub(
        r'&gtk_adjustment_get_value\s*\(GTK_ADJUSTMENT\s*\((\w+)\)\)',
        r'(gpointer)\1',
        content
    )
    # Also handle the GTK_ADJUSTMENT(x)->value pattern (should already be converted)
    content = re.sub(
        r'GTK_ADJUSTMENT\s*\(([^)]+)\)->value\b',
        r'gtk_adjustment_get_value(GTK_ADJUSTMENT(\1))',
        content
    )
    return content


def fix_cdaudio_c(content):
    """Fix Input/cdaudio/cdaudio.c — add XMMS_CDROM_SOLARIS for Linux."""
    # Add Linux define after the DEV_MIXER/XMMS_PAUSE/XMMS_RESUME block
    if 'XMMS_CDROM_SOLARIS' not in content[:500]:
        # Add right after the last include at top of file
        insert = (
            '\n/* GTK3: define platform CDROM implementation for Linux */\n'
            '#if !defined(XMMS_CDROM_SOLARIS) && !defined(XMMS_CDROM_BSD) && defined(__linux__)\n'
            '#    define XMMS_CDROM_SOLARIS 1\n'
            '#endif\n'
        )
        # Insert before the #ifdef CDROMSTOP block
        content = content.replace(
            '\n#ifdef CDROMSTOP',
            insert + '\n#ifdef CDROMSTOP'
        )
    content = apply_common_gtk2(content)
    return content


def fix_cddb_c(content):
    """Fix remaining cddb.c issues — server_clist creation and debug_clist end block."""
    content = apply_common_gtk2(content)

    # Fix remaining server_clist = gtk_clist_new_with_titles (the replacement block failed)
    old_clist_create = (
        '    server_clist = gtk_clist_new_with_titles(4, titles);\n'
        '    g_signal_connect(G_OBJECT(server_clist), "select-row", cddb_server_dialog_select, data);\n'
        '    gtk_box_pack_start(GTK_BOX(vbox), server_clist, TRUE, TRUE, 0);'
    )
    new_clist_create = '''\
    {
        GtkListStore *_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
        const char *_col_names[] = {titles[0], titles[1], titles[2], titles[3]};
        int _ci;
        server_clist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_store));
        g_object_unref(_store);
        for (_ci = 0; _ci < 4; _ci++) {
            GtkCellRenderer *_r = gtk_cell_renderer_text_new();
            GtkTreeViewColumn *_c = gtk_tree_view_column_new_with_attributes(
                _col_names[_ci], _r, "text", _ci, NULL);
            gtk_tree_view_column_set_resizable(_c, TRUE);
            gtk_tree_view_append_column(GTK_TREE_VIEW(server_clist), _c);
        }
        g_signal_connect(G_OBJECT(server_clist), "row-activated",
                         G_CALLBACK(cddb_server_dialog_select), data);
    }
    gtk_box_pack_start(GTK_BOX(vbox), server_clist, TRUE, TRUE, 0);'''
    content = content.replace(old_clist_create, new_clist_create)

    # Fix remaining debug_clist end block (three lines)
    old_debug_end = (
        '    gtk_clist_columns_autosize(GTK_CLIST(debug_clist));\n'
        '    gtk_clist_set_button_actions(GTK_CLIST(debug_clist), 0, GTK_BUTTON_IGNORED);\n'
        '    gtk_clist_moveto(GTK_CLIST(debug_clist), GTK_CLIST(debug_clist)->rows - 1, -1, 0, 0);\n'
        '\n'
        '    cddb_timeout_id = g_timeout_add(500, cddb_update_log_window, NULL);'
    )
    new_debug_end = '    cddb_timeout_id = g_timeout_add(500, cddb_update_log_window, NULL);'
    content = content.replace(old_debug_end, new_debug_end)

    return content


def fix_cdaudio_configure(content):
    """Fix cdaudio/configure.c — incompatible function pointer for toggle_set_sensitive_cb."""
    content = apply_common_gtk2(content)
    return content


# General plugins files patterns
GENERAL_FILES = [
    "General/ir/about.c",
    "General/ir/configure.c",
    "General/joystick/about.c",
    "General/joystick/configure.c",
    "General/song_change/song_change.c",
]

# Files to re-apply common fixes to (vorbis files that still have issues)
REFIX_COMMON = [
    "Input/vorbis/configure.c",
    "Input/vorbis/fileinfo.c",
    "Input/tonegen/tonegen.c",
    "Effect/echo_plugin/gui.c",
]

FILES = {}
for f in GENERAL_FILES:
    if 'joystick/configure' in f:
        FILES[f] = fix_joystick_configure
    else:
        FILES[f] = fix_general_files
for f in REFIX_COMMON:
    FILES[f] = lambda content: wrap_bare_callbacks(apply_common_gtk2(content))

FILES["Effect/stereo_plugin/stereo.c"] = fix_stereo_c
FILES["Input/cdaudio/cdaudio.c"] = fix_cdaudio_c
FILES["Input/cdaudio/cddb.c"] = fix_cddb_c
FILES["Input/cdaudio/configure.c"] = fix_cdaudio_configure

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
