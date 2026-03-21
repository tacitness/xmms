#include "config.h"

#include <gtk/gtk.h>

#include "blur_scope.h"
#include "libxmms/configfile.h"
#include "xmms/i18n.h"

static GtkWidget *configure_win = NULL;
static GtkWidget *vbox, *options_frame, *options_vbox;
static GtkWidget *options_colorpicker;
static GtkWidget *bbox, *ok, *cancel;

static void configure_ok(GtkWidget *w, gpointer data)
{
    ConfigFile *cfg;
    gchar *filename;
    GdkRGBA rgba;

    filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
    cfg = xmms_cfg_open_file(filename);
    if (!cfg)
        cfg = xmms_cfg_new();
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(options_colorpicker), &rgba);
    bscope_cfg.color = ((guint32)(rgba.red   * 255) << 16) |
                       ((guint32)(rgba.green * 255) <<  8) |
                        (guint32)(rgba.blue  * 255);
    xmms_cfg_write_int(cfg, "BlurScope", "color", bscope_cfg.color);
    xmms_cfg_write_file(cfg, filename);
    xmms_cfg_free(cfg);
    g_free(filename);
    generate_cmap();
    gtk_widget_destroy(configure_win);
}

static void configure_cancel(GtkWidget *w, gpointer data)
{
    bscope_cfg.color = GPOINTER_TO_UINT(data);
    generate_cmap();
    gtk_widget_destroy(configure_win);
}

static void color_changed(GtkColorChooser *chooser, GParamSpec *pspec, gpointer data)
{
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(chooser, &rgba);
    bscope_cfg.color = ((guint32)(rgba.red   * 255) << 16) |
                       ((guint32)(rgba.green * 255) <<  8) |
                        (guint32)(rgba.blue  * 255);
    generate_cmap();
}

void bscope_configure(void)
{
    GdkRGBA rgba;
    if (configure_win)
        return;

    bscope_read_config();
    rgba.red   = (bscope_cfg.color >> 16) / 255.0;
    rgba.green = ((bscope_cfg.color >> 8) & 0xFF) / 255.0;
    rgba.blue  = (bscope_cfg.color & 0xFF) / 255.0;
    rgba.alpha = 1.0;

    /* GTK3: GTK_WINDOW_DIALOG -> GTK_WINDOW_TOPLEVEL;
     * gtk_window_set_policy -> gtk_window_set_resizable;
     * GtkColorSelection -> GtkColorChooserWidget */
    configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(configure_win), 10);
    gtk_window_set_title(GTK_WINDOW(configure_win), _("Color Entry"));
    gtk_window_set_resizable(GTK_WINDOW(configure_win), FALSE);
    gtk_window_set_position(GTK_WINDOW(configure_win), GTK_WIN_POS_MOUSE);
    g_signal_connect(G_OBJECT(configure_win), "destroy", G_CALLBACK(gtk_widget_destroyed),
                     &configure_win);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    options_frame = gtk_frame_new(_("Options:"));
    gtk_container_set_border_width(GTK_CONTAINER(options_frame), 5);

    options_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(options_vbox), 5);

    options_colorpicker = gtk_color_chooser_widget_new();
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(options_colorpicker), FALSE);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(options_colorpicker), &rgba);
    g_signal_connect(G_OBJECT(options_colorpicker), "notify::rgba",
                     G_CALLBACK(color_changed), NULL);

    gtk_box_pack_start(GTK_BOX(options_vbox), options_colorpicker, FALSE, FALSE, 0);
    gtk_widget_show(options_colorpicker);

    gtk_container_add(GTK_CONTAINER(options_frame), options_vbox);
    gtk_widget_show(options_vbox);

    gtk_box_pack_start(GTK_BOX(vbox), options_frame, TRUE, TRUE, 0);
    gtk_widget_show(options_frame);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    ok = gtk_button_new_with_label(_("OK"));
    g_signal_connect(G_OBJECT(ok), "clicked", G_CALLBACK(configure_ok), NULL);
    gtk_widget_set_can_default(ok, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    gtk_widget_show(ok);

    cancel = gtk_button_new_with_label(_("Cancel"));
    g_signal_connect(G_OBJECT(cancel), "clicked", G_CALLBACK(configure_cancel),
                     GUINT_TO_POINTER(bscope_cfg.color));
    gtk_widget_set_can_default(cancel, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
    gtk_widget_show(cancel);
    gtk_widget_show(bbox);

    gtk_container_add(GTK_CONTAINER(configure_win), vbox);
    gtk_widget_show(vbox);
    gtk_widget_show(configure_win);
    gtk_widget_grab_default(ok);
}
