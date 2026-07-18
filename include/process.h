#ifndef PROCESS_H
#define PROCESS_H

/*
 * process.h
 *
 * Public interface for the Process module.
 * Enumerates running processes via /proc/[pid]/, computes per-process
 * CPU% (via two time-sampled snapshots) and memory% (via VmRSS),
 * and provides sorting and searching over the resulting list.
 */

#define PROCESS_MAX_ENTRIES 2048
#define PROCESS_NAME_LEN    256

typedef struct {
    int pid;
    char name[PROCESS_NAME_LEN]; /* from /proc/[pid]/status "Name:" */
    char state;                  /* 'R' running, 'S' sleeping, 'Z' zombie, etc. */
    double cpu_percent;          /* 100% == one full core fully busy */
    unsigned long mem_kb;        /* VmRSS: resident memory actually in use */
    double mem_percent;          /* mem_kb / total system RAM * 100 */
} process_entry_t;

typedef struct {
    process_entry_t entries[PROCESS_MAX_ENTRIES];
    int count;
} process_info_t;

/* Keys process_sort() can sort by. */
typedef enum {
    PROCESS_SORT_CPU,  /* descending: highest CPU% first */
    PROCESS_SORT_MEM,  /* descending: highest MEM% first */
    PROCESS_SORT_PID   /* ascending: lowest PID first */
} process_sort_key_t;

/*
 * Scans /proc for all running processes and fills `info`.
 *
 * Internally takes two CPU-time snapshots `sample_interval_ms`
 * milliseconds apart (same idea as cpu_collect()) to compute each
 * process's CPU%. Processes that exit mid-scan are simply omitted
 * from the results rather than causing a failure.
 *
 * Returns 0 on success, -1 on failure (e.g. /proc unreadable).
 */
int process_collect(process_info_t *info, unsigned int sample_interval_ms);

/*
 * Sorts `info->entries` in place according to `key`.
 */
void process_sort(process_info_t *info, process_sort_key_t key);

/*
 * Searches `info` for processes whose name contains `query` as a
 * case-insensitive substring. Matches are copied into `*results_out`
 * (results_out->count reflects the number found). `results_out` must
 * point to a valid, separate process_info_t — it is fully overwritten.
 *
 * Returns the number of matches found (0 or more), or -1 on invalid
 * arguments.
 */
int process_search(const process_info_t *info, const char *query,
                    process_info_t *results_out);

/*
 * Prints a formatted table of up to `max_rows` entries from `info`
 * to stdout (pass a large number, e.g. info->count, to print all).
 */
void process_print(const process_info_t *info, int max_rows);

#endif /* PROCESS_H */
