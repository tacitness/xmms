/*  XMMS - PulseAudio output plugin
 *  Copyright (C) 2024  XMMS Resurrection Project
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef PULSE_H
#define PULSE_H

#include <gtk/gtk.h>

#include <libxmms/configfile.h>
#include <libxmms/util.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <xmms/i18n.h>
#include <xmms/plugin.h>

extern OutputPlugin pulse_op;

struct pulse_config {
    char *server;        /* NULL = default PulseAudio server */
    char *sink;          /* NULL = default sink              */
    int buffer_ms;       /* ring-buffer depth in milliseconds */
    struct {
        int left, right; /* 0–100 */
    } vol;
};

extern struct pulse_config pulse_cfg;

/* init / UI */
void pulse_init(void);
void pulse_about(void);
void pulse_configure(void);
void pulse_save_config(void);

/* volume — software-only */
void pulse_get_volume(int *l, int *r);
void pulse_set_volume(int l, int r);

/* audio I/O */
int pulse_open(AFormat fmt, int rate, int nch);
void pulse_write(void *ptr, int length);
void pulse_close(void);
void pulse_flush(int time);
void pulse_pause(short p);

/* status */
int pulse_free(void);
int pulse_playing(void);
int pulse_get_output_time(void);
int pulse_get_written_time(void);

#endif /* PULSE_H */
