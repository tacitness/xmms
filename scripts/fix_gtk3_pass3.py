#!/usr/bin/env python3
"""GTK3 pass 3: Fix OSS files and remaining issues."""

import re
import sys
import os

root = '/data/src/dev-env/xmms'


def fix_file(path, func):
    with open(path) as f:
        orig = f.read()
    new = func(orig)
    if new != orig:
        with open(path, 'w') as f:
            f.write(new)
        print(f'FIXED: {os.path.relpath(path, root)}')
    else:
        print(f'UNCHANGED: {os.path.relpath(path, root)}')


def gtk2_to_gtk3_common(c):
    """Apply common GTK2→GTK3 patterns."""
    # gtk_signal_connect with GTK_SIGNAL_FUNC
    c = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*([^;)]+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        c
    )
    # gtk_signal_connect without GTK_SIGNAL_FUNC (bare func ptr)
    c = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*([A-Za-z_][A-Za-z0-9_]*),\s*([^;)]+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        c
    )
    # gtk_signal_connect_object
    c = re.sub(
        r'gtk_signal_connect_object\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*GTK_OBJECT\(([^)]+)\)\)',
        r'g_signal_connect_swapped(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        c
    )
    c = re.sub(
        r'gtk_signal_connect_object\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*([A-Za-z_][A-Za-z0-9_]*),\s*GTK_OBJECT\(([^)]+)\)\)',
        r'g_signal_connect_swapped(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        c
    )
    c = re.sub(
        r'gtk_signal_connect_object\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*([A-Za-z_][A-Za-z0-9_]*),\s*([^;)]+)\)',
        r'g_signal_connect_swapped(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        c
    )
    # Remaining GTK_SIGNAL_FUNC / GTK_OBJECT
    c = re.sub(r'\bGTK_SIGNAL_FUNC\s*\(', 'G_CALLBACK(', c)
    c = re.sub(r'\bGTK_OBJECT\s*\(', 'G_OBJECT(', c)
    # GTK_WINDOW_DIALOG
    c = c.replace('GTK_WINDOW_DIALOG', 'GTK_WINDOW_TOPLEVEL')
    # gtk_window_set_policy
    c = re.sub(r'gtk_window_set_policy\s*\([^;]+;',
               '/* TODO(#gtk3): gtk_window_set_policy removed */', c)
    # gtk_container_border_width
    c = c.replace('gtk_container_border_width(', 'gtk_container_set_border_width(')
    # box constructors
    c = re.sub(r'gtk_vbox_new\s*\([^,]+,\s*(\d+)\)',
               r'gtk_box_new(GTK_ORIENTATION_VERTICAL, \1)', c)
    c = re.sub(r'gtk_hbox_new\s*\([^,]+,\s*(\d+)\)',
               r'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, \1)', c)
    c = re.sub(r'gtk_hbutton_box_new\s*\(\s*\)',
               'gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)', c)
    # gtk_box_pack_start_defaults
    c = re.sub(
        r'gtk_box_pack_start_defaults\s*\(GTK_BOX\(([^)]+)\),\s*([^)]+)\)\s*;',
        r'gtk_box_pack_start(GTK_BOX(\1), \2, TRUE, TRUE, 0);', c
    )
    # gtk_widget_set_usize
    c = c.replace('gtk_widget_set_usize(', 'gtk_widget_set_size_request(')
    # GTK_WIDGET_SET_FLAGS
    c = re.sub(r'GTK_WIDGET_SET_FLAGS\s*\(([^,]+),\s*GTK_CAN_DEFAULT\)',
               r'gtk_widget_set_can_default(\1, TRUE)', c)
    # GtkObject * for adjustments → GtkAdjustment *
    c = re.sub(r'\bGtkObject\s*\*\s*(\w+_adj\b)', r'GtkAdjustment *\1', c)
    # GTK_TOGGLE_BUTTON->active
    c = re.sub(r'GTK_TOGGLE_BUTTON\s*\(([^)]+)\)->active',
               r'gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(\1))', c)
    # GTK_ADJUSTMENT->value
    c = re.sub(r'GTK_ADJUSTMENT\s*\(([^)]+)\)->value',
               r'gtk_adjustment_get_value(GTK_ADJUSTMENT(\1))', c)
    # widget->window (not assignment)
    c = re.sub(r'\b(\w+)->window\b(?!\s*[=])', r'gtk_widget_get_window(\1)', c)
    # gtk_radio_button_group
    c = c.replace('gtk_radio_button_group(', 'gtk_radio_button_get_group(')
    # gtk_button_box_set_spacing → gtk_box_set_spacing
    c = re.sub(r'gtk_button_box_set_spacing\s*\(GTK_BUTTON_BOX\(([^)]+)\),\s*',
               r'gtk_box_set_spacing(GTK_BOX(\1), ', c)
    # GtkSignalFunc → GCallback
    c = c.replace('GtkSignalFunc', 'GCallback')
    # GtkObject */ GtkObject  (type name)
    c = re.sub(r'\bGtkObject\b\s*\*', 'GObject *', c)
    c = re.sub(r'\bGtkObject\b', 'GObject', c)
    return c


def oss_add_dev_defines(c):
    """Add DEV_DSP / DEV_MIXER fallback defines to OSS.h"""
    insert_after = '#define IS_BIG_ENDIAN FALSE\n#endif'
    addition = '''
/* GTK3: fallback device name constants if not defined by soundcard.h */
#ifndef DEV_DSP
#    define DEV_DSP "/dev/dsp"
#endif
#ifndef DEV_MIXER
#    define DEV_MIXER "/dev/mixer"
#endif
'''
    return c.replace(insert_after, insert_after + addition, 1)


def oss_fix_configure(c):
    """Fix OSS configure.c."""
    # scan_devices: replace GtkOptionMenu + menu approach with GtkComboBoxText
    c = c.replace(
        'static void scan_devices(gchar *type, GtkWidget *option_menu, GtkSignalFunc sigfunc)',
        'static void scan_devices(gchar *type, GtkWidget *combo_menu, GCallback sigfunc)'
    )
    # Inside scan_devices: replace GTK_MENU(menu) append and option_menu_set_menu
    c = c.replace(
        '''\
    menu = gtk_menu_new();

    if ((file = fopen("/dev/sndstat", "r"))) {
        while (fgets(buffer, 255, file)) {
            if (found && buffer[0] == '\\n')
                break;
            if (buffer[strlen(buffer) - 1] == '\\n')
                buffer[strlen(buffer) - 1] = '\\0';
            if (found) {
                if (index == 0) {
                    tmp2 = strchr(buffer, ':');
                    if (tmp2) {
                        tmp2++;
                        while (*tmp2 == ' ')
                            tmp2++;
                    } else
                        tmp2 = buffer;
                    temp = g_strdup_printf(_("Default (%s)"), tmp2);
                    item = gtk_menu_item_new_with_label(temp);
                    g_free(temp);
                } else
                    item = gtk_menu_item_new_with_label(buffer);
                gtk_signal_connect(GTK_OBJECT(item), "activate", sigfunc, (gpointer)index++);
                gtk_widget_show(item);
                gtk_menu_append(GTK_MENU(menu), item);
            }
            if (!strcasecmp(buffer, type))
                found = 1;
        }
        fclose(file);
    } else {
        item = gtk_menu_item_new_with_label(_("Default"));
        gtk_signal_connect(GTK_OBJECT(item), "activate", sigfunc, (gpointer)0);
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
    }
    gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);''',
        '''\
    /* GTK3: replace GtkOptionMenu/menu approach with GtkComboBoxText */
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo_menu));

    if ((file = fopen("/dev/sndstat", "r"))) {
        while (fgets(buffer, 255, file)) {
            if (found && buffer[0] == '\\n')
                break;
            if (buffer[strlen(buffer) - 1] == '\\n')
                buffer[strlen(buffer) - 1] = '\\0';
            if (found) {
                if (index == 0) {
                    tmp2 = strchr(buffer, ':');
                    if (tmp2) {
                        tmp2++;
                        while (*tmp2 == ' ')
                            tmp2++;
                    } else
                        tmp2 = buffer;
                    temp = g_strdup_printf(_("Default (%s)"), tmp2);
                    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_menu), temp);
                    g_free(temp);
                } else {
                    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_menu), buffer);
                }
                index++;
            }
            if (!strcasecmp(buffer, type))
                found = 1;
        }
        fclose(file);
    } else {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_menu), _("Default"));
    }
    if (sigfunc)
        g_signal_connect(G_OBJECT(combo_menu), "changed", sigfunc, NULL);'''
    )

    # Remove now-unused GtkWidget *menu, *item local variable declarations in scan_devices
    c = c.replace(
        '    GtkWidget *menu, *item;\n',
        ''
    )

    # oss_configure function: replace gtk_option_menu_new with gtk_combo_box_text_new
    # adevice and mdevice
    c = c.replace('    adevice = gtk_option_menu_new();',
                  '    adevice = gtk_combo_box_text_new();')
    c = c.replace('    mdevice = gtk_option_menu_new();',
                  '    mdevice = gtk_combo_box_text_new();')

    # scan_devices calls: add G_CALLBACK wrapper
    c = c.replace(
        '    scan_devices("Installed devices:", adevice, configure_win_audio_dev_cb);\n#else\n    scan_devices("Audio devices:", adevice, configure_win_audio_dev_cb);',
        '    scan_devices("Installed devices:", adevice, G_CALLBACK(configure_win_audio_dev_cb));\n#else\n    scan_devices("Audio devices:", adevice, G_CALLBACK(configure_win_audio_dev_cb));'
    )
    c = c.replace(
        '    scan_devices("Installed devices:", mdevice, configure_win_mixer_dev_cb);\n#else\n    scan_devices("Mixers:", mdevice, configure_win_mixer_dev_cb);',
        '    scan_devices("Installed devices:", mdevice, G_CALLBACK(configure_win_mixer_dev_cb));\n#else\n    scan_devices("Mixers:", mdevice, G_CALLBACK(configure_win_mixer_dev_cb));'
    )
    # Scan_devices "HAVE_NEWPCM" not present case
    c = c.replace(
        '    scan_devices("Audio devices:", adevice, configure_win_audio_dev_cb);',
        '    scan_devices("Audio devices:", adevice, G_CALLBACK(configure_win_audio_dev_cb));'
    )
    c = c.replace(
        '    scan_devices("Mixers:", mdevice, configure_win_mixer_dev_cb);',
        '    scan_devices("Mixers:", mdevice, G_CALLBACK(configure_win_mixer_dev_cb));'
    )

    # gtk_option_menu_set_history → gtk_combo_box_set_active
    c = c.replace(
        '    gtk_option_menu_set_history(GTK_OPTION_MENU(adevice), oss_cfg.audio_device);',
        '    gtk_combo_box_set_active(GTK_COMBO_BOX(adevice), oss_cfg.audio_device);'
    )
    c = c.replace(
        '    gtk_option_menu_set_history(GTK_OPTION_MENU(mdevice), oss_cfg.mixer_device);',
        '    gtk_combo_box_set_active(GTK_COMBO_BOX(mdevice), oss_cfg.mixer_device);'
    )

    # adevice_use_alt_check signal connect
    c = c.replace(
        '    gtk_signal_connect(GTK_OBJECT(adevice_use_alt_check), "toggled", audio_device_toggled, adevice);',
        '    g_signal_connect(G_OBJECT(adevice_use_alt_check), "toggled", G_CALLBACK(audio_device_toggled), adevice);'
    )
    c = c.replace(
        '    gtk_signal_connect(GTK_OBJECT(mdevice_use_alt_check), "toggled", mixer_device_toggled, mdevice);',
        '    g_signal_connect(G_OBJECT(mdevice_use_alt_check), "toggled", G_CALLBACK(mixer_device_toggled), mdevice);'
    )

    # configure_win->window raise
    c = c.replace(
        '    if (configure_win) {\n        gdk_window_raise(configure_win->window);\n        return;\n    }',
        '    if (configure_win) {\n        gdk_window_raise(gtk_widget_get_window(configure_win));\n        return;\n    }'
    )

    # configure_win signal connect
    c = c.replace(
        '    gtk_signal_connect(GTK_OBJECT(configure_win), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed),\n                       &configure_win);',
        '    g_signal_connect(G_OBJECT(configure_win), "destroy", G_CALLBACK(gtk_widget_destroyed), &configure_win);'
    )

    # ok/cancel buttons
    c = c.replace(
        '    gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(configure_win_ok_cb), NULL);',
        '    g_signal_connect(G_OBJECT(ok), "clicked", G_CALLBACK(configure_win_ok_cb), NULL);'
    )
    c = re.sub(
        r'gtk_signal_connect_object\s*\(GTK_OBJECT\(cancel\),\s*"clicked",\s*GTK_SIGNAL_FUNC\(gtk_widget_destroy\),\s*GTK_OBJECT\(configure_win\)\)',
        'g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy), configure_win)',
        c
    )

    # Apply remaining common patterns
    c = gtk2_to_gtk3_common(c)

    return c


def oss_fix_audio_c(c):
    """audio.c just needs DEV_DSP (handled via OSS.h define), no GTK changes."""
    return c  # DEV_DSP fix is in OSS.h


def oss_fix_mixer_c(c):
    """mixer.c just needs DEV_MIXER (handled via OSS.h define)."""
    return c  # DEV_MIXER fix is in OSS.h


def oss_fix_about_c(c):
    c = c.replace(
        '    gtk_signal_connect(GTK_OBJECT(dialog), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed),\n                       &dialog);',
        '    g_signal_connect(G_OBJECT(dialog), "destroy", G_CALLBACK(gtk_widget_destroyed), &dialog);'
    )
    return c


# Run all OSS fixes
fix_file(os.path.join(root, 'Output/OSS/OSS.h'), oss_add_dev_defines)
fix_file(os.path.join(root, 'Output/OSS/about.c'), oss_fix_about_c)
fix_file(os.path.join(root, 'Output/OSS/configure.c'), oss_fix_configure)
# audio.c and mixer.c get DEV_DSP/DEV_MIXER from OSS.h - no changes needed

# Also apply gtk_button_box_set_spacing globally with a Python sweep
# (already done with sed, verify)
import glob
for fpath in glob.glob(os.path.join(root, '**/*.c'), recursive=True):
    with open(fpath) as f:
        c = f.read()
    new = re.sub(
        r'gtk_button_box_set_spacing\s*\(GTK_BUTTON_BOX\(([^)]+)\),\s*',
        r'gtk_box_set_spacing(GTK_BOX(\1), ',
        c
    )
    if new != c:
        with open(fpath, 'w') as f:
            f.write(new)
        print(f'gtk_button_box_set_spacing fixed: {os.path.relpath(fpath, root)}')
