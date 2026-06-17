/*
 * fuzz_titlestring.c — libFuzzer harness for the XMMS title-format parser.
 *
 * xmms_get_titlestring() expands a user-configurable format string (the
 * "%p - %t" style title template) against song metadata. The format string
 * and the metadata fields both originate from untrusted input (config file,
 * tags inside media files), so the %-escape parser is a real attack surface.
 *
 * The fuzz buffer is split: the first NUL-terminated chunk is the format
 * string; the remainder seeds the metadata fields.
 *
 * Build: configure with -DXMMS_BUILD_FUZZERS=ON using clang.
 */
#include <glib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "titlestring.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0)
        return 0;

    /* fmt = data up to the first NUL (or the whole buffer). */
    gchar *fmt = g_strndup((const gchar *)data, size);

    /* Derive a couple of metadata fields from the tail so the parser has
     * something to substitute for %p / %t / %f etc. */
    size_t flen = strlen(fmt);
    const char *tail = (flen < size) ? (const char *)data + flen + 1 : "";
    size_t tail_len = (flen < size) ? size - flen - 1 : 0;
    gchar *meta = g_strndup(tail, tail_len);

    TitleInput *input;
    XMMS_NEW_TITLEINPUT(input);
    input->performer = meta;
    input->album_name = meta;
    input->track_name = meta;
    input->track_number = (gint)tail_len;
    input->year = 1997;
    input->genre = meta;
    input->comment = meta;
    input->file_name = meta;
    input->file_ext = meta;
    input->file_path = meta;

    gchar *out = xmms_get_titlestring(fmt, input);
    g_free(out);

    g_free(input);
    g_free(meta);
    g_free(fmt);
    return 0;
}
