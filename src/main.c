/*
 * main.c
 *
 * Entry point for the Linux System Monitor.
 * Milestone 4: adds disk usage per mounted filesystem.
 */

#include <stdio.h>
#include <stdlib.h>
#include "sysinfo.h"
#include "cpu.h"
#include "mem.h"
#include "disk.h"

/* How long to sample /proc/stat over when measuring CPU usage.
 * 200ms balances responsiveness against reading noise. */
#define CPU_SAMPLE_INTERVAL_MS 200

int main(void) {
    sysinfo_t sys;
    cpu_info_t cpu;
    mem_info_t mem;
    disk_info_t disk;

    if (sysinfo_collect(&sys) != 0) {
        fprintf(stderr, "Fatal: could not collect system information.\n");
        return EXIT_FAILURE;
    }
    sysinfo_print(&sys);

    printf("\nSampling CPU usage (%dms)...\n", CPU_SAMPLE_INTERVAL_MS);
    if (cpu_collect(&cpu, CPU_SAMPLE_INTERVAL_MS) != 0) {
        fprintf(stderr, "Fatal: could not collect CPU information.\n");
        return EXIT_FAILURE;
    }
    cpu_print(&cpu);

    printf("\n");
    if (mem_collect(&mem) != 0) {
        fprintf(stderr, "Fatal: could not collect memory information.\n");
        return EXIT_FAILURE;
    }
    mem_print(&mem);

    printf("\n");
    if (disk_collect(&disk) != 0) {
        fprintf(stderr, "Fatal: could not collect disk information.\n");
        return EXIT_FAILURE;
    }
    disk_print(&disk);

    return EXIT_SUCCESS;
}
