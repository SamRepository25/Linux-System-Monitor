/*
 * users.c
 *
 * Implementation of the Users module.
 *
 * utmpx functions (setutxent/getutxent/endutxent) are POSIX/XSI
 * functions, not exposed under strict -std=c17 unless we ask for
 * them explicitly. _DEFAULT_SOURCE (in addition to _POSIX_C_SOURCE)
 * is what glibc actually gates <utmpx.h>'s declarations behind.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "users.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <utmpx.h>

/* ---- static (file-private) helper functions ---- */

/*
 * Formats a login timestamp (seconds since epoch) into `buf` as
 * "YYYY-MM-DD HH:MM", using localtime_r (the thread-safe variant —
 * good practice even in a single-threaded program, since it avoids
 * the static internal buffer that plain localtime() uses).
 */
static void format_login_time(time_t when, char *buf, size_t buflen) {
    struct tm tm_result;
    if (localtime_r(&when, &tm_result) == NULL) {
        snprintf(buf, buflen, "unknown");
        return;
    }
    strftime(buf, buflen, "%Y-%m-%d %H:%M", &tm_result);
}

/* ---- public API ---- */

int users_collect(user_info_t *info) {
    if (info == NULL) {
        fprintf(stderr, "users: NULL pointer passed to users_collect\n");
        return -1;
    }

    memset(info, 0, sizeof(user_info_t));

    /* Rewind to the start of the utmpx database. Safe to call even
     * if it's the first access; also makes users_collect() safely
     * re-callable across multiple refresh cycles. */
    setutxent();

    struct utmpx *entry;
    while ((entry = getutxent()) != NULL && info->count < USERS_MAX_ENTRIES) {
        /* USER_PROCESS marks an actual logged-in session, as opposed
         * to boot records, runlevel changes, or dead process slots
         * that also live in the same utmpx database. */
        if (entry->ut_type != USER_PROCESS) {
            continue;
        }

        user_entry_t *out = &info->entries[info->count];

        /* ut_user/ut_line/ut_host are fixed-size char arrays in
         * struct utmpx and are NOT guaranteed to be NUL-terminated
         * if they fill the entire field width, so we copy with an
         * explicit bound and terminate manually. */
        strncpy(out->username, entry->ut_user, USERS_NAME_LEN - 1);
        out->username[USERS_NAME_LEN - 1] = '\0';

        strncpy(out->tty, entry->ut_line, USERS_LINE_LEN - 1);
        out->tty[USERS_LINE_LEN - 1] = '\0';

        strncpy(out->host, entry->ut_host, USERS_HOST_LEN - 1);
        out->host[USERS_HOST_LEN - 1] = '\0';

        format_login_time((time_t) entry->ut_tv.tv_sec,
                           out->login_time, USERS_TIME_LEN);

        info->count++;
    }

    endutxent();
    return 0;
}

void users_print(const user_info_t *info) {
    if (info == NULL) return;

    printf("=== Logged-in Users ===\n");

    if (info->count == 0) {
        printf("No users currently logged in.\n");
        return;
    }

    printf("%-12s %-10s %-20s %-16s\n", "USER", "TTY", "HOST", "LOGIN TIME");

    for (int i = 0; i < info->count; i++) {
        const user_entry_t *u = &info->entries[i];
        const char *host = (u->host[0] != '\0') ? u->host : "-";
        printf("%-12s %-10s %-20s %-16s\n", u->username, u->tty, host, u->login_time);
    }
}
