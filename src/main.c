/*
 * main.c
 *
 * Entry point for the Linux System Monitor.
 * Milestone 8 (final): adds kill-by-PID, opt-in logging, a config
 * file, and a colored status summary, on top of Milestone 7's live
 * refresh and CLI argument parsing.
 *
 * sigaction(), sleep(), and kill() are POSIX functions;
 * _POSIX_C_SOURCE must be defined before any system header is
 * included so glibc exposes their declarations under strict -std=c17.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "sysinfo.h"
#include "cpu.h"
#include "mem.h"
#include "disk.h"
#include "process.h"
#include "net.h"
#include "users.h"
#include "cli.h"
#include "config.h"
#include "log.h"
#include "color.h"

#define CPU_SAMPLE_INTERVAL_MS 200
#define ANSI_CLEAR_SCREEN "\033[2J\033[H"
#define CONFIG_FILE_PATH "sysmon.conf"

static volatile sig_atomic_t g_running = 1;

static void handle_sigint(int signum) {
    (void) signum;
    g_running = 0;
}

static void install_sigint_handler(void) {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

static process_sort_key_t map_sort_key(char key) {
    switch (key) {
        case 'm': return PROCESS_SORT_MEM;
        case 'p': return PROCESS_SORT_PID;
        case 'c':
        default:  return PROCESS_SORT_CPU;
    }
}

/*
 * Sends SIGTERM or SIGKILL to `pid`, per config->kill_force.
 * Returns 0 on success, -1 on failure (permission denied, no such
 * process, etc. -- reported via perror()).
 */
static int handle_kill_request(const monitor_config_t *config) {
    int sig = config->kill_force ? SIGKILL : SIGTERM;

    if (kill((pid_t) config->kill_pid, sig) != 0) {
        fprintf(stderr, "Error: could not send %s to PID %d: %s\n",
                config->kill_force ? "SIGKILL" : "SIGTERM",
                config->kill_pid, strerror(errno));
        return -1;
    }

    printf("Sent %s to PID %d.\n",
           config->kill_force ? "SIGKILL" : "SIGTERM", config->kill_pid);
    return 0;
}

/*
 * Prints a one-line colored status summary (CPU% / MEM% / DISK% on
 * root) above the detailed module output, using already-collected
 * data so nothing is sampled twice.
 */
static void print_status_summary(const cpu_info_t *cpu, const mem_info_t *mem,
                                  const disk_info_t *disk) {
    double disk_percent = 0.0;
    for (int i = 0; i < disk->count; i++) {
        if (strcmp(disk->entries[i].mount_point, "/") == 0) {
            disk_percent = disk->entries[i].used_percent;
            break;
        }
    }

    printf("%sStatus:%s CPU %s%.1f%%%s  MEM %s%.1f%%%s  DISK(/) %s%.1f%%%s\n\n",
           g_color_enabled ? COLOR_BOLD : "", color_reset(),
           color_for_percent(cpu->usage_percent), cpu->usage_percent, color_reset(),
           color_for_percent(mem->used_percent), mem->used_percent, color_reset(),
           color_for_percent(disk_percent), disk_percent, color_reset());
}

/*
 * Collects and prints one full report. exit_on_failure nonzero
 * (one-shot mode) aborts on the first failed module; zero (refresh
 * mode) logs and continues with the remaining modules.
 */
static int run_once(const monitor_config_t *config, int exit_on_failure) {
    int had_failure = 0;

    sysinfo_t sys;
    cpu_info_t cpu;
    mem_info_t mem;
    disk_info_t disk;

    if (sysinfo_collect(&sys) != 0) {
        fprintf(stderr, "Error: could not collect system information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    }

    if (cpu_collect(&cpu, CPU_SAMPLE_INTERVAL_MS) != 0) {
        fprintf(stderr, "Error: could not collect CPU information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    }

    if (mem_collect(&mem) != 0) {
        fprintf(stderr, "Error: could not collect memory information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    }

    if (disk_collect(&disk) != 0) {
        fprintf(stderr, "Error: could not collect disk information.\n");
        had_failure = 1;
        if (exit_on_failure) return -1;
    }

    if (!had_failure) {
        print_status_summary(&cpu, &mem, &disk);
    }

    sysinfo_print(&sys);
    printf("\n");
    cpu_print(&cpu);
    printf("\n");
    mem_print(&mem);
    printf("\n");
    disk_print(&disk);

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

    log_write("cpu=%.1f%% mem=%.1f%% procs=%d net_ifaces=%d users=%d %s",
              cpu.usage_percent, mem.used_percent, procs.count, net.count,
              users.count, had_failure ? "(partial)" : "");

    return had_failure ? -1 : 0;
}

int main(int argc, char *argv[]) {
    monitor_config_t config;
    cli_set_defaults(&config);

    /* Auto-detect color support before the config file / CLI flags
     * get a chance to override it. */
    g_color_enabled = color_detect_tty();

    /* Config file is optional; a missing sysmon.conf is expected and
     * silently ignored -- only explicit keys inside it take effect. */
    config_load_file(CONFIG_FILE_PATH, &config);

    int parse_result = cli_parse_args(argc, argv, &config);
    if (parse_result == 1) {
        return EXIT_SUCCESS; /* -h: usage already printed */
    }
    if (parse_result != 0) {
        return EXIT_FAILURE; /* invalid arguments: error already printed */
    }

    /* -x always wins last, regardless of tty detection or config file. */
    if (config.no_color) {
        g_color_enabled = 0;
    }

    /* Kill-by-PID is a standalone action: perform it and exit,
     * without running the monitor. */
    if (config.kill_pid != 0) {
        return (handle_kill_request(&config) == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (config.log_path[0] != '\0') {
        if (log_open(config.log_path) != 0) {
            return EXIT_FAILURE;
        }
    }

    int exit_code;

    if (!config.refresh_enabled) {
        exit_code = (run_once(&config, /* exit_on_failure = */ 1) == 0)
            ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
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

            sleep(config.refresh_interval_sec);
        }

        printf("\nStopped.\n");
        exit_code = EXIT_SUCCESS;
    }

    log_close();
    return exit_code;
}
