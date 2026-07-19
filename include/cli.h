#ifndef CLI_H
#define CLI_H

/*
 * cli.h
 *
 * Public interface for command-line argument parsing.
 *
 * Precedence, low to high: built-in defaults -> config file -> CLI flags.
 * Usage in main.c:
 *   cli_set_defaults(&config);
 *   config_load_file("sysmon.conf", &config);   // optional, missing is fine
 *   cli_parse_args(argc, argv, &config);         // overrides on top
 *
 * cli_parse_args() does NOT reset `config` -- it only overwrites
 * fields for flags actually present on the command line, so the
 * config-file values above survive untouched otherwise.
 */

#define CLI_LOG_PATH_LEN         256
#define CLI_DEFAULT_PROCESS_ROWS  10
#define CLI_DEFAULT_REFRESH_SEC    2
#define CLI_DEFAULT_SORT_KEY     'c'

typedef struct {
    int refresh_enabled;              /* 1 if -r was given */
    unsigned int refresh_interval_sec;
    int max_iterations;               /* 0 = unlimited (Ctrl+C to stop) */
    int process_rows;
    char sort_key;                    /* 'c' cpu, 'm' mem, 'p' pid */

    int kill_pid;                     /* 0 = no kill requested, else target PID */
    int kill_force;                   /* 1 = SIGKILL (-K), 0 = SIGTERM (-k) */

    char log_path[CLI_LOG_PATH_LEN];  /* empty = logging disabled */
    int no_color;                     /* 1 = force-disable colored output (-x) */
} monitor_config_t;

/*
 * Populates `config` with built-in defaults. Must be called before
 * config_load_file() and cli_parse_args().
 */
void cli_set_defaults(monitor_config_t *config);

/*
 * Return values:
 *   0  -> parsed successfully, config is ready to use
 *   1  -> help was requested (-h); usage already printed,
 *         caller should exit with EXIT_SUCCESS
 *  -1  -> invalid arguments; an error message already printed,
 *         caller should exit with EXIT_FAILURE
 */
int cli_parse_args(int argc, char *argv[], monitor_config_t *config);

/* Prints usage/help text to stdout. */
void cli_print_usage(const char *prog_name);

#endif /* CLI_H */
