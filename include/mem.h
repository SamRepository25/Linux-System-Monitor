#ifndef MEM_H
#define MEM_H

/*
 * mem.h
 *
 * Public interface for the Memory module.
 * Collects RAM and swap usage from /proc/meminfo.
 *
 * All raw fields are stored in kilobytes (kB), matching the native
 * unit of /proc/meminfo, so no precision is lost in the collection
 * step. Conversion to human-readable MB/GB happens only at print time.
 */

typedef struct {
    unsigned long total_kb;         /* MemTotal */
    unsigned long free_kb;          /* MemFree */
    unsigned long available_kb;     /* MemAvailable */
    unsigned long buffers_kb;       /* Buffers */
    unsigned long cached_kb;        /* Cached */
    unsigned long used_kb;          /* derived: total - available */
    double used_percent;            /* derived: used / total * 100 */

    unsigned long swap_total_kb;    /* SwapTotal */
    unsigned long swap_free_kb;     /* SwapFree */
    unsigned long swap_used_kb;     /* derived: swap_total - swap_free */
    double swap_used_percent;       /* derived: swap_used / swap_total * 100 */
} mem_info_t;

/*
 * Reads /proc/meminfo and fills `info` with RAM and swap statistics.
 * Returns 0 on success, -1 on failure.
 */
int mem_collect(mem_info_t *info);

/*
 * Prints a formatted summary of `info` to stdout, in human-readable
 * MB/GB units.
 */
void mem_print(const mem_info_t *info);

#endif /* MEM_H */
