/*
 * main.c
 *
 * Entry point for the Linux System Monitor.
 * Milestone 7: adds the Users module, live refresh mode, and
 * command-line argument parsing.
 *
 * sigaction() and sleep() are POSIX functions; _POSIX_C_SOURCE must
 * be defined before any system header is included so glibc exposes
 * their declarations under strict -std=c17.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "sysinfo.h"
#include "cpu.h"
#include "mem.h"
#include "disk.h"
#include "process.h"
#include "net.h"
#include "users.h"
#include "cli.h"

/* How long to sample /proc/stat (and per-process stat, and
 * /proc/net/dev) over when measuring rates. 200ms balances
 * responsiveness against noise. */
#define CPU_SAMPLE_INTERVAL_MS 200

/* ANSI escape sequence: erase entire screen, move cursor to
 * row 1 / column 1. Used between refreshes in live mode so each
 * redraw starts from a clean terminal instead of scrolling forever. */
#define ANSI_CLEAR_SCREEN "\033[2J\033[H"

/*
 * Set by handle_sigint() when the user presses Ctrl+C. Declared
 * volatile sig_atomic_t: this is the only integer type the C
 * standard guarantees can be safely read and written from inside a
 * signal handler without risk of a torn (partially-updated) value.
 */
static volatile sig_atomic_t g_running = 1;

static void handle_sigint(int signum) {
    (void) signum; /* unused, but required by the sigaction handler signature */
    g_running = 0;
}

/*
 * Installs a SIGINT (Ctrl+C) handler via sigaction() — the
 * POSIX-correct way to register a signal handler, preferred over the
 * older signal() function, whose exact semantics vary across Unix
 * implementations. Deliberately does NOT set SA_RESTART, so that a
 * blocking sleep() call is interrupted immediately when Ctrl+C
 * arrives, instead of finishing its current sleep first.
 */
static void install_sigint_handler(void) {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

/*
 * Maps the CLI's plain-char sort key ('c'/'m'/'p') to the Process
 * module's enum. Kept here, in main.c, rather than inside cli.c, so
 * the CLI module stays decoupled from process.h.
 */
static process_sort_key_t map_sort_key(char key) {
    switch (key) {
        case 'm': return PROCESS_SORT_MEM;
        case 'p': return PROCESS_SORT_PID;
        case 'c':
        default:  return PROCESS_SORT_CPU;
    }
}

/*
 * Collects and prints one full report (all modules), using the
 * settings in `config`.
 *
 * If `exit_on_failure` is nonzero, the first module that fails to
 * collect causes an immediate return of -1 with no further output —
 * appropriate for one-shot mode, where a partial/misleading report
 * is worse than a clean failure.
 *
 * If `exit_on_failure` is zero (live refresh mode), a failed module
 * logs an error to stderr but the report continues with the
 * remaining modules — a single bad sample shouldn't kill an
 * otherwise-healthy running dashboard.
 *
 * Returns 0 if every module succeeded, -1 if at least one failed.
 */
static int run_once(const monitor_config_t *config, int exit_on_failure) {
    int had_failure = 0;

    sysinfo_t sys;
    if (sysinfo_collect(&sys) != 0) {
        fprintf(stderr, "Error: could not collect system information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    } else {
        sysinfo_print(&sys);
    }

    printf("\nSampling CPU usage (%dms)...\n", CPU_SAMPLE_INTERVAL_MS);
    cpu_info_t cpu;
    if (cpu_collect(&cpu, CPU_SAMPLE_INTERVAL_MS) != 0) {
        fprintf(stderr, "Error: could not collect CPU information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    } else {
        cpu_print(&cpu);
    }

    printf("\n");
    mem_info_t mem;
    if (mem_collect(&mem) != 0) {
        fprintf(stderr, "Error: could not collect memory information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    } else {
        mem_print(&mem);
    }

    printf("\n");
    disk_info_t disk;
    if (disk_collect(&disk) != 0) {
        fprintf(stderr, "Error: could not collect disk information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    } else {
        disk_print(&disk);
    }

    printf("\nSampling processes (%dms)...\n", CPU_SAMPLE_INTERVAL_MS);
    process_info_t procs;
    if (process_collect(&procs, CPU_SAMPLE_INTERVAL_MS) != 0) {
        fprintf(stderr, "Error: could not collect process information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    } else {
        process_sort(&procs, map_sort_key(config->sort_key));
        process_print(&procs, config->process_rows);
    }

    printf("\nSampling network throughput (%dms)...\n", CPU_SAMPLE_INTERVAL_MS);
    net_info_t net;
    if (net_collect(&net, CPU_SAMPLE_INTERVAL_MS) != 0) {
        fprintf(stderr, "Error: could not collect network information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    } else {
        net_print(&net);
    }

    printf("\n");
    user_info_t users;
    if (users_collect(&users) != 0) {
        fprintf(stderr, "Error: could not collect user information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    } else {
        users_print(&users);
    }

    return had_failure ? -1 : 0;
}

int main(int argc, char *argv[]) {
    monitor_config_t config;
    int parse_result = cli_parse_args(argc, argv, &config);

    if (parse_result == 1) {
        return EXIT_SUCCESS; /* -h: usage already printed */
    }
    if (parse_result != 0) {
        return EXIT_FAILURE; /* invalid arguments: error already printed */
    }

    if (!config.refresh_enabled) {
        /* One-shot mode: a failed module is treated as fatal, since
         * a partial report with no way to retry isn't useful. */
        return (run_once(&config, /* exit_on_failure = */ 1) == 0)
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* Live refresh mode. */
    install_sigint_handler();

    int iteration = 0;
    while (g_running) {
        printf(ANSI_CLEAR_SCREEN);
        fflush(stdout);

        run_once(&config, /* exit_on_failure = */ 0);

        iteration++;
        if (config.max_iterations > 0 && iteration >= config.max_iterations) {
            break;
        }

        printf("\n(Press Ctrl+C to stop; refreshing every %us)\n",
               config.refresh_interval_sec);
        fflush(stdout);

        /* sleep() is interruptible: if SIGINT arrives mid-sleep,
         * it returns early (having set g_running = 0 via the
         * handler), and the while-loop condition catches it on the
         * next check rather than waiting out the full interval. */
        sleep(config.refresh_interval_sec);
    }

    printf("\nStopped.\n");
    return EXIT_SUCCESS;
}
