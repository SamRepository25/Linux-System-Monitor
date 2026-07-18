#ifndef DISK_H
#define DISK_H

/*
 * disk.h
 *
 * Public interface for the Disk module.
 * Enumerates mounted filesystems from /proc/mounts, filters out
 * pseudo/virtual filesystems, and reports space usage per mount
 * point via statvfs().
 */

#define DISK_MAX_ENTRIES 32
#define DISK_PATH_LEN    256
#define DISK_FSTYPE_LEN  32

typedef struct {
    char device[DISK_PATH_LEN];       /* e.g. "/dev/sda1" */
    char mount_point[DISK_PATH_LEN];  /* e.g. "/" or "/home" */
    char fstype[DISK_FSTYPE_LEN];     /* e.g. "ext4" */

    unsigned long long total_bytes;
    unsigned long long used_bytes;
    unsigned long long available_bytes; /* usable by a normal (non-root) user */
    double used_percent;                /* matches df's calculation, see disk.c */
} disk_entry_t;

typedef struct {
    disk_entry_t entries[DISK_MAX_ENTRIES];
    int count;
} disk_info_t;

/*
 * Reads /proc/mounts, filters to real (non-pseudo) filesystems, and
 * fills `info->entries` with space usage for each via statvfs().
 * Returns 0 on success, -1 on failure (e.g. /proc/mounts unreadable).
 *
 * Individual mount points that fail statvfs() (e.g. permission
 * issues, or a mount that vanished mid-scan) are skipped rather than
 * causing the whole collection to fail.
 */
int disk_collect(disk_info_t *info);

/*
 * Prints a formatted table of all collected disk entries to stdout.
 */
void disk_print(const disk_info_t *info);

#endif /* DISK_H */
