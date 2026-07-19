#ifndef COLOR_H
#define COLOR_H

#define COLOR_RESET  "\033[0m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_BOLD   "\033[1m"

/* Global toggle: 1 = colors enabled, 0 = disabled. Set once at
 * startup in main.c (auto-detected via color_detect_tty(), then
 * overridable by config file or -x flag). */
extern int g_color_enabled;

/* Returns 1 if stdout is an interactive terminal, 0 if piped/redirected. */
int color_detect_tty(void);

/* Returns the ANSI code for a percentage: green <50, yellow 50-80,
 * red >80. Returns "" if g_color_enabled is 0. */
const char *color_for_percent(double percent);

/* Returns COLOR_RESET if colors are enabled, "" otherwise. */
const char *color_reset(void);

/* Returns green if is_true is nonzero (e.g. "UP"), red if zero
 * (e.g. "DOWN"). Returns "" if g_color_enabled is 0. */
const char *color_for_bool(int is_true);

#endif /* COLOR_H */
