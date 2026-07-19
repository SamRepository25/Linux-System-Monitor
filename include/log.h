#ifndef LOG_H
#define LOG_H

/* Opens `path` for appending. Returns 0 on success, -1 on failure.
 * Safe to skip entirely — log_write() is a silent no-op if the log
 * was never opened. */
int log_open(const char *path);

/* Appends one timestamped, printf-style line to the log file.
 * No-op if log_open() was never called or failed. */
void log_write(const char *fmt, ...);

/* Closes the log file, if open. Safe to call even if never opened. */
void log_close(void);

#endif /* LOG_H */
