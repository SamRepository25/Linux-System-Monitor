/*
 * main.c
 *
 * Entry point for the Linux System Monitor.
 * Milestone 1: displays static/semi-static system information.
 */

#include <stdio.h>
#include <stdlib.h>
#include "sysinfo.h"

int main(void) {
    sysinfo_t info;

    if (sysinfo_collect(&info) != 0) {
        fprintf(stderr, "Fatal: could not collect system information.\n");
        return EXIT_FAILURE;
    }

    sysinfo_print(&info);

    return EXIT_SUCCESS;
}
