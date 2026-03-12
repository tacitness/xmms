/*  XMMS - PulseAudio output plugin — audio back-end
 *  Copyright (C) 2024  XMMS Resurrection Project
 *
 *  Design:
 *    - pa_simple (blocking simple API) for PA connection
 *    - ring buffer + dedicated audio thread (mirroring the ALSA plugin)
 *    - software volume (PA simple API has no per-stream volume control)
 *    - pause implemented by halting ring-buffer drain; PA underruns silently
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <string.h>
#include <time.h>

#include <libxmms/configfile.h>
#include <pthread.h>

#include "pulse.h"

/* --------------------------------------------------------------------------
 * Compile-time constants
 * -------------------------------------------------------------------------- */

/* Maximum bytes drained to PA per iteration of the audio thread */
#define PA_WRITE_CHUNK 4096

/* Ring buffer bounds (milliseconds): init.c clamps pulse_cfg.buffer_ms here */
#define RING_BUF_MS_MIN 500
#define RING_BUF_MS_MAX 8000
#define RING_BUF_MS_DEF 3000

/* --------------------------------------------------------------------------
 * Global state
 * -------------------------------------------------------------------------- */

/* PulseAudio connection (non-NULL only while open_audio .. close_audio) */
static pa_simple *pa_s = NULL;

/* Ring buffer */
static char *rbuf = NULL; /* heap-allocated at open_audio() */
static int rbuf_size = 0; /* total bytes allocated          */
static int rb_rd = 0;     /* read  cursor (bytes)           */
static int rb_wr = 0;     /* write cursor (bytes)           */

static pthread_mutex_t rbuf_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t rbuf_cond = PTHREAD_COND_INITIALIZER;

/* Audio thread */
static pthread_t audio_tid;
static gboolean going = FALSE; /* thread should keep running  */

/* Pause/flush state */
static gboolean paused = FALSE; /* TRUE while output paused */
static int flush_req = -1;      /* -1=none else flush-to-ms  */

/* Timing */
static guint64 total_written = 0;  /* bytes received from xmms   */
static guint64 hw_written = 0;     /* bytes sent to PA            */
static int output_time_offset = 0; /* ms adjustment after flush */
static int bps = 0;                /* bytes per second (output)  */

/* Pause time cache — valid while paused=TRUE */
static int pause_output_time = 0;

/* Current stream parameters (needed for software-volume path) */
static int stream_channels = 2;
static gboolean stream_is_16bit = TRUE; /* FALSE → 8-bit unsigned */
static gboolean stream_is_u16 = FALSE;  /* U16 needs sign-flip     */

/* --------------------------------------------------------------------------
 * Config (populated by init.c)
 * -------------------------------------------------------------------------- */
struct pulse_config pulse_cfg;

/* --------------------------------------------------------------------------
 * Ring buffer helpers  (must be called with rbuf_lock held)
 * -------------------------------------------------------------------------- */

static int ring_filled_locked(void)
{
    if (rb_wr >= rb_rd)
        return rb_wr - rb_rd;
    return rbuf_size - rb_rd + rb_wr;
}

static int ring_space_locked(void)
{
    /* leave one byte spare so full and empty are distinguishable */
    return rbuf_size - 1 - ring_filled_locked();
}

/* Copy 'len' bytes from ring buffer into 'dst' and advance rd cursor.
 * Caller must ensure len <= ring_filled_locked(). */
static void ring_read_locked(char *dst, int len)
{
    int first = MIN(len, rbuf_size - rb_rd);
    memcpy(dst, rbuf + rb_rd, first);
    if (len > first)
        memcpy(dst + first, rbuf, len - first);
    rb_rd = (rb_rd + len) % rbuf_size;
}

/* Copy 'len' bytes from 'src' into ring buffer and advance wr cursor.
 * Caller must ensure len <= ring_space_locked(). */
static void ring_write_locked(const char *src, int len)
{
    int first = MIN(len, rbuf_size - rb_wr);
    memcpy(rbuf + rb_wr, src, first);
    if (len > first)
        memcpy(rbuf, src + first, len - first);
    rb_wr = (rb_wr + len) % rbuf_size;
}

/* --------------------------------------------------------------------------
 * Software volume scaling
 * -------------------------------------------------------------------------- */

/*
 * Scale a buffer of PCM audio in-place according to pulse_cfg.vol.
 * Supports S16 (NE) and U8; U16 is pre-converted to S16 before this call.
 */
static void apply_volume(char *buf, int bytes)
{
    int vl = pulse_cfg.vol.left;
    int vr = pulse_cfg.vol.right;

    if (vl == 100 && vr == 100)
        return; /* unity gain — nothing to do */

    if (stream_is_16bit) {
        int nsamples = bytes / 2;
        gint16 *s = (gint16 *)buf;
        for (int i = 0; i < nsamples; i++) {
            int v = (stream_channels == 2) ? ((i % 2 == 0) ? vl : vr) : ((vl + vr) / 2);
            s[i] = (gint16)CLAMP((int)s[i] * v / 100, -32768, 32767);
        }
    } else {
        /* U8: values 0–255, centre 128 */
        guint8 *s = (guint8 *)buf;
        for (int i = 0; i < bytes; i++) {
            int v = (stream_channels == 2) ? ((i % 2 == 0) ? vl : vr) : ((vl + vr) / 2);
            int sample = (int)s[i] - 128;
            sample = sample * v / 100;
            s[i] = (guint8)CLAMP(sample + 128, 0, 255);
        }
    }
}

/* --------------------------------------------------------------------------
 * U16 → S16 conversion (flip the sign bit on every sample)
 * -------------------------------------------------------------------------- */

static void convert_u16_to_s16(char *buf, int bytes)
{
    guint16 *s = (guint16 *)buf;
    int n = bytes / 2;
    for (int i = 0; i < n; i++)
        s[i] ^= 0x8000u;
}

/* --------------------------------------------------------------------------
 * PA sample format helper
 * -------------------------------------------------------------------------- */

static pa_sample_format_t xmms_fmt_to_pa(AFormat fmt)
{
    switch (fmt) {
    case FMT_U8:
        return PA_SAMPLE_U8;
    case FMT_S16_LE:
    case FMT_U16_LE:
        return PA_SAMPLE_S16LE;
    case FMT_S16_BE:
    case FMT_U16_BE:
        return PA_SAMPLE_S16BE;
    case FMT_S16_NE:
    case FMT_U16_NE:
        return PA_SAMPLE_S16NE;
    default:
        return PA_SAMPLE_INVALID;
    }
}

/* --------------------------------------------------------------------------
 * Audio drain thread
 * -------------------------------------------------------------------------- */

static void *audio_thread_func(void *arg)
{
    (void)arg;
    char tmp[PA_WRITE_CHUNK];

    while (going) {
        pthread_mutex_lock(&rbuf_lock);

        /* ---- Handle flush request ---------------------------------------- */
        if (flush_req >= 0) {
            int target_ms = flush_req;
            flush_req = -1;

            /* Discard ring buffer */
            rb_rd = rb_wr = 0;

            /* Recalculate byte counters to match the requested time */
            guint64 new_bytes = (guint64)target_ms * bps / 1000;
            total_written = new_bytes;
            hw_written = new_bytes;
            output_time_offset = target_ms;

            pthread_cond_broadcast(&rbuf_cond);
            pthread_mutex_unlock(&rbuf_lock);

            /* Discard PA's own internal buffers too */
            if (pa_s)
                pa_simple_flush(pa_s, NULL);
            continue;
        }

        /* ---- Wait while paused or ring is empty --------------------------- */
        if (paused || ring_filled_locked() == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 5 * 1000 * 1000; /* 5 ms */
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            pthread_cond_timedwait(&rbuf_cond, &rbuf_lock, &ts);
            pthread_mutex_unlock(&rbuf_lock);

            /* If going was cleared while we slept, check if ring is empty */
            if (!going) {
                /* Drain whatever is left before exiting */
                pthread_mutex_lock(&rbuf_lock);
                int left = ring_filled_locked();
                pthread_mutex_unlock(&rbuf_lock);
                if (left == 0)
                    break;
                going = TRUE; /* pretend still going to finish drain below */
                /* fall through to the drain path in the next iteration */
                going = FALSE;
            }
            continue;
        }

        /* ---- Drain one chunk to PA --------------------------------------- */
        int avail = ring_filled_locked();
        int chunk = MIN(avail, PA_WRITE_CHUNK);
        ring_read_locked(tmp, chunk);
        pthread_cond_broadcast(&rbuf_cond);
        pthread_mutex_unlock(&rbuf_lock);

        int err;
        if (pa_simple_write(pa_s, tmp, chunk, &err) < 0)
            g_warning("PulseAudio: write error: %s", pa_strerror(err));

        pthread_mutex_lock(&rbuf_lock);
        hw_written += chunk;
        pthread_mutex_unlock(&rbuf_lock);
    }

    /* ---- Final drain: empty whatever remains in the ring ----------------- */
    {
        int drain_errors = 0;
        for (;;) {
            pthread_mutex_lock(&rbuf_lock);
            int left = ring_filled_locked();
            if (left == 0) {
                pthread_mutex_unlock(&rbuf_lock);
                break;
            }
            int chunk = MIN(left, PA_WRITE_CHUNK);
            ring_read_locked(tmp, chunk);
            pthread_mutex_unlock(&rbuf_lock);

            int err;
            if (pa_simple_write(pa_s, tmp, chunk, &err) < 0) {
                g_warning("PulseAudio: write error during drain: %s", pa_strerror(err));
                /* PA connection is broken — discard remaining ring data and stop
                 * spinning; without this, a dead PA daemon causes an infinite
                 * busy loop at 100%% CPU that prevents XMMS from exiting. */
                if (++drain_errors >= 3)
                    break;
            } else {
                drain_errors = 0;
            }

            pthread_mutex_lock(&rbuf_lock);
            hw_written += chunk;
            pthread_mutex_unlock(&rbuf_lock);
        }
    }

    return NULL;
}

/* --------------------------------------------------------------------------
 * Plugin API
 * -------------------------------------------------------------------------- */

void pulse_init(void)
{
    ConfigFile *cf;

    memset(&pulse_cfg, 0, sizeof(pulse_cfg));
    pulse_cfg.buffer_ms = RING_BUF_MS_DEF;
    pulse_cfg.vol.left = 100;
    pulse_cfg.vol.right = 100;

    cf = xmms_cfg_open_default_file();
    xmms_cfg_read_string(cf, "PULSE", "server", &pulse_cfg.server);
    xmms_cfg_read_string(cf, "PULSE", "sink", &pulse_cfg.sink);
    xmms_cfg_read_int(cf, "PULSE", "buffer_ms", &pulse_cfg.buffer_ms);
    xmms_cfg_read_int(cf, "PULSE", "vol_left", &pulse_cfg.vol.left);
    xmms_cfg_read_int(cf, "PULSE", "vol_right", &pulse_cfg.vol.right);
    xmms_cfg_free(cf);

    pulse_cfg.buffer_ms = CLAMP(pulse_cfg.buffer_ms, RING_BUF_MS_MIN, RING_BUF_MS_MAX);
    pulse_cfg.vol.left = CLAMP(pulse_cfg.vol.left, 0, 100);
    pulse_cfg.vol.right = CLAMP(pulse_cfg.vol.right, 0, 100);
}

void pulse_save_config(void)
{
    ConfigFile *cf = xmms_cfg_open_default_file();
    if (pulse_cfg.server)
        xmms_cfg_write_string(cf, "PULSE", "server", pulse_cfg.server);
    if (pulse_cfg.sink)
        xmms_cfg_write_string(cf, "PULSE", "sink", pulse_cfg.sink);
    xmms_cfg_write_int(cf, "PULSE", "buffer_ms", pulse_cfg.buffer_ms);
    xmms_cfg_write_int(cf, "PULSE", "vol_left", pulse_cfg.vol.left);
    xmms_cfg_write_int(cf, "PULSE", "vol_right", pulse_cfg.vol.right);
    xmms_cfg_write_default_file(cf);
    xmms_cfg_free(cf);
}

void pulse_get_volume(int *l, int *r)
{
    *l = pulse_cfg.vol.left;
    *r = pulse_cfg.vol.right;
}

void pulse_set_volume(int l, int r)
{
    pulse_cfg.vol.left = CLAMP(l, 0, 100);
    pulse_cfg.vol.right = CLAMP(r, 0, 100);
}

int pulse_open(AFormat fmt, int rate, int nch)
{
    pa_sample_spec ss;
    int err;

    ss.format = xmms_fmt_to_pa(fmt);
    ss.rate = (uint32_t)rate;
    ss.channels = (uint8_t)nch;

    if (ss.format == PA_SAMPLE_INVALID) {
        g_warning("PulseAudio: unsupported format %d — falling back to S16NE", fmt);
        ss.format = PA_SAMPLE_S16NE;
    }

    pa_s = pa_simple_new(pulse_cfg.server && pulse_cfg.server[0] ? pulse_cfg.server : NULL, "XMMS",
                         PA_STREAM_PLAYBACK,
                         pulse_cfg.sink && pulse_cfg.sink[0] ? pulse_cfg.sink : NULL, "Music", &ss,
                         NULL, /* default channel map */
                         NULL, /* default buffering attributes */
                         &err);

    if (!pa_s) {
        g_warning("PulseAudio: pa_simple_new() failed: %s", pa_strerror(err));
        return FALSE;
    }

    /* Identify sample properties for volume scaling */
    stream_channels = nch;
    stream_is_16bit = (fmt != FMT_U8);
    stream_is_u16 = (fmt == FMT_U16_LE || fmt == FMT_U16_BE || fmt == FMT_U16_NE);

    /* Calculate bytes-per-second */
    int sample_bytes = stream_is_16bit ? 2 : 1;
    bps = rate * nch * sample_bytes;

    /* Allocate ring buffer */
    rbuf_size = pulse_cfg.buffer_ms * bps / 1000;
    rbuf_size = MAX(rbuf_size, PA_WRITE_CHUNK * 4); /* minimum sanity */
    rbuf_size += 1;                                 /* +1 so full ≠ empty */
    rbuf = g_malloc(rbuf_size);
    rb_rd = rb_wr = 0;

    /* Reset timing */
    total_written = 0;
    hw_written = 0;
    output_time_offset = 0;
    flush_req = -1;
    paused = FALSE;
    pause_output_time = 0;

    /* Start audio thread */
    going = TRUE;
    if (pthread_create(&audio_tid, NULL, audio_thread_func, NULL) != 0) {
        g_warning("PulseAudio: failed to create audio thread");
        pa_simple_free(pa_s);
        pa_s = NULL;
        g_free(rbuf);
        rbuf = NULL;
        going = FALSE;
        return FALSE;
    }

    return TRUE;
}

void pulse_write(void *ptr, int length)
{
    char *buf = (char *)ptr;
    char *tmp = NULL;
    int pos = 0;

    if (!going || !pa_s)
        return;

    /* Work on a private copy so we can modify it (volume / format conversion) */
    tmp = g_malloc(length);
    memcpy(tmp, buf, length);

    if (stream_is_u16)
        convert_u16_to_s16(tmp, length);
    apply_volume(tmp, length);

    /* Push into ring buffer, blocking until space is available */
    while (pos < length) {
        pthread_mutex_lock(&rbuf_lock);

        int space = ring_space_locked();
        if (space <= 0) {
            /* Ring full — wait for the audio thread to drain some */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 5 * 1000 * 1000; /* 5 ms */
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            pthread_cond_timedwait(&rbuf_cond, &rbuf_lock, &ts);
            pthread_mutex_unlock(&rbuf_lock);
            continue;
        }

        int can_write = MIN(length - pos, space);
        ring_write_locked(tmp + pos, can_write);
        total_written += can_write;
        pos += can_write;

        pthread_cond_broadcast(&rbuf_cond);
        pthread_mutex_unlock(&rbuf_lock);
    }

    g_free(tmp);
}

void pulse_close(void)
{
    if (!going && !pa_s)
        return;

    /* Signal thread to stop and wait for it to drain the ring */
    going = FALSE;
    pthread_cond_broadcast(&rbuf_cond);
    pthread_join(audio_tid, NULL);

    /* Ensure PA's own internal buffers are fully played out */
    if (pa_s) {
        pa_simple_drain(pa_s, NULL);
        pa_simple_free(pa_s);
        pa_s = NULL;
    }

    g_free(rbuf);
    rbuf = NULL;
    rbuf_size = 0;
    rb_rd = rb_wr = 0;

    total_written = 0;
    hw_written = 0;
    bps = 0;
    flush_req = -1;
    paused = FALSE;
}

void pulse_flush(int time)
{
    if (!going)
        return;

    pthread_mutex_lock(&rbuf_lock);
    flush_req = time;
    pthread_cond_broadcast(&rbuf_cond);
    pthread_mutex_unlock(&rbuf_lock);

    /* Wait until the audio thread acknowledges the flush.  Read flush_req
     * under the lock to avoid a data race; bail out if going was cleared
     * (audio thread exited) to prevent spinning forever. */
    for (;;) {
        int pending;
        pthread_mutex_lock(&rbuf_lock);
        pending = (flush_req >= 0);
        pthread_mutex_unlock(&rbuf_lock);
        if (!pending || !going)
            break;
        g_usleep(1000);
    }
}

void pulse_pause(short p)
{
    if (!going)
        return;

    pthread_mutex_lock(&rbuf_lock);
    if (p && !paused) {
        /* Capture output position so time doesn't drift during pause */
        pa_usec_t lat_us = 0;
        int err = 0;

        pthread_mutex_unlock(&rbuf_lock); /* drop lock before PA call */

        lat_us = pa_simple_get_latency(pa_s, &err);
        if (lat_us == (pa_usec_t)-1)
            lat_us = 0;

        pthread_mutex_lock(&rbuf_lock);
        int hw_ms = output_time_offset + (int)((hw_written * 1000LL) / bps);
        int lat_ms = (int)(lat_us / 1000);
        pause_output_time = MAX(0, hw_ms - lat_ms);
        paused = TRUE;
        pthread_mutex_unlock(&rbuf_lock);

        /* Flush PA's internal buffer so silence starts immediately */
        pa_simple_flush(pa_s, NULL);
    } else {
        paused = (gboolean)p;
        pthread_cond_broadcast(&rbuf_cond);
        pthread_mutex_unlock(&rbuf_lock);
    }
}

int pulse_free(void)
{
    if (!going)
        return 0;

    pthread_mutex_lock(&rbuf_lock);
    int space = ring_space_locked();
    pthread_mutex_unlock(&rbuf_lock);

    return MAX(0, space);
}

int pulse_playing(void)
{
    if (!going || !pa_s)
        return FALSE;

    pthread_mutex_lock(&rbuf_lock);
    int filled = ring_filled_locked();
    pthread_mutex_unlock(&rbuf_lock);

    return (filled > 0);
}

int pulse_get_written_time(void)
{
    if (!bps)
        return 0;

    pthread_mutex_lock(&rbuf_lock);
    int t = output_time_offset + (int)((total_written * 1000LL) / bps);
    pthread_mutex_unlock(&rbuf_lock);

    return t;
}

int pulse_get_output_time(void)
{
    if (!going || !pa_s || !bps)
        return 0;

    /* Return frozen position while paused to prevent seeking artefacts */
    if (paused)
        return pause_output_time;

    pthread_mutex_lock(&rbuf_lock);
    int hw_ms = output_time_offset + (int)((hw_written * 1000LL) / bps);
    pthread_mutex_unlock(&rbuf_lock);

    int err = 0;
    pa_usec_t lat_us = pa_simple_get_latency(pa_s, &err);
    int lat_ms = (lat_us == (pa_usec_t)-1) ? 0 : (int)(lat_us / 1000);

    return MAX(0, hw_ms - lat_ms);
}
