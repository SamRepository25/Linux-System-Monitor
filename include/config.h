#ifndef CONFIG_H
#define CONFIG_H

#include "cli.h"

/*
 * Reads a simple "key=value" config file (one per line, blank lines
 * and '#' comments allowed) and applies recognized keys onto
 * `config`. Recognized keys: refresh, rows, sort, color (on/off).
 * Unknown keys are silently ignored (forward-compatible).
 *
 * `config` must already be populated with defaults (via
 * cli_set_defaults()) before calling this — only recognized keys
 * present in the file are overridden.
 *
 * Returns 0 if the file was opened and read, -1 if it couldn't be
 * opened (e.g. doesn't exist) — a missing config file is expected
 * and not treated as fatal by the caller.
 */
int config_load_file(const char *path, monitor_config_t *config);

#endif /* CONFIG_H */
