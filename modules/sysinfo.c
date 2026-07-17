/*
 * sysinfo.c
 *
 * Implementation of the System Information module.
 */

#include "sysinfo.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>

/* ---- static (file-private) helper functions ---- */

/*
 * Fills hostname, kernel_version, architecture using the uname() syscall.
 * Returns 0 on success, -1 on failure.
 */
static int collect_uname_fields(sysinfo_t *info) {
    struct utsname u;

    /* uname() is a single syscall that fills a struct with system
     * identity fields. It returns 0 on success, -1 on error (and
     * sets errno). This is far simpler than parsing /proc for the
     * same info, and it's the POSIX-correct way to get it. */
    if (uname(&u) != 0) {
        perror("sysinfo: uname() failed");
        return -1;
    }

    /* strncpy + explicit NUL termination: uname() fields are already
     * NUL-terminated and guaranteed <= 64 chars on Linux, but we
     * defend defensively anyway. This is a habit interviewers look for:
     * never trust a buffer is terminated without proof. */
    strncpy(info->hostname, u.nodename, SYSINFO_STR_LEN - 1);
    info->hostname[SYSINFO_STR_LEN - 1] = '\0';

    strncpy(info->kernel_version, u.release, SYSINFO_STR_LEN - 1);
    info->kernel_version[SYSINFO_STR_LEN - 1] = '\0';

    strncpy(info->architecture, u.machine, SYSINFO_STR_LEN - 1);
    info->architecture[SYSINFO_STR_LEN - 1] = '\0';

    return 0;
}

/*
 * Reads /proc/uptime and fills uptime_seconds and idle_seconds.
 * Format: "<uptime> <idle>\n" both as floating point seconds.
 */
static int collect_uptime(sysinfo_t *info) {
    FILE *fp = fopen("/proc/uptime", "r");
    if (fp == NULL) {
        perror("sysinfo: failed to open /proc/uptime");
        return -1;
    }

    double uptime = 0.0, idle = 0.0;
    /* fscanf returns the number of successfully matched items.
     * We expect exactly 2 (uptime and idle). If we get fewer,
     * the file format was not what we expected — treat as error
     * rather than silently using garbage/zeroed values. */
    int matched = fscanf(fp, "%lf %lf", &uptime, &idle);
    fclose(fp);

    if (matched != 2) {
        fprintf(stderr, "sysinfo: unexpected format in /proc/uptime\n");
        return -1;
    }

    info->uptime_seconds = (long) uptime;
    info->idle_seconds = (long) idle;
    return 0;
}

/*
 * Reads /proc/loadavg and fills load_avg_1/5/15.
 * Format: "<1min> <5min> <15min> <running>/<total> <last_pid>\n"
 * We only need the first three fields.
 */
static int collect_loadavg(sysinfo_t *info) {
    FILE *fp = fopen("/proc/loadavg", "r");
    if (fp == NULL) {
        perror("sysinfo: failed to open /proc/loadavg");
        return -1;
    }

    int matched = fscanf(fp, "%lf %lf %lf",
                          &info->load_avg_1,
                          &info->load_avg_5,
                          &info->load_avg_15);
    fclose(fp);

    if (matched != 3) {
        fprintf(stderr, "sysinfo: unexpected format in /proc/loadavg\n");
        return -1;
    }

    return 0;
}

/* ---- public API ---- */

int sysinfo_collect(sysinfo_t *info) {
    if (info == NULL) {
        fprintf(stderr, "sysinfo: NULL pointer passed to sysinfo_collect\n");
        return -1;
    }

    /* Zero the struct first so any field we fail to set is at least
     * in a predictable (zeroed) state rather than uninitialized memory. */
    memset(info, 0, sizeof(sysinfo_t));

    if (collect_uname_fields(info) != 0) return -1;
    if (collect_uptime(info) != 0) return -1;
    if (collect_loadavg(info) != 0) return -1;

    return 0;
}

void sysinfo_print(const sysinfo_t *info) {
    if (info == NULL) return;

    long hours = info->uptime_seconds / 3600;
    long minutes = (info->uptime_seconds % 3600) / 60;
    long seconds = info->uptime_seconds % 60;

    printf("=== System Information ===\n");
    printf("Hostname     : %s\n", info->hostname);
    printf("Kernel       : %s\n", info->kernel_version);
    printf("Architecture : %s\n", info->architecture);
    printf("Uptime       : %ldh %ldm %lds\n", hours, minutes, seconds);
    printf("Load Average : %.2f, %.2f, %.2f (1m, 5m, 15m)\n",
           info->load_avg_1, info->load_avg_5, info->load_avg_15);
}
