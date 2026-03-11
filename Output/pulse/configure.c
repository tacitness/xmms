/*  XMMS - PulseAudio output plugin — configuration dialog (GTK3)
 *  Copyright (C) 2024  XMMS Resurrection Project
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "pulse.h"

static GtkWidget *configure_win = NULL;
static GtkWidget *server_entry = NULL;
static GtkWidget *sink_entry = NULL;
static GtkWidget *buffer_ms_spin = NULL;

#define GET_TEXT(e) gtk_entry_get_text(GTK_ENTRY(e))
#define GET_SPIN(s) gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s))

static void configure_ok_cb(GtkWidget *w, gpointer data)
{
    const char *srv = GET_TEXT(server_entry);
    const char *snk = GET_TEXT(sink_entry);

    g_free(pulse_cfg.server);
    g_free(pulse_cfg.sink);
    pulse_cfg.server = (srv && srv[0]) ? g_strdup(srv) : NULL;
    pulse_cfg.sink = (snk && snk[0]) ? g_strdup(snk) : NULL;
    pulse_cfg.buffer_ms = GET_SPIN(buffer_ms_spin);

    pulse_save_config();
    gtk_widget_destroy(configure_win);
}

static void configure_cancel_cb(GtkWidget *w, gpointer data)
{
    gtk_widget_destroy(configure_win);
}

void pulse_configure(void)
{
    if (configure_win) {
        gtk_window_present(GTK_WINDOW(configure_win));
        return;
    }

    configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(configure_win), _("PulseAudio Output Plugin — Configuration"));
    gtk_window_set_resizable(GTK_WINDOW(configure_win), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(configure_win), 12);
    g_signal_connect(G_OBJECT(configure_win), "destroy", G_CALLBACK(gtk_widget_destroyed),
                     &configure_win);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(configure_win), vbox);

    /* ---- Server ---------------------------------------------------------- */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);

    GtkWidget *lbl;

    lbl = gtk_label_new(_("Server:"));
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, 0, 1, 1);

    server_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(server_entry), _("default"));
    if (pulse_cfg.server)
        gtk_entry_set_text(GTK_ENTRY(server_entry), pulse_cfg.server);
    gtk_widget_set_hexpand(server_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), server_entry, 1, 0, 1, 1);

    /* ---- Sink ------------------------------------------------------------- */
    lbl = gtk_label_new(_("Sink:"));
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, 1, 1, 1);

    sink_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(sink_entry), _("default"));
    if (pulse_cfg.sink)
        gtk_entry_set_text(GTK_ENTRY(sink_entry), pulse_cfg.sink);
    gtk_widget_set_hexpand(sink_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), sink_entry, 1, 1, 1, 1);

    /* ---- Buffer ----------------------------------------------------------- */
    lbl = gtk_label_new(_("Buffer (ms):"));
    gtk_widget_set_halign(lbl, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, 2, 1, 1);

    GtkAdjustment *adj = gtk_adjustment_new(pulse_cfg.buffer_ms, 500, 8000, 100, 500, 0);
    buffer_ms_spin = gtk_spin_button_new(adj, 100, 0);
    gtk_grid_attach(GTK_GRID(grid), buffer_ms_spin, 1, 2, 1, 1);

    /* ---- Info label ------------------------------------------------------- */
    GtkWidget *info =
        gtk_label_new(_("Leave Server/Sink blank to use the PulseAudio defaults.\n"
                        "Volume is controlled by software — use pavucontrol for per-app\n"
                        "hardware volume when needed."));
    gtk_label_set_line_wrap(GTK_LABEL(info), TRUE);
    gtk_widget_set_halign(info, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), info, FALSE, FALSE, 4);

    /* ---- Buttons ---------------------------------------------------------- */
    GtkWidget *hbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbox), 6);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *btn_cancel = gtk_button_new_with_mnemonic(_("_Cancel"));
    g_signal_connect(G_OBJECT(btn_cancel), "clicked", G_CALLBACK(configure_cancel_cb), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_cancel, FALSE, FALSE, 0);

    GtkWidget *btn_ok = gtk_button_new_with_mnemonic(_("_OK"));
    g_signal_connect(G_OBJECT(btn_ok), "clicked", G_CALLBACK(configure_ok_cb), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_ok, FALSE, FALSE, 0);
    gtk_widget_set_can_default(btn_ok, TRUE);
    gtk_window_set_default(GTK_WINDOW(configure_win), btn_ok);

    gtk_widget_show_all(configure_win);
}
