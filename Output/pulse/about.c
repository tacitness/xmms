/*  XMMS - PulseAudio output plugin — about dialog
 *  Copyright (C) 2024  XMMS Resurrection Project
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <xmms/util.h>

#include "pulse.h"

void pulse_about(void)
{
    static GtkWidget *dialog = NULL;

    if (dialog)
        return;

    dialog = xmms_show_message(_("About PulseAudio Output Plugin"),
                               _("XMMS PulseAudio Output Plugin\n\n"
                                 "Outputs audio via the PulseAudio sound server using the\n"
                                 "simple blocking API (pa_simple) with a ring-buffer audio\n"
                                 "thread for reliable pause/flush handling.\n\n"
                                 "Features:\n"
                                 "  • Connect to any PulseAudio server / sink\n"
                                 "  • Configurable ring-buffer depth (500 – 8000 ms)\n"
                                 "  • Software volume with independent L/R channels\n"
                                 "  • Pause, seek (flush) and skip support\n\n"
                                 "Copyright (C) 2024  XMMS Resurrection Project\n\n"
                                 "This program is free software; you can redistribute it and/or\n"
                                 "modify it under the terms of the GNU General Public License as\n"
                                 "published by the Free Software Foundation; either version 2 of\n"
                                 "the License, or (at your option) any later version."),
                               _("OK"), FALSE, NULL, NULL);

    g_signal_connect(G_OBJECT(dialog), "destroy", G_CALLBACK(gtk_widget_destroyed), &dialog);
}
