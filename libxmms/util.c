#if defined(HAVE_CONFIG_H)
#    include "config.h"
#endif

#include <gtk/gtk.h>

#ifdef HAVE_SCHED_H
#    include <sched.h>
#elif defined HAVE_SYS_SCHED_H
#    include <sys/sched.h>
#endif

#ifdef __FreeBSD__
#    include <sys/sysctl.h>
#    include <sys/types.h>
#endif

#if TIME_WITH_SYS_TIME
#    include <sys/time.h>
#    include <time.h>
#else
#    if HAVE_SYS_TIME_H
#        include <sys/time.h>
#    else
#        include <time.h>
#    endif
#endif

#ifndef HAVE_NANOSLEEP
#    include <sys/types.h>
#    ifdef HAVE_UNISTD_H
#        include <unistd.h>
#    endif
#endif


/* GTK3: GtkSignalFunc -> GCallback, GTK_DIALOG->vbox -> gtk_dialog_get_content_area() */
GtkWidget *xmms_show_message(gchar *title, gchar *text, gchar *button_text, gboolean modal,
                             GCallback button_action, gpointer action_data)
{
    GtkWidget *dialog, *vbox, *label, *bbox, *button;

    dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_modal(GTK_WINDOW(dialog), modal);

    /* GTK3: gtk_vbox_new() -> gtk_box_new(VERTICAL) */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    /* GTK3: GTK_DIALOG(dialog)->vbox -> gtk_dialog_get_content_area() */
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), vbox, TRUE, TRUE,
                       0);

    label = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
    gtk_widget_show(label);
    gtk_widget_show(vbox);

    /* GTK3: gtk_hbutton_box_new() -> gtk_button_box_new(HORIZONTAL) */
    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
    /* GTK3: gtk_button_box_set_spacing() deprecated -> gtk_box_set_spacing() */
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    /* GTK3: GTK_DIALOG(dialog)->action_area -> gtk_dialog_get_action_area() */
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(dialog))), bbox, FALSE, FALSE,
                       0);

    button = gtk_button_new_with_label(button_text);
    if (button_action)
        /* GTK3: gtk_signal_connect() -> g_signal_connect() */
        g_signal_connect(button, "clicked", button_action, action_data);
    /* GTK3: gtk_signal_connect_object() -> g_signal_connect_swapped() */
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
    /* GTK3: GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT) */
    gtk_widget_set_can_default(button, TRUE);
    gtk_widget_grab_default(button);
    gtk_widget_show(button);

    gtk_widget_show(bbox);
    gtk_widget_show(dialog);

    return dialog;
}

gboolean xmms_check_realtime_priority(void)
{
#ifdef HAVE_SCHED_SETSCHEDULER
#    ifdef __FreeBSD__
    /*
     * Check if priority scheduling is enabled in the kernel
     * before sched_getschedule() (so that we don't get
     * non-present syscall warnings in kernel log).
     */
    int val = 0;
    size_t len;

    len = sizeof(val);
    sysctlbyname("p1003_1b.priority_scheduling", &val, &len, NULL, 0);
    if (!val)
        return FALSE;
#    endif
    if (sched_getscheduler(0) == SCHED_RR)
        return TRUE;
    else
#endif
        return FALSE;
}

void xmms_usleep(gint usec)
{
#ifdef HAVE_NANOSLEEP
    struct timespec req;

    req.tv_sec = usec / 1000000;
    usec -= req.tv_sec * 1000000;
    req.tv_nsec = usec * 1000;

    nanosleep(&req, NULL);
#else
    struct timeval tv;

    tv.tv_sec = usec / 1000000;
    usec -= tv.tv_sec * 1000000;
    tv.tv_usec = usec;
    select(0, NULL, NULL, NULL, &tv);
#endif
}
