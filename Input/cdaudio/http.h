/*
 *  Copyright 1999 Håvard Kvålen <havardk@sol.no>
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

#ifndef XMMS_HTTP_H
#define XMMS_HTTP_H

#include <glib.h>

#include "config.h"

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <netdb.h>
#include <netinet/in.h>


gint http_open_connection(gchar *server, gint port);
void http_close_connection(gint sock);
gint http_read_line(gint sock, gchar *buf, gint size);
gint http_read_first_line(gint sock, gchar *buf, gint size);
gchar *http_get(gchar *url);

#endif
