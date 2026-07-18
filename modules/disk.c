/*
 * disk.c
 *
 * Implementation of the Disk module.
 *
 * statvfs() is a POSIX function; _POSIX_C_SOURCE must be defined
 * before any system header is included so glibc exposes its
 * declaration under strict -std=c17.
 */
#define _POSIX_C_SOURCE 200809L

#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>

/* ---- static (file-private) helper functions ---- */

/*
 * Filesystem types that don't represent real, user-relevant disk
 * storage: kernel-internal pseudo filesystems, RAM-backed tmpfs,
 * and squashfs (read-only package/snap mounts, and in this sandbox,
 * mounted skill/tool directories) that would otherwise clutter output.
 */
static const char *PSEUDO_FSTYPES[] = {
    "proc", "sysfs", "devtmpfs", "devpts", "tmpfs",
    "cgroup", "cgroup2", "pstore", "bpf", "tracefs",
    "debugfs", "mqueue", "hugetlbfs", "securityfs", "autofs",
    "binfmt_misc", "configfs", "fusectl", "rpc_pipefs",
    "squashfs", "overlay", "fuse.rclone",
};
static const size_t PSEUDO_FSTYPES_COUNT =
    sizeof(PSEUDO_FSTYPES) / sizeof(PSEUDO_FSTYPES[0]);

static int is_pseudo_fs(const char *fstype) {
    for (size_t i = 0; i < PSEUDO_FSTYPES_COUNT; i++) {
        if (strcmp(fstype, PSEUDO_FSTYPES[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * Fills in the space-usage fields of `entry` by calling statvfs()
 * on its mount_point. Returns 0 on success, -1 on failure.
 */
static int collect_usage(disk_entry_t *entry) {
    struct statvfs vfs;

    if (statvfs(entry->mount_point, &vfs) != 0) {
        /* Not fatal to the whole scan — just skip this one entry.
         * Common causes: permission denied, or a mount that
         * disappeared between reading /proc/mounts and this call. */
        return -1;
    }

    unsigned long long frsize = (unsigned long long) vfs.f_frsize;
    unsigned long long total = (unsigned long long) vfs.f_blocks * frsize;
    unsigned long long free_incl_reserved = (unsigned long long) vfs.f_bfree * frsize;
    unsigned long long available = (unsigned long long) vfs.f_bavail * frsize;

    unsigned long long used = (free_incl_reserved > total)
        ? 0
        : total - free_incl_reserved;

    entry->total_bytes = total;
    entry->used_bytes = used;
    entry->available_bytes = available;

    /* Matches df's convention: percentage is of (used + available),
     * i.e. space visible to a normal user, not the raw total which
     * may include a root-reserved slice. This lets used_percent
     * correctly reach 100% when a normal user's space is exhausted. */
    unsigned long long denominator = used + available;
    entry->used_percent = (denominator > 0)
        ? ((double) used / (double) denominator) * 100.0
        : 0.0;

    return 0;
}

/*
 * Formats a byte count as a human-readable string, choosing
 * MB or GB depending on magnitude.
 */
static void format_bytes(unsigned long long bytes, char *buf, size_t buflen) {
    double mb = (double) bytes / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        snprintf(buf, buflen, "%.2f GB", mb / 1024.0);
    } else {
        snprintf(buf, buflen, "%.1f MB", mb);
    }
}

/* ---- public API ---- */

int disk_collect(disk_info_t *info) {
    if (info == NULL) {
        fprintf(stderr, "disk: NULL pointer passed to disk_collect\n");
        return -1;
    }

    FILE *fp = fopen("/proc/mounts", "r");
    if (fp == NULL) {
        perror("disk: failed to open /proc/mounts");
        return -1;
    }

    memset(info, 0, sizeof(disk_info_t));

    char line[512];
    char device[DISK_PATH_LEN];
    char mount_point[DISK_PATH_LEN];
    char fstype[DISK_FSTYPE_LEN];

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (info->count >= DISK_MAX_ENTRIES) {
            fprintf(stderr, "disk: reached DISK_MAX_ENTRIES (%d), some mounts skipped\n",
                    DISK_MAX_ENTRIES);
            break;
        }

        /* Fields: <device> <mount_point> <fstype> <options> <dump> <pass>
         * We only need the first three; %*s discards the rest without
         * storing it. Note: mount points containing spaces are escaped
         * as "\040" in /proc/mounts — we don't unescape that here,
         * which is an acceptable, documented limitation for this
         * project (real-world paths with spaces are rare). */
        int matched = sscanf(line, "%255s %255s %31s %*s %*s %*s",
                              device, mount_point, fstype);
        if (matched != 3) {
            continue; /* malformed line, skip */
        }

        if (is_pseudo_fs(fstype)) {
            continue;
        }

        disk_entry_t *entry = &info->entries[info->count];
        memset(entry, 0, sizeof(disk_entry_t));

        strncpy(entry->device, device, DISK_PATH_LEN - 1);
        strncpy(entry->mount_point, mount_point, DISK_PATH_LEN - 1);
        strncpy(entry->fstype, fstype, DISK_FSTYPE_LEN - 1);

        if (collect_usage(entry) != 0) {
            continue; /* skip mount points statvfs() couldn't read */
        }

        info->count++;
    }

    fclose(fp);
    return 0;
}

void disk_print(const disk_info_t *info) {
    if (info == NULL) return;

    printf("=== Disk Information ===\n");

    if (info->count == 0) {
        printf("No real filesystems found.\n");
        return;
    }

    printf("%-20s %-8s %10s %10s %10s %7s\n",
           "Mount Point", "FS Type", "Total", "Used", "Available", "Use%");

    for (int i = 0; i < info->count; i++) {
        const disk_entry_t *e = &info->entries[i];

        char total_str[32], used_str[32], avail_str[32];
        format_bytes(e->total_bytes, total_str, sizeof(total_str));
        format_bytes(e->used_bytes, used_str, sizeof(used_str));
        format_bytes(e->available_bytes, avail_str, sizeof(avail_str));

        printf("%-20s %-8s %10s %10s %10s %6.1f%%\n",
               e->mount_point, e->fstype, total_str, used_str, avail_str,
               e->used_percent);
    }
}
