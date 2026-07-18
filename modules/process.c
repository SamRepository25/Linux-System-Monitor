/*
 * process.c
 *
 * Implementation of the Process module.
 *
 * opendir/readdir, clock_gettime, and nanosleep are POSIX functions;
 * _POSIX_C_SOURCE must be defined before any system header is
 * included so glibc exposes their declarations under strict -std=c17.
 */
#define _POSIX_C_SOURCE 200809L

#include "process.h"
#include "mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

/* ---- internal snapshot type (not exposed in the header) ---- */

/*
 * Raw CPU-time snapshot for one process, taken directly from
 * /proc/[pid]/stat. Kept separate from process_entry_t since it's
 * only needed transiently during collection, not part of the
 * public result.
 */
typedef struct {
    int pid;
    unsigned long utime;
    unsigned long stime;
    int valid; /* 0 if this pid's stat couldn't be read this round */
} cpu_snapshot_t;

/* ---- static (file-private) helper functions ---- */

/*
 * Returns 1 if `name` is a non-empty string of only digits (i.e. a
 * valid PID directory name under /proc), 0 otherwise.
 */
static int is_pid_dir(const char *name) {
    if (name == NULL || name[0] == '\0') return 0;
    for (const char *p = name; *p != '\0'; p++) {
        if (!isdigit((unsigned char) *p)) return 0;
    }
    return 1;
}

/*
 * Reads /proc/[pid]/stat and extracts state, utime, and stime.
 * Handles the "(comm)" field correctly by locating the LAST ')' in
 * the line, since the process name itself may contain spaces or
 * parentheses.
 * Returns 0 on success, -1 on failure (e.g. process has exited).
 */
static int read_stat_times(int pid, char *state_out,
                            unsigned long *utime_out, unsigned long *stime_out) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1; /* most likely: process exited between scan and read */
    }

    char line[1024];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char *last_paren = strrchr(line, ')');
    if (last_paren == NULL) {
        return -1; /* malformed line */
    }

    /* Fields after "(comm)", in order: state(3) ppid(4) pgrp(5)
     * session(6) tty_nr(7) tpgid(8) flags(9) minflt(10) cminflt(11)
     * majflt(12) cmajflt(13) utime(14) stime(15). We only need
     * state, utime, and stime, so the rest are discarded with %*d/%*u.
     * Note: suppressed (%*) fields intentionally omit length
     * modifiers (%*u, not %*lu) — GCC's format checker flags
     * assignment-suppression combined with a length modifier as
     * non-portable, and since nothing is stored for these fields
     * anyway, the modifier makes no functional difference. */
    int matched = sscanf(last_paren + 1,
        " %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
        state_out, utime_out, stime_out);

    return (matched == 3) ? 0 : -1;
}

/*
 * Reads /proc/[pid]/status and extracts the process name and
 * resident memory (VmRSS, in kB). If VmRSS is absent (some kernel
 * threads report none), mem_kb_out is left at 0 — not an error.
 * Returns 0 on success, -1 if the file couldn't be read at all.
 */
static int read_status_fields(int pid, char *name_out, size_t name_len,
                               unsigned long *mem_kb_out) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    name_out[0] = '\0';
    *mem_kb_out = 0;

    char line[256];
    int have_name = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (!have_name && strncmp(line, "Name:", 5) == 0) {
            /* "%<n>[^\n]" reads everything up to (not including) the
             * newline, skipping the leading whitespace that scanf's
             * bare space in the format string consumes. */
            char buf[PROCESS_NAME_LEN];
            if (sscanf(line, "Name: %255[^\n]", buf) == 1) {
                strncpy(name_out, buf, name_len - 1);
                name_out[name_len - 1] = '\0';
                have_name = 1;
            }
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %lu kB", mem_kb_out);
            break; /* VmRSS appears after Name; safe to stop here */
        }
    }

    fclose(fp);

    if (!have_name) {
        strncpy(name_out, "?", name_len - 1);
        name_out[name_len - 1] = '\0';
    }
    return 0;
}

/*
 * Case-insensitive substring search: returns 1 if `needle` occurs
 * anywhere in `haystack`, 0 otherwise. Written manually (rather than
 * relying on the GNU extension strcasestr) to stay portable under
 * strict -std=c17.
 */
static int contains_case_insensitive(const char *haystack, const char *needle) {
    size_t hay_len = strlen(haystack);
    size_t needle_len = strlen(needle);

    if (needle_len == 0) return 1;
    if (needle_len > hay_len) return 0;

    for (size_t i = 0; i + needle_len <= hay_len; i++) {
        size_t j = 0;
        while (j < needle_len &&
               tolower((unsigned char) haystack[i + j]) ==
               tolower((unsigned char) needle[j])) {
            j++;
        }
        if (j == needle_len) return 1;
    }
    return 0;
}

/* qsort comparators — each returns <0, 0, >0 for descending order
 * on the relevant field (ascending for PID). */

static int cmp_by_cpu_desc(const void *a, const void *b) {
    const process_entry_t *pa = (const process_entry_t *) a;
    const process_entry_t *pb = (const process_entry_t *) b;
    if (pa->cpu_percent > pb->cpu_percent) return -1;
    if (pa->cpu_percent < pb->cpu_percent) return 1;
    return 0;
}

static int cmp_by_mem_desc(const void *a, const void *b) {
    const process_entry_t *pa = (const process_entry_t *) a;
    const process_entry_t *pb = (const process_entry_t *) b;
    if (pa->mem_percent > pb->mem_percent) return -1;
    if (pa->mem_percent < pb->mem_percent) return 1;
    return 0;
}

static int cmp_by_pid_asc(const void *a, const void *b) {
    const process_entry_t *pa = (const process_entry_t *) a;
    const process_entry_t *pb = (const process_entry_t *) b;
    return pa->pid - pb->pid;
}

/* ---- public API ---- */

int process_collect(process_info_t *info, unsigned int sample_interval_ms) {
    if (info == NULL) {
        fprintf(stderr, "process: NULL pointer passed to process_collect\n");
        return -1;
    }

    memset(info, 0, sizeof(process_info_t));

    /* Total system RAM is needed to compute mem_percent per process.
     * Reusing the Memory module here is a deliberate cross-module
     * dependency: it avoids re-implementing /proc/meminfo parsing,
     * at the small cost of re-reading that file once more. */
    mem_info_t mem;
    if (mem_collect(&mem) != 0) {
        fprintf(stderr, "process: failed to determine total system RAM\n");
        return -1;
    }

    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("process: failed to open /proc");
        return -1;
    }

    /* Snapshot 1: scan /proc once for all current PIDs and record
     * their CPU time "before" values. Declared static so this large
     * array lives in the BSS segment rather than on the stack. */
    static cpu_snapshot_t before[PROCESS_MAX_ENTRIES];
    int pid_count = 0;

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (!is_pid_dir(entry->d_name)) continue;
        if (pid_count >= PROCESS_MAX_ENTRIES) break;

        int pid = atoi(entry->d_name);
        char state;
        unsigned long utime, stime;

        if (read_stat_times(pid, &state, &utime, &stime) == 0) {
            before[pid_count].pid = pid;
            before[pid_count].utime = utime;
            before[pid_count].stime = stime;
            before[pid_count].valid = 1;
            pid_count++;
        }
    }
    closedir(proc_dir);

    /* Measure real elapsed time around the sleep, rather than
     * trusting the requested interval, since nanosleep() can return
     * early if interrupted by a signal. */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    struct timespec sleep_ts;
    sleep_ts.tv_sec = (time_t) (sample_interval_ms / 1000);
    sleep_ts.tv_nsec = (long) (sample_interval_ms % 1000) * 1000000L;
    nanosleep(&sleep_ts, NULL);

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed_seconds = (double) (t_end.tv_sec - t_start.tv_sec) +
                              (double) (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    if (elapsed_seconds <= 0.0) elapsed_seconds = (double) sample_interval_ms / 1000.0;

    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) clk_tck = 100; /* conventional Linux default, safe fallback */

    double elapsed_ticks = elapsed_seconds * (double) clk_tck;

    /* Snapshot 2: re-read the SAME pids' /proc/[pid]/stat. Any pid
     * that has exited in the meantime simply fails read_stat_times()
     * and is skipped from the final results. */
    for (int i = 0; i < pid_count; i++) {
        if (!before[i].valid) continue;

        int pid = before[i].pid;
        char state;
        unsigned long utime_after, stime_after;

        if (read_stat_times(pid, &state, &utime_after, &stime_after) != 0) {
            continue; /* process exited between snapshots — omit it */
        }

        unsigned long utime_delta = (utime_after >= before[i].utime)
            ? utime_after - before[i].utime : 0;
        unsigned long stime_delta = (stime_after >= before[i].stime)
            ? stime_after - before[i].stime : 0;
        unsigned long total_delta = utime_delta + stime_delta;

        double cpu_percent = (elapsed_ticks > 0.0)
            ? ((double) total_delta / elapsed_ticks) * 100.0
            : 0.0;

        char name[PROCESS_NAME_LEN];
        unsigned long mem_kb;
        if (read_status_fields(pid, name, sizeof(name), &mem_kb) != 0) {
            continue; /* process exited even more recently — omit it */
        }

        double mem_percent = (mem.total_kb > 0)
            ? ((double) mem_kb / (double) mem.total_kb) * 100.0
            : 0.0;

        if (info->count >= PROCESS_MAX_ENTRIES) break;

        process_entry_t *out = &info->entries[info->count];
        out->pid = pid;
        strncpy(out->name, name, PROCESS_NAME_LEN - 1);
        out->name[PROCESS_NAME_LEN - 1] = '\0';
        out->state = state;
        out->cpu_percent = cpu_percent;
        out->mem_kb = mem_kb;
        out->mem_percent = mem_percent;
        info->count++;
    }

    return 0;
}

void process_sort(process_info_t *info, process_sort_key_t key) {
    if (info == NULL || info->count <= 1) return;

    int (*cmp)(const void *, const void *);
    switch (key) {
        case PROCESS_SORT_CPU: cmp = cmp_by_cpu_desc; break;
        case PROCESS_SORT_MEM: cmp = cmp_by_mem_desc; break;
        case PROCESS_SORT_PID: cmp = cmp_by_pid_asc;  break;
        default: return;
    }

    qsort(info->entries, (size_t) info->count, sizeof(process_entry_t), cmp);
}

int process_search(const process_info_t *info, const char *query,
                    process_info_t *results_out) {
    if (info == NULL || query == NULL || results_out == NULL) {
        return -1;
    }

    memset(results_out, 0, sizeof(process_info_t));

    for (int i = 0; i < info->count; i++) {
        if (contains_case_insensitive(info->entries[i].name, query)) {
            if (results_out->count >= PROCESS_MAX_ENTRIES) break;
            results_out->entries[results_out->count] = info->entries[i];
            results_out->count++;
        }
    }

    return results_out->count;
}

void process_print(const process_info_t *info, int max_rows) {
    if (info == NULL) return;

    printf("=== Processes ===\n");
    printf("%-8s %-6s %-24s %8s %8s %10s\n",
           "PID", "STATE", "NAME", "CPU%", "MEM%", "RSS");

    int rows = (max_rows < info->count) ? max_rows : info->count;
    for (int i = 0; i < rows; i++) {
        const process_entry_t *p = &info->entries[i];
        double rss_mb = (double) p->mem_kb / 1024.0;

        printf("%-8d %-6c %-24s %7.1f%% %7.1f%% %8.1f MB\n",
               p->pid, p->state, p->name, p->cpu_percent, p->mem_percent, rss_mb);
    }
}
