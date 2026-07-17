#ifndef SYSINFO_H
#define SYSINFO_H

/*
 * sysinfo.h
 *
 * Public interface for the System Information module.
 * Collects static and semi-static system identity data:
 *   - hostname, kernel version, architecture (via uname())
 *   - uptime (via /proc/uptime)
 *   - load average (via /proc/loadavg)
 *
 * Design note: we use a fixed-size struct instead of dynamic
 * allocation. All fields have known maximum lengths (Linux
 * guarantees this for uname() fields), so there's no need for
 * malloc/free here — simpler and leak-free.
 */

#include <sys/utsname.h> /* for UTSNAME_LENGTH-ish sizing, portability */

#define SYSINFO_STR_LEN 65 /* Linux uname() fields are max 64 chars + NUL */

typedef struct {
    char hostname[SYSINFO_STR_LEN];
    char kernel_version[SYSINFO_STR_LEN];
    char architecture[SYSINFO_STR_LEN];

    long uptime_seconds;   /* whole seconds the system has been up */
    long idle_seconds;     /* cumulative idle time, summed across cores */

    double load_avg_1;     /* average running+waiting processes, last 1 min */
    double load_avg_5;     /* last 5 min */
    double load_avg_15;    /* last 15 min */
} sysinfo_t;

/*
 * Populates `info` with current system information.
 *
 * Returns 0 on success, -1 on failure (and prints an error via perror()).
 * On failure, fields in `info` are left in an undefined state — caller
 * should not use `info` if this returns -1.
 */
int sysinfo_collect(sysinfo_t *info);

/*
 * Prints a formatted summary of `info` to stdout.
 */
void sysinfo_print(const sysinfo_t *info);

#endif /* SYSINFO_H */
