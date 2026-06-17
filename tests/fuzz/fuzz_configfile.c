/*
 * fuzz_configfile.c — libFuzzer harness for the XMMS INI/config parser.
 *
 * xmms_cfg_open_file() is the backend used to parse `.pls` playlists
 * (read_ini_string in playlist.c) and the on-disk xmms config — i.e. it
 * processes untrusted, user-supplied files. This harness writes the fuzz
 * input to a temp file, parses it, exercises a few lookups, and frees.
 *
 * Build: configure with -DXMMS_BUILD_FUZZERS=ON using clang.
 * Run:   ./fuzz_configfile -max_total_time=60 corpus/configfile
 */
#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "configfile.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Write the fuzz buffer to a unique temp file. */
    char tmpl[] = "/tmp/xmms_fuzz_cfg_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0)
        return 0;
    if (size > 0 && write(fd, data, size) < 0) {
        close(fd);
        unlink(tmpl);
        return 0;
    }
    close(fd);

    ConfigFile *cfg = xmms_cfg_open_file(tmpl);
    if (cfg) {
        /* Exercise the read paths against attacker-influenced section/keys. */
        gchar *s = NULL;
        gint i = 0;
        gboolean b = FALSE;
        gfloat f = 0.0f;
        if (xmms_cfg_read_string(cfg, "playlist", "NumberOfEntries", &s) && s)
            g_free(s);
        s = NULL;
        if (xmms_cfg_read_string(cfg, "playlist", "File1", &s) && s)
            g_free(s);
        xmms_cfg_read_int(cfg, "xmms", "volume", &i);
        xmms_cfg_read_boolean(cfg, "xmms", "shuffle", &b);
        xmms_cfg_read_float(cfg, "equalizer", "preamp", &f);
        xmms_cfg_free(cfg);
    }

    unlink(tmpl);
    return 0;
}
