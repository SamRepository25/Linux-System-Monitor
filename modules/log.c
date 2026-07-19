#define _POSIX_C_SOURCE 200809L

#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static FILE *g_log_fp = NULL;

int log_open(const char *path) {
    if (path == NULL) return -1;

    g_log_fp = fopen(path, "a");
    if (g_log_fp == NULL) {
        perror("log: failed to open log file");
        return -1;
    }
    return 0;
}

void log_write(const char *fmt, ...) {
    if (g_log_fp == NULL) return;

    time_t now = time(NULL);
    struct tm tm_result;
    localtime_r(&now, &tm_result);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_result);
    fprintf(g_log_fp, "[%s] ", timestamp);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_fp, fmt, args);
    va_end(args);

    fprintf(g_log_fp, "\n");
    fflush(g_log_fp);
}

void log_close(void) {
    if (g_log_fp != NULL) {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
}
