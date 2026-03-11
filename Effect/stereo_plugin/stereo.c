#include <gtk/gtk.h>

#include <xmms/plugin.h>

#include "config.h"
#include "libxmms/configfile.h"
#include "libxmms/util.h"
#include "xmms/i18n.h"

static void init(void);
static void about(void);
static void configure(void);
static int mod_samples(gpointer *d, gint length, AFormat afmt, gint srate, gint nch);

EffectPlugin stereo_ep = {NULL, NULL, NULL, /* Description */
                          init, NULL, about, configure, mod_samples};

static const char *about_text = N_("Extra Stereo Plugin\n\n"
                                   "By Johan Levin 1999.");

static GtkWidget *conf_dialog = NULL;
static gfloat value;

EffectPlugin *get_eplugin_info(void)
{
    stereo_ep.description = g_strdup_printf(_("Extra Stereo Plugin %s"), VERSION);
    return &stereo_ep;
}

static void init(void)
{
    ConfigFile *cfg;
    cfg = xmms_cfg_open_default_file();
    if (!xmms_cfg_read_float(cfg, "extra_stereo", "intensity", &value))
        value = 2.5;
    xmms_cfg_free(cfg);
}

static void about(void)
{
    static GtkWidget *about_dialog = NULL;

    if (about_dialog != NULL)
        return;

    about_dialog = xmms_show_message(_("About Extra Stereo Plugin"), _(about_text), _("OK"), FALSE,
                                     NULL, NULL);
    g_signal_connect(G_OBJECT(about_dialog), "destroy", G_CALLBACK(gtk_widget_destroyed),
                     &about_dialog);
}

static void conf_ok_cb(GtkButton *button, gpointer data)
{
    ConfigFile *cfg;

    value = (gfloat)gtk_adjustment_get_value(GTK_ADJUSTMENT(data));

    cfg = xmms_cfg_open_default_file();
    xmms_cfg_write_float(cfg, "extra_stereo", "intensity", value);
    xmms_cfg_write_default_file(cfg);
    xmms_cfg_free(cfg);
    gtk_widget_destroy(conf_dialog);
}

static void conf_cancel_cb(GtkButton *button, gpointer data)
{
    gtk_widget_destroy(conf_dialog);
}

static void conf_apply_cb(GtkButton *button, gpointer data)
{
    value = (gfloat)gtk_adjustment_get_value(GTK_ADJUSTMENT(data));
}

static void configure(void)
{
    GtkWidget *hbox, *label, *scale, *button, *bbox;
    GtkAdjustment *adjustment;

    if (conf_dialog != NULL)
        return;

    conf_dialog = gtk_dialog_new();
    g_signal_connect(G_OBJECT(conf_dialog), "destroy", G_CALLBACK(gtk_widget_destroyed),
                     &conf_dialog);
    gtk_window_set_title(GTK_WINDOW(conf_dialog), _("Configure Extra Stereo"));

    label = gtk_label_new(_("Effect intensity:"));
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(conf_dialog))), label, TRUE,
                       TRUE, 0);
    gtk_widget_show(label);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(conf_dialog))), hbox, TRUE,
                       TRUE, 10);
    gtk_widget_show(hbox);

    adjustment = gtk_adjustment_new(value, 0.0, 15.0 + 1.0, 0.1, 1.0, 1.0);
    scale = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
    gtk_box_pack_start(GTK_BOX(hbox), scale, TRUE, TRUE, 10);
    gtk_widget_show(scale);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX((gtk_dialog_get_action_area(GTK_DIALOG(conf_dialog)))), bbox, TRUE,
                       TRUE, 0);

    button = gtk_button_new_with_label(_("OK"));
    gtk_widget_set_can_default(button, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(conf_ok_cb), (gpointer)adjustment);
    gtk_widget_grab_default(button);
    gtk_widget_show(button);

    button = gtk_button_new_with_label(_("Cancel"));
    gtk_widget_set_can_default(button, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(conf_cancel_cb), NULL);
    gtk_widget_show(button);

    button = gtk_button_new_with_label(_("Apply"));
    gtk_widget_set_can_default(button, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(conf_apply_cb), (gpointer)adjustment);
    gtk_widget_show(button);

    gtk_widget_show(bbox);

    gtk_widget_show(conf_dialog);
}

static int mod_samples(gpointer *d, gint length, AFormat afmt, gint srate, gint nch)
{
    gint i;
    gdouble avg, ldiff, rdiff, tmp, mul;
    gint16 *data = (gint16 *)*d;

    if (!(afmt == FMT_S16_NE || (afmt == FMT_S16_LE && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
          (afmt == FMT_S16_BE && G_BYTE_ORDER == G_BIG_ENDIAN)) ||
        nch != 2)
        return length;

    mul = value;

    for (i = 0; i < length / 2; i += 2) {
        avg = (data[i] + data[i + 1]) / 2;
        ldiff = data[i] - avg;
        rdiff = data[i + 1] - avg;

        tmp = avg + ldiff * mul;
        if (tmp < -32768)
            tmp = -32768;
        if (tmp > 32767)
            tmp = 32767;
        data[i] = tmp;

        tmp = avg + rdiff * mul;
        if (tmp < -32768)
            tmp = -32768;
        if (tmp > 32767)
            tmp = 32767;
        data[i + 1] = tmp;
    }
    return length;
}
