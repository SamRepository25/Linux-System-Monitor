#ifndef USERS_H
#define USERS_H

/*
 * users.h
 *
 * Public interface for the Users module.
 * Enumerates currently logged-in users via the POSIX utmpx login
 * accounting API (setutxent/getutxent/endutxent), which reads the
 * system's utmp database (typically /var/run/utmp).
 */

#define USERS_MAX_ENTRIES 64
#define USERS_NAME_LEN     32
#define USERS_LINE_LEN     32
#define USERS_HOST_LEN     64
#define USERS_TIME_LEN     32

typedef struct {
    char username[USERS_NAME_LEN];   /* login name, e.g. "simak" */
    char tty[USERS_LINE_LEN];        /* terminal line, e.g. "pts/0" */
    char host[USERS_HOST_LEN];       /* origin host, empty for local logins */
    char login_time[USERS_TIME_LEN]; /* formatted "YYYY-MM-DD HH:MM" */
} user_entry_t;

typedef struct {
    user_entry_t entries[USERS_MAX_ENTRIES];
    int count;
} user_info_t;

/*
 * Scans the utmpx login database for active user sessions and fills
 * `info`. An empty result (info->count == 0) is not an error — it's
 * normal on headless machines, containers, or systems with no utmp
 * database present at all.
 *
 * Returns 0 on success (including when zero users are found), -1 only
 * on a genuine NULL-argument misuse.
 */
int users_collect(user_info_t *info);

/*
 * Prints a formatted table of logged-in users to stdout.
 */
void users_print(const user_info_t *info);

#endif /* USERS_H */
