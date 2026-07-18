#ifndef NET_H
#define NET_H

/*
 * net.h
 *
 * Public interface for the Network module.
 * Collects per-interface RX/TX byte and packet counters from
 * /proc/net/dev, computes throughput (KB/s) via two time-sampled
 * snapshots, and reports interface up/down status from sysfs.
 */

#define NET_MAX_INTERFACES 32
#define NET_IFACE_NAME_LEN 32

typedef struct {
    char name[NET_IFACE_NAME_LEN];

    unsigned long long rx_bytes;    /* cumulative bytes received since boot */
    unsigned long long tx_bytes;    /* cumulative bytes transmitted since boot */
    unsigned long long rx_packets;
    unsigned long long tx_packets;

    double rx_rate_kbps;            /* derived: throughput over the sample window */
    double tx_rate_kbps;

    int is_up;                      /* 1 if operstate is "up" or "unknown" (see net.c) */
} net_interface_t;

typedef struct {
    net_interface_t interfaces[NET_MAX_INTERFACES];
    int count;
} net_info_t;

/*
 * Reads /proc/net/dev twice, `sample_interval_ms` milliseconds apart,
 * to compute RX/TX throughput per interface, and reads each
 * interface's operstate from /sys/class/net/<iface>/operstate.
 * Fills `info`. Returns 0 on success, -1 on failure.
 */
int net_collect(net_info_t *info, unsigned int sample_interval_ms);

/*
 * Prints a formatted table of all collected interfaces to stdout.
 */
void net_print(const net_info_t *info);

#endif /* NET_H */
