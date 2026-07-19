#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "color.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Strips trailing whitespace/newline from `s` in place. */
static void trim_trailing(char *s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char) s[len - 1])) {
        s[--len] = '\0';
    }
}

int config_load_file(const char *path, monitor_config_t *config) {
    if (path == NULL || config == NULL) return -1;

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1; /* missing config file is expected, not an error */
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_trailing(line);

        char *p = line;
        while (isspace((unsigned char) *p)) p++;
        if (*p == '\0' || *p == '#') continue; /* blank line or comment */

        char key[64];
        char value[64];
        if (sscanf(p, "%63[^=]=%63s", key, value) != 2) continue;
        trim_trailing(key);

        if (strcmp(key, "refresh") == 0) {
            int v = atoi(value);
            if (v > 0) {
                config->refresh_enabled = 1;
                config->refresh_interval_sec = (unsigned int) v;
            }
        } else if (strcmp(key, "rows") == 0) {
            int v = atoi(value);
            if (v > 0) config->process_rows = v;
        } else if (strcmp(key, "sort") == 0) {
            if (value[0] == 'c' || value[0] == 'm' || value[0] == 'p') {
                config->sort_key = value[0];
            }
        } else if (strcmp(key, "color") == 0) {
            g_color_enabled = (strcmp(value, "on") == 0 || strcmp(value, "1") == 0);
        }
        /* any other key is ignored intentionally */
    }

    fclose(fp);
    return 0;
}
