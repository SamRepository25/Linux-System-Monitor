#ifndef CPU_H
#define CPU_H

/*
 * cpu.h
 *
 * Public interface for the CPU module.
 * Collects:
 *   - overall CPU usage % (via two /proc/stat samples over an interval)
 *   - core count (via sysconf)
 *   - CPU model name (via /proc/cpuinfo)
 *   - current CPU frequency in MHz (via /proc/cpuinfo)
 */

#include <stddef.h>

#define CPU_MODEL_STR_LEN 128

/*
 * Raw cumulative CPU time counters read from the aggregate "cpu" line
 * of /proc/stat, in jiffies (units don't matter — we only use ratios
 * of deltas between two snapshots).
 */
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
    unsigned long long guest;
    unsigned long long guest_nice;
} cpu_times_t;

/*
 * Final, display-ready CPU information.
 */
typedef struct {
    double usage_percent;                  /* 0.0 - 100.0 */
    int core_count;                        /* number of online logical cores */
    char model_name[CPU_MODEL_STR_LEN];    /* e.g. "Intel(R) Core(TM) i5-13450HX" */
    double frequency_mhz;                  /* current clock speed of first core */
} cpu_info_t;

/*
 * Reads the aggregate "cpu" line from /proc/stat into `times`.
 * Returns 0 on success, -1 on failure.
 */
int cpu_read_times(cpu_times_t *times);

/*
 * Computes CPU usage percentage from two time snapshots.
 * `prev` must have been read strictly before `curr`.
 * Returns a value in [0.0, 100.0]. Returns 0.0 if the total delta
 * between snapshots is zero (e.g. called with identical snapshots).
 */
double cpu_usage_percent(const cpu_times_t *prev, const cpu_times_t *curr);

/*
 * Returns the number of online logical CPU cores, or -1 on failure.
 */
int cpu_get_core_count(void);

/*
 * Copies the CPU model name into `buf` (size `buflen`).
 * Returns 0 on success, -1 on failure (e.g. field not found).
 */
int cpu_get_model(char *buf, size_t buflen);

/*
 * Reads the current clock frequency (MHz) of the first core into
 * `freq_out`. Returns 0 on success, -1 on failure.
 */
int cpu_get_frequency_mhz(double *freq_out);

/*
 * Orchestrates a full CPU info collection:
 *   1. take a /proc/stat snapshot
 *   2. sleep for `sample_interval_ms` milliseconds
 *   3. take a second snapshot
 *   4. compute usage %, core count, model, frequency
 * Fills `info`. Returns 0 on success, -1 on failure.
 */
int cpu_collect(cpu_info_t *info, unsigned int sample_interval_ms);

/*
 * Prints a formatted summary of `info` to stdout.
 */
void cpu_print(const cpu_info_t *info);

#endif /* CPU_H */
