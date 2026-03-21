/*  GTK3 port: replaced all deprecated GTK2 APIs. */
#include <gtk/gtk.h>

#include "config.h"
#include "libxmms/configfile.h"
#include "opengl_spectrum.h"
#include "xmms/i18n.h"

static GtkWidget *configure_win = NULL;
static GtkWidget *vbox, *options_frame, *options_vbox;
static GtkWidget *options_3dfx_fullscreen;
static GtkWidget *bbox, *ok, *cancel;

static void configure_ok(GtkWidget *w, gpointer data)
{
    ConfigFile *cfg;
    gchar *filename;
    (void)w;
    (void)data;

    oglspectrum_cfg.tdfx_mode =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(options_3dfx_fullscreen));

    filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
    cfg = xmms_cfg_open_file(filename);
    if (!cfg)
        cfg = xmms_cfg_new();
    xmms_cfg_write_boolean(cfg, "OpenGL Spectrum", "tdfx_fullscreen", oglspectrum_cfg.tdfx_mode);
    xmms_cfg_write_file(cfg, filename);
    xmms_cfg_free(cfg);
    g_free(filename);

    gtk_widget_destroy(configure_win);
}

void oglspectrum_configure(void)
{
    if (configure_win)
        return;

    oglspectrum_read_config();

    /* GTK3: GTK_WINDOW_DIALOG removed; use GTK_WINDOW_TOPLEVEL */
    configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(configure_win), 10);
    gtk_window_set_title(GTK_WINDOW(configure_win), _("OpenGL Spectrum configuration"));
    /* GTK3: gtk_window_set_policy() removed; use gtk_window_set_resizable() */
    gtk_window_set_resizable(GTK_WINDOW(configure_win), FALSE);
    gtk_window_set_position(GTK_WINDOW(configure_win), GTK_WIN_POS_MOUSE);
    g_signal_connect(G_OBJECT(configure_win), "destroy", G_CALLBACK(gtk_widget_destroyed),
                     &configure_win);

    /* GTK3: gtk_vbox_new() removed; use gtk_box_new(GTK_ORIENTATION_VERTICAL, …) */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    options_frame = gtk_frame_new(_("Options:"));
    gtk_container_set_border_width(GTK_CONTAINER(options_frame), 5);

    options_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(options_vbox), 5);

    options_3dfx_fullscreen = gtk_check_button_new_with_label(_("3DFX Fullscreen mode"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(options_3dfx_fullscreen),
                                 oglspectrum_cfg.tdfx_mode);
    gtk_box_pack_start(GTK_BOX(options_vbox), options_3dfx_fullscreen, FALSE, FALSE, 0);
    gtk_widget_show(options_3dfx_fullscreen);

    gtk_container_add(GTK_CONTAINER(options_frame), options_vbox);
    gtk_widget_show(options_vbox);

    gtk_box_pack_start(GTK_BOX(vbox), options_frame, TRUE, TRUE, 0);
    gtk_widget_show(options_frame);

    /* GTK3: gtk_hbutton_box_new() removed; use gtk_button_box_new(HORIZONTAL) */
    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    ok = gtk_button_new_with_label(_("OK"));
    g_signal_connect(G_OBJECT(ok), "clicked", G_CALLBACK(configure_ok), NULL);
    /* GTK3: GTK_WIDGET_SET_FLAGS() removed; use gtk_widget_set_can_default() */
    gtk_widget_set_can_default(ok, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    gtk_widget_show(ok);

    cancel = gtk_button_new_with_label(_("Cancel"));
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked", G_CALLBACK(gtk_widget_destroy),
                             configure_win);
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
    gtk_widget_show(cancel);
    gtk_widget_show(bbox);

    gtk_container_add(GTK_CONTAINER(configure_win), vbox);
    gtk_widget_show(vbox);
    gtk_widget_show(configure_win);
    gtk_widget_grab_default(ok);
}
