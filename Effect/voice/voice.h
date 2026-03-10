
#ifndef VOICE_H
#define VOICE_H

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "config.h"
#include "xmms/plugin.h"

extern EffectPlugin voice_ep;

void voice_about(void);

#endif
