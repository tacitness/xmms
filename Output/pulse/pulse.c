/*  XMMS - PulseAudio output plugin
 *  Copyright (C) 2024  XMMS Resurrection Project
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "pulse.h"

OutputPlugin pulse_op = {
    NULL, /* handle  — filled by xmms */
    NULL, /* filename — filled by xmms */
    NULL, /* description — set in get_oplugin_info() */
    pulse_init,
    pulse_about,
    pulse_configure,
    pulse_get_volume,
    pulse_set_volume,
    pulse_open,
    pulse_write,
    pulse_close,
    pulse_flush,
    pulse_pause,
    pulse_free,
    pulse_playing,
    pulse_get_output_time,
    pulse_get_written_time,
};

OutputPlugin *get_oplugin_info(void)
{
    pulse_op.description = g_strdup(_("PulseAudio output plugin"));
    return &pulse_op;
}
