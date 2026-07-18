#ifndef CLI_H
#define CLI_H

/*
 * cli.h
 *
 * Public interface for command-line argument parsing.
 * Populates a monitor_config_t describing how src/main.c should run:
 * one-shot vs. live refresh, how many process rows to show, and how
 * to sort them. Deliberately has no dependency on process.h — the
 * sort key is stored as a plain char and mapped to
 * process_sort_key_t by the caller (src/main.c), keeping this module
 * decoupled from the Process module's internals.
 */

#define CLI_DEFAULT_PROCESS_ROWS   10
#define CLI_DEFAULT_REFRESH_SEC     2
#define CLI_DEFAULT_SORT_KEY      'c'

typedef struct {
    int refresh_enabled;              /* 1 if -r was given */
    unsigned int refresh_interval_sec;
    int max_iterations;               /* 0 = unlimited (Ctrl+C to stop) */
    int process_rows;
    char sort_key;                    /* 'c' cpu, 'm' mem, 'p' pid */
} monitor_config_t;

/*
 * Return values for cli_parse_args():
 *   0  -> parsed successfully, config is ready to use
 *   1  -> help was requested (-h); usage was already printed,
 *         caller should exit with EXIT_SUCCESS
 *  -1  -> invalid arguments; an error message was already printed,
 *         caller should exit with EXIT_FAILURE
 */
int cli_parse_args(int argc, char *argv[], monitor_config_t *config);

/*
 * Prints usage/help text to stdout.
 */
void cli_print_usage(const char *prog_name);

#endif /* CLI_H */
