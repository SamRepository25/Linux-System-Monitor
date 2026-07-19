#define _POSIX_C_SOURCE 200809L

#include "color.h"

#include <stdio.h>
#include <unistd.h>

int g_color_enabled = 1;

int color_detect_tty(void) {
    return isatty(fileno(stdout));
}

const char *color_for_percent(double percent) {
    if (!g_color_enabled) return "";
    if (percent >= 80.0) return COLOR_RED;
    if (percent >= 50.0) return COLOR_YELLOW;
    return COLOR_GREEN;
}

const char *color_reset(void) {
    return g_color_enabled ? COLOR_RESET : "";
}

const char *color_for_bool(int is_true) {
    if (!g_color_enabled) return "";
    return is_true ? COLOR_GREEN : COLOR_RED;
}
