/*
 * main.c
 *
 * Entry point for the Linux System Monitor.
 * Milestone 2: adds CPU usage, core count, model, and frequency.
 */

#include <stdio.h>
#include <stdlib.h>
#include "sysinfo.h"
#include "cpu.h"

/* How long to sample /proc/stat over when measuring CPU usage.
 * 200ms balances responsiveness against reading noise. */
#define CPU_SAMPLE_INTERVAL_MS 200

int main(void) {
    sysinfo_t sys;
    cpu_info_t cpu;

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

    return EXIT_SUCCESS;
}
