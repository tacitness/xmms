#include <gtk/gtk.h>

#include "echo.h"
#include "libxmms/configfile.h"
#include "libxmms/util.h"
#include "xmms/i18n.h"

static const char *echo_about_text = N_("Echo Plugin\n"
                                        "By Johan Levin 1999.\n\n"
                                        "Surround echo by Carl van Schaik 1999");

static GtkWidget *conf_dialog = NULL, *surround_btn;
static GtkAdjustment *echo_delay_adj, *echo_feedback_adj, *echo_volume_adj;

void echo_about(void)
{
    static GtkWidget *echo_about_dialog = NULL;

    if (echo_about_dialog != NULL)
        return;

    echo_about_dialog =
        xmms_show_message(_("About Echo Plugin"), _(echo_about_text), _("OK"), FALSE, NULL, NULL);
    g_signal_connect(G_OBJECT(echo_about_dialog), "destroy", G_CALLBACK(gtk_widget_destroyed),
                     &echo_about_dialog);
}

static void apply_changes(void)
{
    ConfigFile *cfg;
    echo_delay = gtk_adjustment_get_value(GTK_ADJUSTMENT(echo_delay_adj));
    echo_feedback = gtk_adjustment_get_value(GTK_ADJUSTMENT(echo_feedback_adj));
    echo_volume = gtk_adjustment_get_value(GTK_ADJUSTMENT(echo_volume_adj));
    echo_surround_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(surround_btn));

    cfg = xmms_cfg_open_default_file();
    xmms_cfg_write_int(cfg, "echo_plugin", "delay", echo_delay);
    xmms_cfg_write_int(cfg, "echo_plugin", "feedback", echo_feedback);
    xmms_cfg_write_int(cfg, "echo_plugin", "volume", echo_volume);
    xmms_cfg_write_boolean(cfg, "echo_plugin", "enable_surround", echo_surround_enable);
    xmms_cfg_write_default_file(cfg);
    xmms_cfg_free(cfg);
}

static void conf_ok_cb(GtkButton *button, gpointer data)
{
    apply_changes();
    gtk_widget_destroy(GTK_WIDGET(conf_dialog));
}

static void conf_cancel_cb(GtkButton *button, gpointer data)
{
    gtk_widget_destroy(GTK_WIDGET(conf_dialog));
}

static void conf_apply_cb(GtkButton *button, gpointer data)
{
    apply_changes();
}

void echo_configure(void)
{
    GtkWidget *button, *table, *label, *hscale, *bbox;

    if (conf_dialog != NULL)
        return;

    conf_dialog = gtk_dialog_new();
    g_signal_connect(G_OBJECT(conf_dialog), "destroy", G_CALLBACK(gtk_widget_destroyed),
                     &conf_dialog);
    gtk_window_set_title(GTK_WINDOW(conf_dialog), _("Configure Echo"));

    echo_delay_adj = gtk_adjustment_new(echo_delay, 0, MAX_DELAY + 100, 10, 100, 100);
    echo_feedback_adj = gtk_adjustment_new(echo_feedback, 0, 100 + 10, 2, 10, 10);
    echo_volume_adj = gtk_adjustment_new(echo_volume, 0, 100 + 10, 2, 10, 10);

    table = gtk_table_new(2, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 5);
    gtk_container_set_border_width(GTK_CONTAINER(table), 5);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(conf_dialog))), table, TRUE,
                       TRUE, 5);
    gtk_widget_show(table);

    label = gtk_label_new(_("Delay: (ms)"));
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(label);

    label = gtk_label_new(_("Feedback: (%)"));
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(label);

    label = gtk_label_new(_("Volume: (%)"));
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(label);

    hscale = gtk_hscale_new(GTK_ADJUSTMENT(echo_delay_adj));
    gtk_widget_set_size_request(hscale, 400, 35);
    gtk_scale_set_digits(GTK_SCALE(hscale), 0);
    gtk_table_attach_defaults(GTK_TABLE(table), hscale, 1, 2, 0, 1);
    gtk_widget_show(hscale);

    hscale = gtk_hscale_new(GTK_ADJUSTMENT(echo_feedback_adj));
    gtk_widget_set_size_request(hscale, 400, 35);
    gtk_scale_set_digits(GTK_SCALE(hscale), 0);
    gtk_table_attach_defaults(GTK_TABLE(table), hscale, 1, 2, 1, 2);
    gtk_widget_show(hscale);

    hscale = gtk_hscale_new(GTK_ADJUSTMENT(echo_volume_adj));
    gtk_widget_set_size_request(hscale, 400, 35);
    gtk_scale_set_digits(GTK_SCALE(hscale), 0);
    gtk_table_attach_defaults(GTK_TABLE(table), hscale, 1, 2, 2, 3);
    gtk_widget_show(hscale);

    surround_btn = gtk_check_button_new_with_label(_("Surround echo"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(surround_btn), echo_surround_enable);
    gtk_widget_show(surround_btn);

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(conf_dialog))), surround_btn,
                       TRUE, TRUE, 5);

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX((gtk_dialog_get_action_area(GTK_DIALOG(conf_dialog)))), bbox, TRUE,
                       TRUE, 0);


    button = gtk_button_new_with_label(_("OK"));
    gtk_widget_set_can_default(button, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(conf_ok_cb), NULL);
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
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(conf_apply_cb), NULL);
    gtk_widget_show(button);
    gtk_widget_show(bbox);

    gtk_widget_show(conf_dialog);
}
