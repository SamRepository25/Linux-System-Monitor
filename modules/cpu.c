/*
 * cpu.c
 *
 * Implementation of the CPU module.
 *
 * Note on feature test macros: sysconf() and nanosleep() are POSIX
 * functions, not plain ISO C. Under strict -std=c17, glibc hides their
 * declarations unless we ask for POSIX visibility explicitly. Defining
 * _POSIX_C_SOURCE before any system header is included exposes them
 * without dropping down to a GNU-specific dialect.
 */
#define _POSIX_C_SOURCE 200809L

#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* sysconf */
#include <time.h>   /* nanosleep, struct timespec */

/* ---- static (file-private) helper functions ---- */

/*
 * Converts a millisecond count into a struct timespec suitable for
 * nanosleep(). Split into whole seconds + remaining nanoseconds.
 */
static struct timespec ms_to_timespec(unsigned int ms) {
    struct timespec ts;
    ts.tv_sec = (time_t) (ms / 1000);
    ts.tv_nsec = (long) (ms % 1000) * 1000000L; /* remainder ms -> ns */
    return ts;
}

/*
 * Sums all 10 jiffie counters into one "total" value for a snapshot.
 */
static unsigned long long total_time(const cpu_times_t *t) {
    return t->user + t->nice + t->system + t->idle + t->iowait +
           t->irq + t->softirq + t->steal + t->guest + t->guest_nice;
}

/*
 * Sums the counters that represent "not doing useful work":
 * idle + iowait.
 */
static unsigned long long idle_time(const cpu_times_t *t) {
    return t->idle + t->iowait;
}

/* ---- public API ---- */

int cpu_read_times(cpu_times_t *times) {
    if (times == NULL) {
        fprintf(stderr, "cpu: NULL pointer passed to cpu_read_times\n");
        return -1;
    }

    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        perror("cpu: failed to open /proc/stat");
        return -1;
    }

    char line[512];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "cpu: failed to read first line of /proc/stat\n");
        fclose(fp);
        return -1;
    }
    fclose(fp);

    memset(times, 0, sizeof(cpu_times_t));

    /* Expected format: "cpu  <user> <nice> <system> <idle> <iowait>
     * <irq> <softirq> <steal> <guest> <guest_nice>"
     * Older kernels may omit steal/guest/guest_nice, so we only
     * require the first 8 fields to succeed; missing trailing
     * fields stay at 0 from the memset above. */
    int matched = sscanf(line,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &times->user, &times->nice, &times->system, &times->idle,
        &times->iowait, &times->irq, &times->softirq, &times->steal,
        &times->guest, &times->guest_nice);

    if (matched < 8) {
        fprintf(stderr, "cpu: unexpected format in /proc/stat (matched %d fields)\n", matched);
        return -1;
    }

    return 0;
}

double cpu_usage_percent(const cpu_times_t *prev, const cpu_times_t *curr) {
    if (prev == NULL || curr == NULL) return 0.0;

    unsigned long long total_prev = total_time(prev);
    unsigned long long total_curr = total_time(curr);
    unsigned long long idle_prev = idle_time(prev);
    unsigned long long idle_curr = idle_time(curr);

    /* Guard against total going "backwards" or being unchanged
     * (e.g. called too fast, or counters wrapped). Avoids division
     * by zero and nonsensical negative deltas. */
    if (total_curr <= total_prev) {
        return 0.0;
    }

    unsigned long long total_delta = total_curr - total_prev;
    unsigned long long idle_delta = idle_curr - idle_prev;

    /* idle_delta should never exceed total_delta, but guard anyway
     * so we never return a negative percentage. */
    if (idle_delta > total_delta) {
        return 0.0;
    }

    unsigned long long active_delta = total_delta - idle_delta;

    return ((double) active_delta / (double) total_delta) * 100.0;
}

int cpu_get_core_count(void) {
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores < 1) {
        perror("cpu: sysconf(_SC_NPROCESSORS_ONLN) failed");
        return -1;
    }
    return (int) cores;
}

int cpu_get_model(char *buf, size_t buflen) {
    if (buf == NULL || buflen == 0) {
        fprintf(stderr, "cpu: invalid buffer passed to cpu_get_model\n");
        return -1;
    }

    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) {
        perror("cpu: failed to open /proc/cpuinfo");
        return -1;
    }

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "model name", 10) == 0) {
            /* Format: "model name\t: <value>\n" — find the colon,
             * then skip the colon and the single space after it. */
            char *colon = strchr(line, ':');
            if (colon != NULL) {
                char *value = colon + 2; /* skip ": " */

                /* Strip the trailing newline, if present. */
                size_t len = strlen(value);
                if (len > 0 && value[len - 1] == '\n') {
                    value[len - 1] = '\0';
                }

                strncpy(buf, value, buflen - 1);
                buf[buflen - 1] = '\0';
                found = 1;
            }
            break; /* only need the first occurrence */
        }
    }

    fclose(fp);

    if (!found) {
        fprintf(stderr, "cpu: 'model name' field not found in /proc/cpuinfo\n");
        return -1;
    }
    return 0;
}

int cpu_get_frequency_mhz(double *freq_out) {
    if (freq_out == NULL) {
        fprintf(stderr, "cpu: NULL pointer passed to cpu_get_frequency_mhz\n");
        return -1;
    }

    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) {
        perror("cpu: failed to open /proc/cpuinfo");
        return -1;
    }

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "cpu MHz", 7) == 0) {
            char *colon = strchr(line, ':');
            if (colon != NULL) {
                *freq_out = strtod(colon + 1, NULL);
                found = 1;
            }
            break; /* first core's frequency is representative enough */
        }
    }

    fclose(fp);

    if (!found) {
        fprintf(stderr, "cpu: 'cpu MHz' field not found in /proc/cpuinfo\n");
        return -1;
    }
    return 0;
}

int cpu_collect(cpu_info_t *info, unsigned int sample_interval_ms) {
    if (info == NULL) {
        fprintf(stderr, "cpu: NULL pointer passed to cpu_collect\n");
        return -1;
    }

    memset(info, 0, sizeof(cpu_info_t));

    cpu_times_t before, after;

    if (cpu_read_times(&before) != 0) return -1;

    struct timespec ts = ms_to_timespec(sample_interval_ms);
    /* nanosleep can be interrupted by a signal before the full
     * duration elapses. We don't loop-retry here since a slightly
     * shorter sample window still produces a valid (if marginally
     * less accurate) reading — not worth the added complexity for
     * this project. */
    nanosleep(&ts, NULL);

    if (cpu_read_times(&after) != 0) return -1;

    info->usage_percent = cpu_usage_percent(&before, &after);

    int cores = cpu_get_core_count();
    if (cores < 0) return -1;
    info->core_count = cores;

    if (cpu_get_model(info->model_name, sizeof(info->model_name)) != 0) {
        return -1;
    }

    if (cpu_get_frequency_mhz(&info->frequency_mhz) != 0) {
        return -1;
    }

    return 0;
}

void cpu_print(const cpu_info_t *info) {
    if (info == NULL) return;

    printf("=== CPU Information ===\n");
    printf("Model        : %s\n", info->model_name);
    printf("Cores        : %d\n", info->core_count);
    printf("Frequency    : %.2f MHz\n", info->frequency_mhz);
    printf("Usage        : %.1f%%\n", info->usage_percent);
}
