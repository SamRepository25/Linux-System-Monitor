#define _POSIX_C_SOURCE 200809L

#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* getopt, optarg, optind, optopt */

/*
 * Parses a decimal string into `out`. Returns 0 on success, -1 if
 * the string isn't a valid non-negative integer.
 */
static int parse_uint(const char *str, int *out) {
    char *endptr;
    long value = strtol(str, &endptr, 10);

    if (endptr == str || *endptr != '\0' || value < 0) {
        return -1;
    }
    *out = (int) value;
    return 0;
}

void cli_set_defaults(monitor_config_t *config) {
    if (config == NULL) return;

    config->refresh_enabled = 0;
    config->refresh_interval_sec = CLI_DEFAULT_REFRESH_SEC;
    config->max_iterations = 0;
    config->process_rows = CLI_DEFAULT_PROCESS_ROWS;
    config->sort_key = CLI_DEFAULT_SORT_KEY;

    config->kill_pid = 0;
    config->kill_force = 0;

    config->log_path[0] = '\0';
    config->no_color = 0;
}

int cli_parse_args(int argc, char *argv[], monitor_config_t *config) {
    if (config == NULL) {
        return -1;
    }

    int opt;
    /* Leading ':' makes getopt distinguish "missing argument" (':')
     * from "unknown option" ('?'). */
    while ((opt = getopt(argc, argv, ":r:n:t:s:k:K:l:xh")) != -1) {
        switch (opt) {
            case 'r': {
                int value;
                if (parse_uint(optarg, &value) != 0 || value <= 0) {
                    fprintf(stderr, "Error: -r requires a positive integer (seconds)\n");
                    return -1;
                }
                config->refresh_enabled = 1;
                config->refresh_interval_sec = (unsigned int) value;
                break;
            }
            case 'n': {
                int value;
                if (parse_uint(optarg, &value) != 0) {
                    fprintf(stderr, "Error: -n requires a non-negative integer\n");
                    return -1;
                }
                config->max_iterations = value;
                break;
            }
            case 't': {
                int value;
                if (parse_uint(optarg, &value) != 0 || value <= 0) {
                    fprintf(stderr, "Error: -t requires a positive integer (row count)\n");
                    return -1;
                }
                config->process_rows = value;
                break;
            }
            case 's': {
                if (strlen(optarg) != 1 ||
                    (optarg[0] != 'c' && optarg[0] != 'm' && optarg[0] != 'p')) {
                    fprintf(stderr, "Error: -s must be one of: c (cpu), m (mem), p (pid)\n");
                    return -1;
                }
                config->sort_key = optarg[0];
                break;
            }
            case 'k': {
                int value;
                if (parse_uint(optarg, &value) != 0 || value <= 0) {
                    fprintf(stderr, "Error: -k requires a positive PID\n");
                    return -1;
                }
                config->kill_pid = value;
                config->kill_force = 0; /* SIGTERM */
                break;
            }
            case 'K': {
                int value;
                if (parse_uint(optarg, &value) != 0 || value <= 0) {
                    fprintf(stderr, "Error: -K requires a positive PID\n");
                    return -1;
                }
                config->kill_pid = value;
                config->kill_force = 1; /* SIGKILL */
                break;
            }
            case 'l': {
                strncpy(config->log_path, optarg, CLI_LOG_PATH_LEN - 1);
                config->log_path[CLI_LOG_PATH_LEN - 1] = '\0';
                break;
            }
            case 'x':
                config->no_color = 1;
                break;
            case 'h':
                cli_print_usage(argv[0]);
                return 1;
            case ':':
                fprintf(stderr, "Error: option -%c requires an argument\n", optopt);
                return -1;
            case '?':
            default:
                fprintf(stderr, "Error: unknown option -%c\n", optopt);
                cli_print_usage(argv[0]);
                return -1;
        }
    }

    return 0;
}

void cli_print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\n");
    printf("A terminal-based Linux system monitor.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -r SECONDS   Enable live refresh, redrawing every SECONDS (default: one-shot)\n");
    printf("  -n COUNT     Stop after COUNT refreshes (default: 0, run until Ctrl+C)\n");
    printf("  -t ROWS      Number of process rows to display (default: %d)\n",
           CLI_DEFAULT_PROCESS_ROWS);
    printf("  -s KEY       Sort processes by: c=cpu, m=mem, p=pid (default: c)\n");
    printf("  -k PID       Send SIGTERM to PID, then exit\n");
    printf("  -K PID       Send SIGKILL to PID, then exit\n");
    printf("  -l FILE      Append timestamped run summaries to FILE\n");
    printf("  -x           Disable colored output\n");
    printf("  -h           Show this help message and exit\n");
    printf("\n");
    printf("A 'sysmon.conf' file in the current directory (key=value, '#' comments)\n");
    printf("is loaded automatically if present. Recognized keys: refresh, rows, sort, color.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                  # one-shot report\n", prog_name);
    printf("  %s -r 2             # live refresh every 2 seconds until Ctrl+C\n", prog_name);
    printf("  %s -k 1234          # gracefully terminate PID 1234\n", prog_name);
    printf("  %s -r 5 -l run.log  # live refresh, logging each cycle to run.log\n", prog_name);
}
