/*
 * mem.c
 *
 * Implementation of the Memory module.
 */

#include "mem.h"

#include <stdio.h>
#include <string.h>

/* ---- static (file-private) helper functions ---- */

/*
 * Parses a single /proc/meminfo line of the form:
 *   "<Key>:            <value> kB\n"
 * into `key_out` (without the trailing colon) and `value_out` (in kB).
 * Returns 1 if the line matched this format, 0 otherwise.
 */
static int parse_meminfo_line(const char *line, char *key_out,
                               size_t key_out_len, unsigned long *value_out) {
    /* "%63[^:]" reads up to 63 non-colon characters into key_out —
     * i.e. everything before the colon. "%lu" then reads the numeric
     * value. The literal "kB" in most lines is simply not consumed,
     * which is fine — we don't need it. A few lines in /proc/meminfo
     * (like HugePages counts) have no "kB" suffix at all; sscanf
     * still succeeds because we only require 2 matched items. */
    if (key_out_len == 0) return 0;

    int matched = sscanf(line, "%63[^:]: %lu", key_out, value_out);
    (void) key_out_len; /* width is baked into the format string above */

    return matched == 2;
}

/*
 * Formats a kB value as a human-readable string in `buf`, choosing
 * MB or GB depending on magnitude (GB once the value reaches 1024 MB).
 */
static void format_kb(unsigned long kb, char *buf, size_t buflen) {
    double mb = (double) kb / 1024.0;
    if (mb >= 1024.0) {
        snprintf(buf, buflen, "%.2f GB", mb / 1024.0);
    } else {
        snprintf(buf, buflen, "%.1f MB", mb);
    }
}

/* ---- public API ---- */

int mem_collect(mem_info_t *info) {
    if (info == NULL) {
        fprintf(stderr, "mem: NULL pointer passed to mem_collect\n");
        return -1;
    }

    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror("mem: failed to open /proc/meminfo");
        return -1;
    }

    memset(info, 0, sizeof(mem_info_t));

    char line[256];
    char key[64];
    unsigned long value;

    /* Track which required fields we actually found, so we can
     * detect a malformed or unexpected /proc/meminfo instead of
     * silently returning a struct full of zeros. */
    int have_total = 0, have_free = 0, have_available = 0;
    int have_buffers = 0, have_cached = 0;
    int have_swap_total = 0, have_swap_free = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (!parse_meminfo_line(line, key, sizeof(key), &value)) {
            continue; /* skip lines we can't parse (shouldn't normally happen) */
        }

        if (strcmp(key, "MemTotal") == 0) {
            info->total_kb = value;
            have_total = 1;
        } else if (strcmp(key, "MemFree") == 0) {
            info->free_kb = value;
            have_free = 1;
        } else if (strcmp(key, "MemAvailable") == 0) {
            info->available_kb = value;
            have_available = 1;
        } else if (strcmp(key, "Buffers") == 0) {
            info->buffers_kb = value;
            have_buffers = 1;
        } else if (strcmp(key, "Cached") == 0) {
            info->cached_kb = value;
            have_cached = 1;
        } else if (strcmp(key, "SwapTotal") == 0) {
            info->swap_total_kb = value;
            have_swap_total = 1;
        } else if (strcmp(key, "SwapFree") == 0) {
            info->swap_free_kb = value;
            have_swap_free = 1;
        }
        /* Any other key is irrelevant to us — ignored intentionally. */
    }

    fclose(fp);

    if (!have_total || !have_free || !have_buffers || !have_cached ||
        !have_swap_total || !have_swap_free) {
        fprintf(stderr, "mem: one or more required fields missing from /proc/meminfo\n");
        return -1;
    }

    /* MemAvailable was added in Linux 3.14 (2014). On the extremely
     * unlikely chance we're on an ancient kernel without it, fall
     * back to the older, cruder estimate: total - free - buffers - cached. */
    if (!have_available) {
        unsigned long reclaimable = info->buffers_kb + info->cached_kb;
        info->available_kb = (info->free_kb + reclaimable > info->total_kb)
            ? info->total_kb
            : info->free_kb + reclaimable;
    }

    /* Derived RAM figures. */
    info->used_kb = (info->available_kb > info->total_kb)
        ? 0
        : info->total_kb - info->available_kb;

    info->used_percent = (info->total_kb > 0)
        ? ((double) info->used_kb / (double) info->total_kb) * 100.0
        : 0.0;

    /* Derived swap figures. Guard against systems with no swap
     * configured (swap_total_kb == 0), which would otherwise divide
     * by zero. */
    info->swap_used_kb = (info->swap_free_kb > info->swap_total_kb)
        ? 0
        : info->swap_total_kb - info->swap_free_kb;

    info->swap_used_percent = (info->swap_total_kb > 0)
        ? ((double) info->swap_used_kb / (double) info->swap_total_kb) * 100.0
        : 0.0;

    return 0;
}

void mem_print(const mem_info_t *info) {
    if (info == NULL) return;

    char total_str[32], used_str[32], free_str[32], avail_str[32];
    format_kb(info->total_kb, total_str, sizeof(total_str));
    format_kb(info->used_kb, used_str, sizeof(used_str));
    format_kb(info->free_kb, free_str, sizeof(free_str));
    format_kb(info->available_kb, avail_str, sizeof(avail_str));

    printf("=== Memory Information ===\n");
    printf("Total RAM    : %s\n", total_str);
    printf("Used RAM     : %s (%.1f%%)\n", used_str, info->used_percent);
    printf("Free RAM     : %s\n", free_str);
    printf("Available    : %s\n", avail_str);

    if (info->swap_total_kb == 0) {
        printf("Swap         : none configured\n");
    } else {
        char swap_total_str[32], swap_used_str[32];
        format_kb(info->swap_total_kb, swap_total_str, sizeof(swap_total_str));
        format_kb(info->swap_used_kb, swap_used_str, sizeof(swap_used_str));
        printf("Swap         : %s / %s (%.1f%%)\n",
               swap_used_str, swap_total_str, info->swap_used_percent);
    }
}
