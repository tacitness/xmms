#include "skin_preview.h"

#include <glib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    GError *error = NULL;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <skin-path> <output-png>\n", argv[0]);
        return 2;
    }

    if (!skin_preview_write_png(argv[1], argv[2], &error)) {
        fprintf(stderr, "%s\n", error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        return 1;
    }

    return 0;
}
