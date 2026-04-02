/*  XMMS - Cross-platform multimedia player
 *  libxmms/snap.c — vis-plugin snap/dock vtable global
 *
 *  Zero-initialised here; the main xmms binary calls vis_snap_init() at
 *  startup to populate all five function pointers before any vis plugin
 *  window can be created.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include "snap.h"

/* Zero-initialised — main binary populates this at startup via vis_snap_init() */
XmmsSnapInterface xmms_snap = {NULL, NULL, NULL, NULL, NULL};
