/*
 * cli.c
 *
 * Implementation of command-line argument parsing using getopt(),
 * the standard POSIX facility for this rather than hand-rolling
 * argv parsing.
 */
#define _POSIX_C_SOURCE 200809L

#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* getopt, optarg, optind */

/*
 * Parses a decimal string into `out`. Returns 0 on success, -1 if
 * the string isn't a valid non-negative integer (using strtol with
 * full error checking rather than atoi, which silently returns 0
 * for garbage input and can't distinguish "0" from "not a number").
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

int cli_parse_args(int argc, char *argv[], monitor_config_t *config) {
    if (config == NULL) {
        return -1;
    }

    /* Defaults: one-shot mode, top 10 processes, sorted by CPU. */
    config->refresh_enabled = 0;
    config->refresh_interval_sec = CLI_DEFAULT_REFRESH_SEC;
    config->max_iterations = 0;
    config->process_rows = CLI_DEFAULT_PROCESS_ROWS;
    config->sort_key = CLI_DEFAULT_SORT_KEY;

    int opt;
    /* Leading ':' makes getopt distinguish "missing argument" (':')
     * from "unknown option" ('?') via its return value, so we can
     * give more precise error messages below. */
    while ((opt = getopt(argc, argv, ":r:n:t:s:h")) != -1) {
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
    printf("  -h           Show this help message and exit\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                  # one-shot report\n", prog_name);
    printf("  %s -r 2             # live refresh every 2 seconds until Ctrl+C\n", prog_name);
    printf("  %s -r 1 -n 10       # live refresh every second, stop after 10 updates\n", prog_name);
    printf("  %s -t 20 -s m       # one-shot report, top 20 processes by memory\n", prog_name);
}
