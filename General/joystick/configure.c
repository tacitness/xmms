
/*  Joystick plugin for xmms by Tim Ferguson (timf@dgs.monash.edu.au
 *                                  http://www.dgs.monash.edu.au/~timf/) ...
 *  14/12/2000 - patched to allow 5 or more buttons to be used (Justin Wake
 * <justin@globalsoft.com.au>) XMMS is Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas,
 * Thomas Nilsson and 4Front Technologies
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
 */
#include "joy.h"
#include "xmms/i18n.h"

static GtkWidget *joyconf_mainwin;
static GtkWidget *dev_entry1, *dev_entry2, *sens_entry, **joy_menus;

#define num_menu_txt 14
static char *menu_txt[] = {N_("Play/Pause"), N_("Stop"),         N_("Next Track"),
                           N_("Prev Track"), N_("Fwd 5 tracks"), N_("Back 5 tracks"),
                           N_("Volume Up"),  N_("Volume Down"),  N_("Forward 5s"),
                           N_("Rewind 5s"),  N_("Shuffle"),      N_("Repeat"),
                           N_("Alternate"),  N_("Nothing")};

/* ---------------------------------------------------------------------- */
static void set_joy_config(void)
{
    int i;
    joy_cfg.sens = atoi(gtk_entry_get_text(GTK_ENTRY(sens_entry)));
    joy_cfg.up = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[0]));
    joy_cfg.down = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[1]));
    joy_cfg.left = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[2]));
    joy_cfg.right = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[3]));
    joy_cfg.alt_up = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[4]));
    joy_cfg.alt_down = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[5]));
    joy_cfg.alt_left = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[6]));
    joy_cfg.alt_right = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[7]));

    for (i = 0; i < joy_cfg.num_buttons; i++) {
        joy_cfg.button_cmd[i] = gtk_combo_box_get_active(GTK_COMBO_BOX(joy_menus[8 + i]));
    }
}

/* ---------------------------------------------------------------------- */
static void joyconf_ok_cb(GtkWidget *w, gpointer data)
{
    set_joy_config();
    joy_cfg.device_1 = g_strdup(gtk_entry_get_text(GTK_ENTRY(dev_entry1)));
    joy_cfg.device_2 = g_strdup(gtk_entry_get_text(GTK_ENTRY(dev_entry2)));
    joyapp_save_config();
    g_free(joy_menus);
    gtk_widget_destroy(joyconf_mainwin);
}

/* ---------------------------------------------------------------------- */
static void joyconf_cancel_cb(GtkWidget *w, gpointer data)
{
    joyapp_read_config();
    joyapp_read_buttoncmd();
    g_free(joy_menus);
    gtk_widget_destroy(joyconf_mainwin);
}

/* ---------------------------------------------------------------------- */
static void joyconf_apply_cb(GtkWidget *w, gpointer data)
{
    set_joy_config();
}

/* ---------------------------------------------------------------------- */
void joy_configure(void)
{
    gint hist_val[4];
    GtkWidget *vbox, *vbox2, *hbox, *box, *box2, *frame, *table, *label, *button, *item;
    GtkWidget *dir_pack, *blist;
    int i, j;
    char buf[20];

    joyapp_read_config();

    if (!joyconf_mainwin) {
        joyconf_mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        g_signal_connect(G_OBJECT(joyconf_mainwin), "destroy", G_CALLBACK(gtk_widget_destroyed),
                         &joyconf_mainwin);
        gtk_window_set_title(GTK_WINDOW(joyconf_mainwin), _("XMMS Joystick Configuration"));
        gtk_window_set_position(GTK_WINDOW(joyconf_mainwin), GTK_WIN_POS_MOUSE);
        gtk_container_set_border_width(GTK_CONTAINER(joyconf_mainwin), 10);

        /* -------------------------------------------------- */
        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_container_add(GTK_CONTAINER(joyconf_mainwin), hbox);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_add(GTK_CONTAINER(hbox), vbox);

        vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_add(GTK_CONTAINER(hbox), vbox2);

        box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(box), 5);
        gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, TRUE, 0);

        box2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(box2), 5);
        gtk_box_pack_start(GTK_BOX(vbox2), box2, TRUE, TRUE, 0);

        /* -------------------------------------------------- */
        frame = gtk_frame_new(_("Devices:"));
        gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);

        table = gtk_table_new(3, 2, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(table), 5);
        gtk_container_add(GTK_CONTAINER(frame), table);
        gtk_table_set_row_spacings(GTK_TABLE(table), 5);
        gtk_table_set_col_spacings(GTK_TABLE(table), 5);

        /* ------------------------------ */
        label = gtk_label_new(_("Joystick 1:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
        gtk_widget_show(label);

        dev_entry1 = gtk_entry_new();
        gtk_widget_set_size_request(dev_entry1, 100, -1);
        gtk_entry_set_text(GTK_ENTRY(dev_entry1), joy_cfg.device_1);
        gtk_table_attach_defaults(GTK_TABLE(table), dev_entry1, 1, 2, 0, 1);
        gtk_widget_show(dev_entry1);

        /* ------------------------------ */
        label = gtk_label_new(_("Joystick 2:"));
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
        gtk_widget_show(label);

        dev_entry2 = gtk_entry_new();
        gtk_widget_set_size_request(dev_entry2, 100, -1);
        gtk_entry_set_text(GTK_ENTRY(dev_entry2), joy_cfg.device_2);
        gtk_table_attach_defaults(GTK_TABLE(table), dev_entry2, 1, 2, 1, 2);
        gtk_widget_show(dev_entry2);

        /* ------------------------------ */
        label = gtk_label_new(_("Sensitivity (10-32767):"));
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
        gtk_widget_show(label);

        sens_entry = gtk_entry_new();
        gtk_widget_set_size_request(sens_entry, 100, -1);
        sprintf(buf, "%d", joy_cfg.sens);
        gtk_entry_set_text(GTK_ENTRY(sens_entry), buf);
        gtk_table_attach_defaults(GTK_TABLE(table), sens_entry, 1, 2, 2, 3);
        gtk_widget_show(sens_entry);

        /* ------------------------------ */
        gtk_widget_show(table);
        gtk_widget_show(frame);

        /* -------------------------------------------------- */
        joy_menus = g_malloc(sizeof(GtkWidget *) * (8 + joy_cfg.num_buttons));
        for (j = 0; j < (8 + joy_cfg.num_buttons); j++) {
            joy_menus[j] = gtk_combo_box_text_new();
            for (i = 0; i < num_menu_txt; i++) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(joy_menus[j]), _(menu_txt[i]));
            }
        }

        /* -------------------------------------------------- */
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

        /* GTK3: GtkPacker replaced with GtkGrid, GtkOptionMenu with joy_menus combo */
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
        gtk_widget_show(frame);

        /* -------------------------------------------------- */
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

        /* GTK3: GtkPacker replaced with GtkGrid, GtkOptionMenu with joy_menus combo */
        {
            static const char *_dlbl2[] = {N_("Alt Up:"), N_("Alt Down:"), N_("Alt Left:"),
                                           N_("Alt Right:")};
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
        gtk_widget_show(frame);

        /* -------------------------------------------------- */
        frame = gtk_frame_new(_("Buttons:"));
        gtk_box_pack_start(GTK_BOX(box2), frame, FALSE, FALSE, 0);

        table = gtk_table_new(joy_cfg.num_buttons, 2, TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(table), 5);
        gtk_container_add(GTK_CONTAINER(frame), table);
        gtk_table_set_row_spacings(GTK_TABLE(table), 5);
        gtk_table_set_col_spacings(GTK_TABLE(table), 5);

        for (i = 0; i < joy_cfg.num_buttons; i++) {
            char *tmpstr = g_strdup_printf(_("Button %d:"), i + 1);
            label = gtk_label_new(tmpstr);
            g_free(tmpstr);
            gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
            gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, i, i + 1);
            gtk_widget_show(label);

            blist = joy_menus[8 + i];
            gtk_widget_set_size_request(blist, 120, -1);
            gtk_table_attach_defaults(GTK_TABLE(table), blist, 1, 2, i, i + 1);
            gtk_combo_box_set_active(GTK_COMBO_BOX(blist), joy_cfg.button_cmd[i]);
            gtk_widget_show(blist);
        }

        gtk_widget_show(table);
        gtk_widget_show(frame);

        /* -------------------------------------------------- */
        gtk_widget_show(box);

        /* -------------------------------------------------- */
        box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(box), GTK_BUTTONBOX_END);
        gtk_box_set_spacing(GTK_BOX(box), 5);
        gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);

        button = gtk_button_new_with_label(_("OK"));
        g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(joyconf_ok_cb), NULL);
        gtk_widget_set_can_default(button, TRUE);
        gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
        gtk_widget_grab_default(button);
        gtk_widget_show(button);

        button = gtk_button_new_with_label(_("Cancel"));
        g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(joyconf_cancel_cb), NULL);
        gtk_widget_set_can_default(button, TRUE);
        gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
        gtk_widget_show(button);

        button = gtk_button_new_with_label(_("Apply"));
        g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(joyconf_apply_cb), NULL);
        gtk_widget_set_can_default(button, TRUE);
        gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
        gtk_widget_show(button);

        gtk_widget_show(box);
        gtk_widget_show(box2);
        /* -------------------------------------------------- */
        gtk_widget_show(vbox);
        gtk_widget_show(vbox2);
        gtk_widget_show(hbox);
        gtk_widget_show(joyconf_mainwin);
    }
}
