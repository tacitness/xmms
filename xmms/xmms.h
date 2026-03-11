/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front
 * Technologies
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
#ifndef XMMS_H
#define XMMS_H

#include <X11/Xlib.h>
#include <errno.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <dirent.h>
#include <pthread.h>

#include "about.h"
#include "bmp.h"
#include "config.h"
#include "controlsocket.h"
#include "dnd.h"
#include "dock.h"
#include "effect.h"
/* GTK3: skin.h, widget.h, and vis.h included early — all widget constructors depend on
 * SkinIndex (skin.h), Widget (widget.h), and VisType/Vis (vis.h) being defined before use */
#include "plugin.h"    /* defines InputPlugin, AFormat, InputVisType — must precede input.h */
#include "skin.h"      /* defines SkinIndex — must precede hslider.h, menurow.h, monostereo.h */
#include "widget.h"    /* defines Widget — must precede eq_graph.h, eq_slider.h, menurow.h etc. */
#include "vis.h"       /* defines VisType — must precede main.h */
#include "eq_graph.h"
#include "eq_slider.h"
#include "equalizer.h"
#include "fullscreen.h"
#include "general.h"
#include "hints.h"
#include "hslider.h"
#include "i18n.h"
#include "input.h"
#include "main.h"
#include "menurow.h"
#include "monostereo.h"
#include "number.h"
#include "output.h"
#include "pbutton.h"
#include "playlist.h"
#include "playlist_list.h"
#include "playlist_popup.h"
#include "playlist_slider.h"
#include "playlistwin.h"
#include "playstatus.h"
/* plugin.h already included above */
#include "pluginenum.h"
#include "prefswin.h"
#include "sbutton.h"
/* skin.h already included above */
#include "skinwin.h"
#include "sm.h"
#include "svis.h"
#include "tbutton.h"
#include "textbox.h"
#include "urldecode.h"
#include "util.h"
#include "visualization.h"
/* widget.h and vis.h already included above */

#endif
