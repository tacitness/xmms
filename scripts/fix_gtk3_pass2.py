#!/usr/bin/env python3
"""
GTK2→GTK3 migration pass 2: fix all remaining plugin and about.c errors.
"""

import re
import sys

# ──────────────────────────────────────────────────────────
# Universal substitution rules (regex list)
# ──────────────────────────────────────────────────────────

SIMPLE_RULES = [
    # GTK signal → GObject signal
    (r'gtk_signal_connect_object\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*GTK_OBJECT\(([^)]+)\)\)',
     r'g_signal_connect_swapped(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)'),
    (r'gtk_signal_connect_object\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*([^)]+)\)',
     r'g_signal_connect_swapped(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)'),
    # gtk_signal_connect with GTK_OBJECT + GTK_SIGNAL_FUNC
    (r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*([^)]+)\)',
     r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)'),
    # gtk_signal_connect without GTK_SIGNAL_FUNC wrapper
    (r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*([a-zA-Z_][a-zA-Z0-9_]*),\s*([^)]+)\)',
     r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)'),
    # Remaining GTK_SIGNAL_FUNC
    (r'\bGTK_SIGNAL_FUNC\s*\(', 'G_CALLBACK('),
    # Remaining GTK_OBJECT in calls
    (r'\bGTK_OBJECT\s*\(', 'G_OBJECT('),
    # Window type
    (r'\bGTK_WINDOW_DIALOG\b', 'GTK_WINDOW_TOPLEVEL'),
    # gtk_window_set_policy → gtk_window_set_resizable (3 bool args → keep 3rd arg as bool)
    (r'gtk_window_set_policy\s*\(([^,]+),\s*[^,]+,\s*([^,]+),\s*[^)]+\)\s*;',
     r'gtk_window_set_resizable(\1, \2);'),
    # Box constructors
    (r'gtk_hbox_new\s*\(([^,]+),\s*([^)]+)\)',
     r'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, \2)'),
    (r'gtk_vbox_new\s*\(([^,]+),\s*([^)]+)\)',
     r'gtk_box_new(GTK_ORIENTATION_VERTICAL, \2)'),
    (r'gtk_hbutton_box_new\s*\(\s*\)',
     'gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)'),
    # gtk_container_border_width → gtk_container_set_border_width
    (r'gtk_container_border_width\s*\(', 'gtk_container_set_border_width('),
    # gtk_box_pack_start_defaults → gtk_box_pack_start with expand/fill
    (r'gtk_box_pack_start_defaults\s*\(GTK_BOX\(([^)]+)\),\s*([^)]+)\)',
     r'gtk_box_pack_start(GTK_BOX(\1), \2, TRUE, TRUE, 0)'),
    # gtk_widget_set_usize → gtk_widget_set_size_request
    (r'\bgtk_widget_set_usize\s*\(', 'gtk_widget_set_size_request('),
    # GTK_WIDGET_SET_FLAGS → gtk_widget_set_can_default
    (r'GTK_WIDGET_SET_FLAGS\s*\(([^,]+),\s*GTK_CAN_DEFAULT\)',
     r'gtk_widget_set_can_default(\1, TRUE)'),
    # GtkObject → GObject (for adjustments)
    (r'\bGtkObject\b\s*\*\s*(\w+_adj\b)', r'GtkAdjustment *\1'),
    # GTK_TOGGLE_BUTTON(x)->active → gtk_toggle_button_get_active
    (r'GTK_TOGGLE_BUTTON\s*\(([^)]+)\)->active',
     r'gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(\1))'),
    # GTK_ADJUSTMENT(x)->value → gtk_adjustment_get_value
    (r'GTK_ADJUSTMENT\s*\(([^)]+)\)->value',
     r'gtk_adjustment_get_value(GTK_ADJUSTMENT(\1))'),
    # widget->window → gtk_widget_get_window(widget)
    (r'\b(\w+)->window\b(?!\s*[=])',
     r'gtk_widget_get_window(\1)'),
    # gtk_timeout_remove → g_source_remove
    (r'\bgtk_timeout_remove\s*\(', 'g_source_remove('),
    # gtk_timeout_add → g_timeout_add
    (r'\bgtk_timeout_add\s*\(', 'g_timeout_add('),
    # gtk_radio_button_group → gtk_radio_button_get_group
    (r'\bgtk_radio_button_group\s*\(', 'gtk_radio_button_get_group('),
    # GDK_Escape → GDK_KEY_Escape
    (r'\bGDK_Escape\b', 'GDK_KEY_Escape'),
    # gtk_entry_new_with_max_length → gtk_entry_new() + gtk_entry_set_max_length
    (r'\bgtk_entry_new_with_max_length\s*\(\s*(\d+)\s*\)',
     r'gtk_entry_new(); gtk_entry_set_max_length(NULL/* set entry ptr */, \1)'),
    # GtkTooltips → just remove usage, gtk_tooltips_new() → no-op
    # GdkPixmap → GdkPixbuf (handled per-file for complex cases)
]


def apply_rules(content, rules=SIMPLE_RULES):
    for pat, repl in rules:
        content = re.sub(pat, repl, content, flags=re.MULTILINE)
    return content


# ──────────────────────────────────────────────────────────
# Per-file fixes
# ──────────────────────────────────────────────────────────

def fix_about_c(content):
    """Complete rewrite of generate_credit_list and show_about_window."""

    # Add on_widget_destroyed wrapper if not present
    if 'on_widget_destroyed' not in content:
        content = content.replace(
            'void show_about_window(void)',
            '''static void on_widget_destroyed(GtkWidget *w, gpointer data)
{
    gtk_widget_destroyed(w, (GtkWidget **)data);
}

void show_about_window(void)'''
        )

    # Replace generate_credit_list entirely
    old_fn_start = 'static GtkWidget *generate_credit_list(const char *text[], gboolean sec_space)'
    new_fn = '''\
static GtkWidget *generate_credit_list(const char *text[], gboolean sec_space)
{
    GtkWidget *textview, *scrollwin;
    GtkTextBuffer *buf;
    GtkTextIter iter;
    int i = 0;

    buf = gtk_text_buffer_new(NULL);
    gtk_text_buffer_get_start_iter(buf, &iter);
    while (text[i]) {
        gchar *line;
        /* section header */
        line = g_strdup_printf("%s\\t%s\\n", gettext(text[i]), gettext(text[i + 1]));
        gtk_text_buffer_insert(buf, &iter, line, -1);
        g_free(line);
        i += 2;
        while (text[i]) {
            line = g_strdup_printf("\\t%s\\n", gettext(text[i++]));
            gtk_text_buffer_insert(buf, &iter, line, -1);
            g_free(line);
        }
        i++;
        if (text[i] && sec_space)
            gtk_text_buffer_insert(buf, &iter, "\\n", -1);
    }
    textview = gtk_text_view_new_with_buffer(buf);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
    g_object_unref(buf);

    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scrollwin), textview);
    gtk_container_set_border_width(GTK_CONTAINER(scrollwin), 10);
    gtk_widget_set_size_request(scrollwin, -1, 120);
    return scrollwin;
}
'''
    # Find and replace the whole old function
    pattern = re.compile(
        r'static GtkWidget \*generate_credit_list\(.*?^}',
        re.DOTALL | re.MULTILINE
    )
    content = pattern.sub(new_fn.rstrip(), content, count=1)

    # Replace GdkPixmap declaration for logo
    content = content.replace(
        'static GdkPixmap *xmms_logo_pmap = NULL, *xmms_logo_mask = NULL;',
        'static GdkPixbuf *xmms_logo_pixbuf = NULL;'
    )

    # Replace logo creation
    content = content.replace(
        'if (!xmms_logo_pmap)\n        xmms_logo_pmap =\n            gdk_pixmap_create_from_xpm_d(about_window->window, &xmms_logo_mask, NULL, xmms_logo);',
        'if (!xmms_logo_pixbuf)\n        xmms_logo_pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)xmms_logo);'
    )

    # Replace gtk_pixmap_new
    content = content.replace(
        'about_credits_logo = gtk_pixmap_new(xmms_logo_pmap, xmms_logo_mask);',
        'about_credits_logo = gtk_image_new_from_pixbuf(xmms_logo_pixbuf);'
    )

    # GTK_WINDOW_DIALOG → GTK_WINDOW_TOPLEVEL
    content = content.replace('GTK_WINDOW_DIALOG', 'GTK_WINDOW_TOPLEVEL')

    # gtk_window_set_policy → gtk_window_set_resizable
    content = re.sub(
        r'gtk_window_set_policy\s*\(GTK_WINDOW\(about_window\),[^;]+;',
        'gtk_window_set_resizable(GTK_WINDOW(about_window), TRUE);',
        content
    )

    # gtk_signal_connect → g_signal_connect with on_widget_destroyed wrapper
    content = content.replace(
        'gtk_signal_connect(GTK_OBJECT(about_window), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed),\n                       &about_window);',
        'g_signal_connect(G_OBJECT(about_window), "destroy", G_CALLBACK(on_widget_destroyed), &about_window);'
    )

    # key_press_event handler (no GTK_SIGNAL_FUNC wrapper needed — bare callback)
    content = content.replace(
        'gtk_signal_connect(GTK_OBJECT(about_window), "key_press_event", util_dialog_keypress_cb, NULL);',
        'g_signal_connect(G_OBJECT(about_window), "key_press_event", G_CALLBACK(util_dialog_keypress_cb), NULL);'
    )

    # gtk_hbox_new homogeneous
    content = re.sub(
        r'about_credits_logo_box = gtk_hbox_new\(TRUE,\s*0\)',
        'about_credits_logo_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0)',
        content
    )
    content = content.replace(
        'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0)',
        '{GtkWidget *_b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_box_set_homogeneous(GTK_BOX(_b), TRUE); about_credits_logo_box = _b;',
        1,  # Only first occurrence (the about_credits_logo_box one)
    )
    # That's getting complex - let me do it differently
    # Reset and do simpler version:
    content = re.sub(
        r'\n    about_credits_logo_box = gtk_hbox_new\(TRUE,\s*0\);\n',
        '\n    about_credits_logo_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);\n    gtk_box_set_homogeneous(GTK_BOX(about_credits_logo_box), TRUE);\n',
        content
    )

    # gtk_vbox_new for about_vbox
    content = re.sub(
        r'about_vbox = gtk_vbox_new\(FALSE,\s*5\)',
        'about_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5)',
        content
    )

    # gtk_hbutton_box_new
    content = content.replace('bbox = gtk_hbutton_box_new();',
                               'bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);')

    # close button signal (gtk_signal_connect_object)
    content = content.replace(
        'gtk_signal_connect_object(GTK_OBJECT(close_btn), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),\n                              GTK_OBJECT(about_window));',
        'g_signal_connect_swapped(G_OBJECT(close_btn), "clicked", G_CALLBACK(gtk_widget_destroy), about_window);'
    )

    # GTK_WIDGET_SET_FLAGS
    content = content.replace(
        'GTK_WIDGET_SET_FLAGS(close_btn, GTK_CAN_DEFAULT);',
        'gtk_widget_set_can_default(close_btn, TRUE);'
    )

    return content


def fix_alsa_about_c(content):
    content = content.replace(
        'gtk_signal_connect(GTK_OBJECT(dialog), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed),\n                       &dialog);',
        'g_signal_connect(G_OBJECT(dialog), "destroy", G_CALLBACK(gtk_widget_destroyed), &dialog);'
    )
    return apply_rules(content)


def fix_alsa_audio_c(content):
    return apply_rules(content)


def fix_alsa_configure_c(content):
    """Comprehensive rewrite for ALSA configure dialog."""

    # --- configure_win_ok_cb: GET_CHARS(GTK_COMBO(x)->entry) → GET_CHARS(gtk_bin_get_child(GTK_BIN(x))) ---
    content = content.replace(
        'alsa_cfg.pcm_device = GET_CHARS(GTK_COMBO(devices_combo)->entry);',
        'alsa_cfg.pcm_device = GET_CHARS(gtk_bin_get_child(GTK_BIN(devices_combo)));'
    )
    content = content.replace(
        'alsa_cfg.mixer_device = GET_CHARS(GTK_COMBO(mixer_devices_combo)->entry);',
        'alsa_cfg.mixer_device = GET_CHARS(gtk_bin_get_child(GTK_BIN(mixer_devices_combo)));'
    )

    # --- get_cards() function: replace with GtkComboBoxText version ---
    old_get_cards = r'''static int get_cards\(GtkOptionMenu \*omenu, GtkSignalFunc cb, int active\)
\{
    GtkWidget \*menu, \*item;
    int card = -1, err, set = 0, curr = -1;

    menu = gtk_menu_new\(\);
    if \(\(err = snd_card_next\(&card\)\) != 0\)
        g_warning\("snd_next_card\(\) failed: %s", snd_strerror\(-err\)\);

    while \(card > -1\) \{
        char \*label;

        curr\+\+;
        if \(card == active\)
            set = curr;
        if \(\(err = snd_card_get_name\(card, &label\)\) != 0\) \{
            g_warning\("snd_carg_get_name\(\) failed: %s", snd_strerror\(-err\)\);
            break;
        \}

        item = gtk_menu_item_new_with_label\(label\);
        gtk_signal_connect\(GTK_OBJECT\(item\), "activate", cb, GINT_TO_POINTER\(card\)\);
        gtk_widget_show\(item\);
        gtk_menu_append\(GTK_MENU\(menu\), item\);
        if \(\(err = snd_card_next\(&card\)\) != 0\) \{
            g_warning\("snd_next_card\(\) failed: %s", snd_strerror\(-err\)\);
            break;
        \}
    \}

    gtk_option_menu_set_menu\(omenu, menu\);
    return set;
\}'''

    new_get_cards = '''\
/* GTK3: card number array for combo index mapping */
static int alsa_mixer_card_list[64];
static int alsa_mixer_card_count = 0;

static int get_cards(GtkComboBox *omenu, GCallback cb, int active)
{
    int card = -1, err, set = 0, curr = -1;

    alsa_mixer_card_count = 0;
    if ((err = snd_card_next(&card)) != 0)
        g_warning("snd_next_card() failed: %s", snd_strerror(-err));

    while (card > -1 && alsa_mixer_card_count < 64) {
        char *label;

        curr++;
        if (card == active)
            set = curr;
        if ((err = snd_card_get_name(card, &label)) != 0) {
            g_warning("snd_carg_get_name() failed: %s", snd_strerror(-err));
            break;
        }

        alsa_mixer_card_list[alsa_mixer_card_count++] = card;
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(omenu), label);

        if ((err = snd_card_next(&card)) != 0) {
            g_warning("snd_next_card() failed: %s", snd_strerror(-err));
            break;
        }
    }

    if (cb)
        g_signal_connect(G_OBJECT(omenu), "changed", cb, NULL);
    return set;
}'''

    content = re.sub(old_get_cards, new_get_cards, content, flags=re.DOTALL)

    # --- mixer_card_cb: use combo index to look up card ---
    content = content.replace(
        '''\
static void mixer_card_cb(GtkWidget *widget, gpointer card)
{
    if (current_mixer_card == GPOINTER_TO_INT(card))
        return;
    current_mixer_card = GPOINTER_TO_INT(card);
    get_mixer_devices(GTK_COMBO(mixer_devices_combo), current_mixer_card);
}''',
        '''\
static void mixer_card_cb(GtkWidget *widget, gpointer data)
{
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    int card = (idx >= 0 && idx < alsa_mixer_card_count) ? alsa_mixer_card_list[idx] : -1;
    if (current_mixer_card == card)
        return;
    current_mixer_card = card;
    get_mixer_devices(GTK_COMBO_BOX_TEXT(mixer_devices_combo), current_mixer_card);
}'''
    )

    # --- get_mixer_devices: GtkCombo * → GtkComboBoxText *, replace gtk_combo_set_popdown_strings ---
    content = content.replace(
        'static int get_mixer_devices(GtkCombo *combo, int card)',
        'static int get_mixer_devices(GtkComboBoxText *combo, int card)'
    )
    content = content.replace(
        '    gtk_combo_set_popdown_strings(combo, items);',
        '''\
    gtk_combo_box_text_remove_all(combo);
    {
        GList *l = items;
        while (l) { gtk_combo_box_text_append_text(combo, (const gchar *)l->data); l = l->next; }
        g_list_free_full(items, g_free);
    }'''
    )

    # --- get_devices_for_card: GtkCombo * → GtkComboBoxText * ---
    content = content.replace(
        'static void get_devices_for_card(GtkCombo *combo, int card)',
        'static void get_devices_for_card(GtkComboBoxText *combo, int card)'
    )
    # Replace gtk_list_item_new_with_label + gtk_combo_set_item_string block
    content = content.replace(
        '''\
        device = g_strdup_printf("hw:%d,%d", card, pcm_device);
        descr =
            g_strconcat(card_name, ": ", snd_pcm_info_get_name(pcm_info), " (", device, ")", NULL);
        item = gtk_list_item_new_with_label(descr);
        gtk_widget_show(item);
        g_free(descr);
        gtk_combo_set_item_string(combo, GTK_ITEM(item), device);
        g_free(device);
        gtk_container_add(GTK_CONTAINER(combo->list), item);''',
        '''\
        device = g_strdup_printf("hw:%d,%d", card, pcm_device);
        descr =
            g_strconcat(card_name, ": ", snd_pcm_info_get_name(pcm_info), " (", device, ")", NULL);
        gtk_combo_box_text_append_text(combo, device);
        g_free(descr);
        g_free(device);'''
    )

    # --- get_devices: GtkCombo * → GtkComboBoxText * ---
    content = content.replace(
        'static void get_devices(GtkCombo *combo)',
        'static void get_devices(GtkComboBoxText *combo)'
    )
    # Replace first gtk_list_item (default PCM device)
    content = content.replace(
        '''\
    descr = g_strdup_printf(_("Default PCM device (%s)"), "default");
    item = gtk_list_item_new_with_label(descr);
    gtk_widget_show(item);
    g_free(descr);
    gtk_combo_set_item_string(combo, GTK_ITEM(item), "default");
    gtk_container_add(GTK_CONTAINER(combo->list), item);''',
        '    gtk_combo_box_text_append_text(combo, "default");'
    )

    # --- alsa_configure(): replace combo creation and option menu creation ---
    content = content.replace(
        '    GtkObject *buffer_time_adj, *period_time_adj, *thread_buffer_time_adj;',
        '    GtkAdjustment *buffer_time_adj, *period_time_adj, *thread_buffer_time_adj;'
    )

    # gdk_window_raise on configure_win (window member access)
    content = content.replace(
        '    if (configure_win) {\n        gdk_window_raise(configure_win->window);\n        return;\n    }',
        '    if (configure_win) {\n        gdk_window_raise(gtk_widget_get_window(configure_win));\n        return;\n    }'
    )

    # GTK_WINDOW_DIALOG
    content = content.replace('GTK_WINDOW_DIALOG', 'GTK_WINDOW_TOPLEVEL')

    # gtk_signal_connect for configure_win destroy
    content = content.replace(
        '    gtk_signal_connect(GTK_OBJECT(configure_win), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed),\n                       &configure_win);',
        '    g_signal_connect(G_OBJECT(configure_win), "destroy", G_CALLBACK(gtk_widget_destroyed), &configure_win);'
    )

    # gtk_window_set_policy
    content = re.sub(
        r'gtk_window_set_policy\s*\(GTK_WINDOW\(configure_win\),[^;]+;',
        'gtk_window_set_resizable(GTK_WINDOW(configure_win), TRUE);',
        content
    )

    # gtk_container_border_width → gtk_container_set_border_width
    content = content.replace('gtk_container_border_width(', 'gtk_container_set_border_width(')

    # box constructors
    content = re.sub(r'gtk_vbox_new\(([^,]+),\s*(\d+)\)',
                     r'gtk_box_new(GTK_ORIENTATION_VERTICAL, \2)', content)
    content = re.sub(r'gtk_hbox_new\(([^,]+),\s*(\d+)\)',
                     r'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, \2)', content)
    content = content.replace('gtk_hbutton_box_new()',
                               'gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)')

    # gtk_box_pack_start_defaults
    content = re.sub(
        r'gtk_box_pack_start_defaults\s*\(GTK_BOX\(([^)]+)\),\s*([^)]+)\)\s*;',
        r'gtk_box_pack_start(GTK_BOX(\1), \2, TRUE, TRUE, 0);',
        content
    )

    # gtk_widget_set_usize
    content = content.replace('gtk_widget_set_usize(', 'gtk_widget_set_size_request(')

    # Replace gtk_combo_new() → gtk_combo_box_text_new_with_entry()
    content = content.replace('devices_combo = gtk_combo_new();',
                               'devices_combo = gtk_combo_box_text_new_with_entry();')
    content = content.replace('mixer_devices_combo = gtk_combo_new();',
                               'mixer_devices_combo = gtk_combo_box_text_new_with_entry();')

    # get_devices() calls: GTK_COMBO(x) → GTK_COMBO_BOX_TEXT(x)
    content = content.replace('get_devices(GTK_COMBO(devices_combo));',
                               'get_devices(GTK_COMBO_BOX_TEXT(devices_combo));')
    content = content.replace(
        'gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(devices_combo)->entry), alsa_cfg.pcm_device);',
        'gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(devices_combo))), alsa_cfg.pcm_device);'
    )

    # mixer_card_om: Replace GtkOptionMenu with GtkComboBoxText
    content = content.replace('mixer_card_om = gtk_option_menu_new();',
                               'mixer_card_om = GTK_WIDGET(gtk_combo_box_text_new());')
    content = content.replace(
        '    mset = get_cards(GTK_OPTION_MENU(mixer_card_om), mixer_card_cb, alsa_cfg.mixer_card);',
        '    mset = get_cards(GTK_COMBO_BOX(mixer_card_om), G_CALLBACK(mixer_card_cb), alsa_cfg.mixer_card);'
    )
    content = content.replace(
        '    gtk_option_menu_set_history(GTK_OPTION_MENU(mixer_card_om), mset);',
        '    gtk_combo_box_set_active(GTK_COMBO_BOX(mixer_card_om), mset);'
    )
    content = content.replace(
        '    get_mixer_devices(GTK_COMBO(mixer_devices_combo), alsa_cfg.mixer_card);',
        '    get_mixer_devices(GTK_COMBO_BOX_TEXT(mixer_devices_combo), alsa_cfg.mixer_card);'
    )
    content = content.replace(
        '    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(mixer_devices_combo)->entry), alsa_cfg.mixer_device);',
        '    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(mixer_devices_combo))), alsa_cfg.mixer_device);'
    )

    # softvolume_toggle_cb: gtk_signal_connect
    content = content.replace(
        '    gtk_signal_connect(GTK_OBJECT(softvolume_toggle_button), "toggled", softvolume_toggle_cb,\n                       mixer_card_om);',
        '    g_signal_connect(G_OBJECT(softvolume_toggle_button), "toggled", G_CALLBACK(softvolume_toggle_cb), mixer_card_om);'
    )

    # GTK_WIDGET_SET_FLAGS
    content = re.sub(
        r'GTK_WIDGET_SET_FLAGS\s*\(([^,]+),\s*GTK_CAN_DEFAULT\)',
        r'gtk_widget_set_can_default(\1, TRUE)',
        content
    )

    # ok/cancel button signals
    content = content.replace(
        '    gtk_signal_connect(GTK_OBJECT(ok), "clicked", configure_win_ok_cb, NULL);',
        '    g_signal_connect(G_OBJECT(ok), "clicked", G_CALLBACK(configure_win_ok_cb), NULL);'
    )
    content = content.replace(
        '    gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked", gtk_widget_destroy,\n                              GTK_OBJECT(configure_win));',
        '    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy), configure_win);'
    )

    return content


def fix_disk_writer_c(content):
    content = apply_rules(content)

    # GtkTooltips → replaced with gtk_widget_set_tooltip_text
    # The pattern: GtkTooltips *use_suffix_tooltips; → remove
    # gtk_tooltips_new() → nothing
    # gtk_tooltips_set_tip(tooltips, widget, text, NULL) → gtk_widget_set_tooltip_text(widget, text)
    # gtk_tooltips_enable(tooltips) → nothing
    content = content.replace('    GtkTooltips *use_suffix_tooltips;\n', '')
    content = content.replace(
        '        use_suffix_tooltips = gtk_tooltips_new();\n',
        ''
    )
    content = content.replace(
        '        gtk_tooltips_set_tip(use_suffix_tooltips, use_suffix_toggle,\n                             "If enabled, the extension from the original filename will not be "\n                             "stripped before adding the .wav extension to the end.",\n                             NULL);\n',
        '        gtk_widget_set_tooltip_text(use_suffix_toggle,\n                             "If enabled, the extension from the original filename will not be "\n                             "stripped before adding the .wav extension to the end.");\n'
    )
    content = content.replace('        gtk_tooltips_enable(use_suffix_tooltips);\n', '')

    return content


def fix_mpg123_configure_c(content):
    """Fix mpg123 configure.c."""
    # GtkObject * for adjustments
    content = re.sub(r'\bGtkObject\s*\*\s*(?=\w+_adj\b)', 'GtkAdjustment *', content)

    # GTK_TOGGLE_BUTTON(x)->active
    content = re.sub(
        r'GTK_TOGGLE_BUTTON\s*\(([^)]+)\)->active',
        r'gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(\1))',
        content
    )

    # GTK_ADJUSTMENT(x)->value
    content = re.sub(
        r'GTK_ADJUSTMENT\s*\(([^)]+)\)->value',
        r'gtk_adjustment_get_value(GTK_ADJUSTMENT(\1))',
        content
    )

    # mpg123_configurewin->window
    content = content.replace(
        '        gdk_window_raise(mpg123_configurewin->window);',
        '        gdk_window_raise(gtk_widget_get_window(mpg123_configurewin));'
    )

    # GTK_WINDOW_DIALOG
    content = content.replace('GTK_WINDOW_DIALOG', 'GTK_WINDOW_TOPLEVEL')

    # gtk_window_set_policy
    content = re.sub(
        r'gtk_window_set_policy\s*\([^;]+;',
        '/* TODO(#gtk3): gtk_window_set_policy removed */',
        content
    )

    # gtk_container_border_width
    content = content.replace('gtk_container_border_width(', 'gtk_container_set_border_width(')

    # gtk_signal_connect with GTK_SIGNAL_FUNC and GTK_OBJECT
    content = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*([^)]+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        content
    )
    # gtk_signal_connect without GTK_SIGNAL_FUNC
    content = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*([a-zA-Z_][a-zA-Z0-9_]*),\s*([^)]+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        content
    )
    # gtk_signal_connect_object
    content = re.sub(
        r'gtk_signal_connect_object\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*GTK_OBJECT\(([^)]+)\)\)',
        r'g_signal_connect_swapped(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        content
    )

    # gtk_signal_connect for widget_destroyed
    content = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\((\w+)\),\s*"destroy",\s*GTK_SIGNAL_FUNC\(gtk_widget_destroyed\),\s*&(\w+)\)',
        r'g_signal_connect(G_OBJECT(\1), "destroy", G_CALLBACK(gtk_widget_destroyed), &\2)',
        content
    )

    # GTK_WIDGET_SET_FLAGS
    content = re.sub(
        r'GTK_WIDGET_SET_FLAGS\s*\(([^,]+),\s*GTK_CAN_DEFAULT\)',
        r'gtk_widget_set_can_default(\1, TRUE)',
        content
    )

    # gtk_radio_button_group
    content = content.replace('gtk_radio_button_group(', 'gtk_radio_button_get_group(')

    # box constructors
    content = re.sub(r'gtk_vbox_new\(([^,]+),\s*(\d+)\)',
                     r'gtk_box_new(GTK_ORIENTATION_VERTICAL, \2)', content)
    content = re.sub(r'gtk_hbox_new\(([^,]+),\s*(\d+)\)',
                     r'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, \2)', content)
    content = content.replace('gtk_hbutton_box_new()',
                               'gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)')

    # gtk_widget_set_usize
    content = content.replace('gtk_widget_set_usize(', 'gtk_widget_set_size_request(')

    # gtk_box_pack_start_defaults
    content = re.sub(
        r'gtk_box_pack_start_defaults\s*\(GTK_BOX\(([^)]+)\),\s*([^)]+)\)\s*;',
        r'gtk_box_pack_start(GTK_BOX(\1), \2, TRUE, TRUE, 0);',
        content
    )

    # streaming_cast_vbox creation: gtk_vbox_new(5, FALSE) — args swapped
    content = content.replace(
        '    streaming_cast_vbox = gtk_vbox_new(5, FALSE);',
        '    streaming_cast_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);'
    )

    return content


def fix_mpg123_fileinfo_c(content):
    """Fix mpg123 fileinfo.c - complex GtkList/GtkCombo migration."""

    # genre_selected: GtkList → GtkComboBox
    content = content.replace(
        'static void genre_selected(GtkList *list, GtkWidget *w, gpointer data)\n{\n    void *p;\n    p = gtk_object_get_data(GTK_OBJECT(w), "genre_id");\n    if (p != NULL)\n        current_genre = GPOINTER_TO_INT(p);\n    else\n        current_genre = 0;\n}',
        '''\
/* GTK3: genre ID lookup array (populated by genre_set_popdown) */
static int *genre_id_array = NULL;
static int genre_id_count = 0;

static void genre_selected(GtkWidget *combo, gpointer data)
{
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (idx >= 0 && idx < genre_id_count && genre_id_array)
        current_genre = genre_id_array[idx];
    else
        current_genre = 0;
}'''
    )

    # genre_set_popdown: replace GtkList/GtkCombo usage
    content = content.replace(
        '''\
static void genre_set_popdown(GtkWidget *combo, GList *genres)
{
    struct genre_item *item;
    GtkWidget *w;

    while (genres) {
        item = genres->data;
        w = gtk_list_item_new_with_label(item->name);
        gtk_object_set_data(GTK_OBJECT(w), "genre_id", GINT_TO_POINTER(item->id));
        gtk_widget_show(w);
        gtk_container_add(GTK_CONTAINER(GTK_COMBO(combo)->list), w);
        genres = genres->next;
    }
}''',
        '''\
static void genre_set_popdown(GtkWidget *combo, GList *genres)
{
    struct genre_item *item;
    int count;
    int i = 0;

    count = (int)g_list_length(genres);
    g_free(genre_id_array);
    genre_id_array = g_new0(int, count + 1);
    genre_id_count = 0;

    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));

    while (genres) {
        item = genres->data;
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), item->name);
        genre_id_array[i++] = item->id;
        genre_id_count++;
        genres = genres->next;
    }
}'''
    )

    # file_info_box_keypress_cb: GDK_Escape → GDK_KEY_Escape
    content = content.replace('GDK_Escape', 'GDK_KEY_Escape')

    # Rename combo from gtk_combo_new
    # First: find where genre combo is created - it's likely created somewhere as gtk_combo_new()
    # We need to replace that with gtk_combo_box_text_new()
    # And connect "changed" signal for genre selection

    # The window creation block fixes:
    content = content.replace('GTK_WINDOW_DIALOG', 'GTK_WINDOW_TOPLEVEL')

    # gtk_window_set_policy
    content = re.sub(
        r'gtk_window_set_policy\s*\([^;]+;',
        '/* TODO(#gtk3): gtk_window_set_policy removed */',
        content
    )

    # gtk_signal_connect with GTK_OBJECT/no GTK_SIGNAL_FUNC (destroy, key_press_event)
    content = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*("destroy"),\s*gtk_widget_destroyed,\s*&(\w+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(gtk_widget_destroyed), &\3)',
        content
    )
    content = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*("key_press_event"),\s*([a-zA-Z_]+),\s*([^)]+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        content
    )

    # gtk_entry_new_with_max_length: three occurrences for title, artist, album entries
    # We need to set max_length separately
    # Replace the three occurrences:
    content = re.sub(
        r'(\w+_entry) = gtk_entry_new_with_max_length\((\d+)\);',
        r'\1 = gtk_entry_new();\n        gtk_entry_set_max_length(GTK_ENTRY(\1), \2);',
        content
    )

    # box constructors
    content = re.sub(r'gtk_vbox_new\(([^,]+),\s*(\d+)\)',
                     r'gtk_box_new(GTK_ORIENTATION_VERTICAL, \2)', content)
    content = re.sub(r'gtk_hbox_new\(([^,]+),\s*(\d+)\)',
                     r'gtk_box_new(GTK_ORIENTATION_HORIZONTAL, \2)', content)
    content = content.replace('gtk_hbutton_box_new()',
                               'gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)')

    # gtk_widget_set_usize
    content = content.replace('gtk_widget_set_usize(', 'gtk_widget_set_size_request(')

    # GTK_WIDGET_SET_FLAGS
    content = re.sub(
        r'GTK_WIDGET_SET_FLAGS\s*\(([^,]+),\s*GTK_CAN_DEFAULT\)',
        r'gtk_widget_set_can_default(\1, TRUE)',
        content
    )

    # gtk_signal_connect_object for buttons
    content = re.sub(
        r'gtk_signal_connect_object\s*\(GTK_OBJECT\(([^)]+)\),\s*("clicked"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*GTK_OBJECT\(([^)]+)\)\)',
        r'g_signal_connect_swapped(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        content
    )

    # Remaining gtk_signal_connect with GTK_SIGNAL_FUNC
    content = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*([^)]+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        content
    )
    content = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*([a-zA-Z_][a-zA-Z0-9_]*),\s*([^)]+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        content
    )

    # GTK_SIGNAL_FUNC remaining
    content = content.replace('GTK_SIGNAL_FUNC(', 'G_CALLBACK(')

    # gtk_box_pack_start_defaults
    content = re.sub(
        r'gtk_box_pack_start_defaults\s*\(GTK_BOX\(([^)]+)\),\s*([^)]+)\)\s*;',
        r'gtk_box_pack_start(GTK_BOX(\1), \2, TRUE, TRUE, 0);',
        content
    )

    # genre combo: replace gtk_combo_new() with gtk_combo_box_text_new()
    # and connect "changed" signal instead of list "select_child"
    content = content.replace(
        '    genre_combo = gtk_combo_new();',
        '    genre_combo = gtk_combo_box_text_new();'
    )
    # Remove old list signal connecton (select_child → we use "changed" on combo itself)
    content = re.sub(
        r'\s*gtk_signal_connect\s*\(GTK_OBJECT\(GTK_COMBO\(genre_combo\)->list\),\s*"select_child"[^;]+;',
        '\n    g_signal_connect(G_OBJECT(genre_combo), "changed", G_CALLBACK(genre_selected), NULL);',
        content
    )

    return content


def fix_mpg123_c(content):
    """Fix single gtk_signal_connect in mpg123.c."""
    content = re.sub(
        r'gtk_signal_connect\s*\(GTK_OBJECT\(([^)]+)\),\s*(".*?"),\s*GTK_SIGNAL_FUNC\(([^)]+)\),\s*([^)]+)\)',
        r'g_signal_connect(G_OBJECT(\1), \2, G_CALLBACK(\3), \4)',
        content
    )
    return content


def fix_mpg123_h(content):
    """Add #ifndef guards around extern decls that conflict with getbits.h macros."""
    content = content.replace(
        'extern unsigned int mpg123_get1bit(void);',
        '#ifndef mpg123_get1bit\nextern unsigned int mpg123_get1bit(void);\n#endif'
    )
    content = content.replace(
        'extern unsigned int mpg123_getbits(int);',
        '#ifndef mpg123_getbits\nextern unsigned int mpg123_getbits(int);\n#endif'
    )
    content = content.replace(
        'extern unsigned int mpg123_getbits_fast(int);',
        '#ifndef mpg123_getbits_fast\nextern unsigned int mpg123_getbits_fast(int);\n#endif'
    )
    return content


def fix_id3_h(content):
    """Add #include <stdio.h> for FILE type."""
    if '#include <stdio.h>' not in content:
        content = content.replace(
            '#include <glib.h>',
            '#include <stdio.h>\n#include <glib.h>'
        )
    return content


def fix_layer1_c(content):
    """Fix rval type to match getbits.c (unsigned int, not unsigned long)."""
    content = content.replace(
        '/* Used by the getbits macros */\nstatic unsigned long rval;',
        '/* Used by the getbits macros */\nstatic unsigned int rval;'
    )
    return content


# ──────────────────────────────────────────────────────────
# Main dispatcher
# ──────────────────────────────────────────────────────────

FIXERS = {
    'xmms/about.c':                          fix_about_c,
    'Output/alsa/about.c':                   fix_alsa_about_c,
    'Output/alsa/audio.c':                   fix_alsa_audio_c,
    'Output/alsa/configure.c':               fix_alsa_configure_c,
    'Output/disk_writer/disk_writer.c':      fix_disk_writer_c,
    'Input/mpg123/configure.c':              fix_mpg123_configure_c,
    'Input/mpg123/fileinfo.c':               fix_mpg123_fileinfo_c,
    'Input/mpg123/mpg123.c':                 fix_mpg123_c,
    'Input/mpg123/mpg123.h':                 fix_mpg123_h,
    'Input/mpg123/id3.h':                    fix_id3_h,
    'Input/mpg123/layer1.c':                 fix_layer1_c,
}


import os

root = '/data/src/dev-env/xmms'

for rel_path, fixer in FIXERS.items():
    fpath = os.path.join(root, rel_path)
    try:
        with open(fpath, 'r') as f:
            original = f.read()
        fixed = fixer(original)
        if fixed != original:
            with open(fpath, 'w') as f:
                f.write(fixed)
            print(f'FIXED: {rel_path}')
        else:
            print(f'UNCHANGED: {rel_path}')
    except Exception as e:
        print(f'ERROR processing {rel_path}: {e}', file=sys.stderr)
        import traceback; traceback.print_exc()
