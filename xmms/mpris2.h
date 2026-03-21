/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2003  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2004  Haavard Kvaalen
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

/* mpris2.h — MPRIS 2.2 D-Bus service — Closes #37 */

#ifndef MPRIS2_H
#define MPRIS2_H

/*
 * mpris2_init() — register org.mpris.MediaPlayer2.xmms on the session bus.
 * Call once after gtk_init() and before gtk_main().
 */
void mpris2_init(void);

/*
 * mpris2_update_state() — emit org.freedesktop.DBus.Properties.PropertiesChanged
 * on the Player interface.  Call whenever playback status or track changes.
 */
void mpris2_update_state(void);

#endif /* MPRIS2_H */
