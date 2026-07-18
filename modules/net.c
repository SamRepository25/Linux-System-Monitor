/*
 * net.c
 *
 * Implementation of the Network module.
 *
 * clock_gettime and nanosleep are POSIX functions; _POSIX_C_SOURCE
 * must be defined before any system header is included so glibc
 * exposes their declarations under strict -std=c17.
 */
#define _POSIX_C_SOURCE 200809L

#include "net.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ---- static (file-private) helper functions ---- */

/*
 * Reads /proc/net/dev in full and fills `out` with one entry per
 * interface (skipping the two header lines). Returns the number of
 * interfaces found, or -1 on failure to open the file.
 */
static int read_net_dev(net_interface_t out[], int max_entries) {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        perror("net: failed to open /proc/net/dev");
        return -1;
    }

    char line[512];
    int count = 0;

    /* Discard the two fixed header lines ("Inter-|..." and
     * " face |..."); if the file ever has fewer lines than that,
     * the loop below simply finds nothing to parse, which is safe. */
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp) != NULL && count < max_entries) {
        char name[NET_IFACE_NAME_LEN];
        unsigned long long rx_bytes, rx_packets, tx_bytes, tx_packets;

        /* Leading space in the format skips the whitespace padding
         * before the interface name. "%31[^:]" then captures
         * everything up to (not including) the colon. The six
         * "%*u" fields discard rx_errs, rx_drop, rx_fifo, rx_frame,
         * rx_compressed, rx_multicast — landing us exactly on
         * tx_bytes and tx_packets next. Suppressed fields
         * deliberately omit length modifiers (%*u, not %*llu) since
         * GCC's format checker flags that combination and nothing
         * is stored for them regardless. */
        int matched = sscanf(line,
            " %31[^:]: %llu %llu %*u %*u %*u %*u %*u %*u %llu %llu",
            name, &rx_bytes, &rx_packets, &tx_bytes, &tx_packets);

        if (matched != 5) {
            continue; /* malformed line, skip */
        }

        strncpy(out[count].name, name, NET_IFACE_NAME_LEN - 1);
        out[count].name[NET_IFACE_NAME_LEN - 1] = '\0';
        out[count].rx_bytes = rx_bytes;
        out[count].rx_packets = rx_packets;
        out[count].tx_bytes = tx_bytes;
        out[count].tx_packets = tx_packets;
        out[count].rx_rate_kbps = 0.0;
        out[count].tx_rate_kbps = 0.0;
        out[count].is_up = 0;
        count++;
    }

    fclose(fp);
    return count;
}

/*
 * Reads /sys/class/net/<iface>/operstate and returns 1 if the
 * interface should be considered "active" for display purposes.
 *
 * "up" is the normal active state. "unknown" is also treated as
 * active because loopback ("lo") commonly reports "unknown" despite
 * being fully functional — a well-known quirk, since loopback
 * doesn't implement real carrier-detection semantics.
 */
static int read_operstate_is_up(const char *iface_name) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface_name);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return 0; /* can't determine — treat as down rather than guessing "up" */
    }

    char state[32];
    int is_up = 0;
    if (fgets(state, sizeof(state), fp) != NULL) {
        /* Strip trailing newline for a clean comparison. */
        size_t len = strlen(state);
        if (len > 0 && state[len - 1] == '\n') {
            state[len - 1] = '\0';
        }
        is_up = (strcmp(state, "up") == 0 || strcmp(state, "unknown") == 0);
    }

    fclose(fp);
    return is_up;
}

/*
 * Finds the interface named `name` in `snapshot` (of `count`
 * entries) and returns a pointer to it, or NULL if not present.
 */
static const net_interface_t *find_by_name(const net_interface_t snapshot[],
                                            int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(snapshot[i].name, name) == 0) {
            return &snapshot[i];
        }
    }
    return NULL;
}

/* ---- public API ---- */

int net_collect(net_info_t *info, unsigned int sample_interval_ms) {
    if (info == NULL) {
        fprintf(stderr, "net: NULL pointer passed to net_collect\n");
        return -1;
    }

    memset(info, 0, sizeof(net_info_t));

    net_interface_t before[NET_MAX_INTERFACES];
    int before_count = read_net_dev(before, NET_MAX_INTERFACES);
    if (before_count < 0) {
        return -1;
    }

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    struct timespec sleep_ts;
    sleep_ts.tv_sec = (time_t) (sample_interval_ms / 1000);
    sleep_ts.tv_nsec = (long) (sample_interval_ms % 1000) * 1000000L;
    nanosleep(&sleep_ts, NULL);

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed_seconds = (double) (t_end.tv_sec - t_start.tv_sec) +
                              (double) (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    if (elapsed_seconds <= 0.0) {
        elapsed_seconds = (double) sample_interval_ms / 1000.0;
    }

    net_interface_t after[NET_MAX_INTERFACES];
    int after_count = read_net_dev(after, NET_MAX_INTERFACES);
    if (after_count < 0) {
        return -1;
    }

    int out_count = (after_count < NET_MAX_INTERFACES) ? after_count : NET_MAX_INTERFACES;

    for (int i = 0; i < out_count; i++) {
        net_interface_t *entry = &info->interfaces[i];
        *entry = after[i];

        const net_interface_t *prev = find_by_name(before, before_count, entry->name);
        if (prev != NULL) {
            unsigned long long rx_delta = (entry->rx_bytes >= prev->rx_bytes)
                ? entry->rx_bytes - prev->rx_bytes : 0;
            unsigned long long tx_delta = (entry->tx_bytes >= prev->tx_bytes)
                ? entry->tx_bytes - prev->tx_bytes : 0;

            entry->rx_rate_kbps = ((double) rx_delta / elapsed_seconds) / 1024.0;
            entry->tx_rate_kbps = ((double) tx_delta / elapsed_seconds) / 1024.0;
        }
        /* If the interface didn't exist in the "before" snapshot
         * (e.g. it just came up), rates simply stay at 0 — no
         * baseline to compare against yet. */

        entry->is_up = read_operstate_is_up(entry->name);
    }

    info->count = out_count;
    return 0;
}

void net_print(const net_info_t *info) {
    if (info == NULL) return;

    printf("=== Network Information ===\n");

    if (info->count == 0) {
        printf("No network interfaces found.\n");
        return;
    }

    printf("%-10s %-6s %12s %12s %10s %10s\n",
           "IFACE", "STATE", "RX", "TX", "RX RATE", "TX RATE");

    for (int i = 0; i < info->count; i++) {
        const net_interface_t *n = &info->interfaces[i];

        double rx_mb = (double) n->rx_bytes / (1024.0 * 1024.0);
        double tx_mb = (double) n->tx_bytes / (1024.0 * 1024.0);

        printf("%-10s %-6s %9.2f MB %9.2f MB %7.1f KB/s %7.1f KB/s\n",
               n->name, n->is_up ? "UP" : "DOWN",
               rx_mb, tx_mb, n->rx_rate_kbps, n->tx_rate_kbps);
    }
}
