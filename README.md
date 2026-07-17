# Linux System Monitor

A terminal-based system monitor written in C, built as a systems-programming
portfolio project. Inspired by tools like `htop` and `top`, it reads live data
directly from the Linux kernel via the `/proc` filesystem and POSIX syscalls.

This project is being built incrementally, one module at a time. Each module
is documented, tested, and warning-free before the next is added.

## Status: Milestone 1 — System Information Module

Currently implemented:

- Hostname, kernel version, and CPU architecture (via `uname()`)
- System uptime (via `/proc/uptime`)
- Load average — 1 / 5 / 15 minute (via `/proc/loadavg`)

Planned next: CPU usage, memory, disk, process list, network stats, logged-in
users, live refresh, and CLI arguments.

## Requirements

- Linux (native or WSL2)
- GCC with C17 support
- GNU Make

## Project Structure

```
linux-system-monitor/
├── src/
│   └── main.c              # Entry point
├── include/
│   └── sysinfo.h            # Public API for the sysinfo module
├── modules/
│   └── sysinfo.c             # System info implementation
├── build/                    # Generated at build time (git-ignored)
├── Makefile
├── .gitignore
└── README.md
```

## Build

```bash
make
```

This compiles with `-std=c17 -Wall -Wextra -Werror`, so the build fails on any
warning — the project targets zero compiler warnings at all times.

Object files and the binary are placed under `build/`, mirroring the source
tree (e.g. `modules/sysinfo.c` → `build/modules/sysinfo.o`).

## Run

```bash
./build/sysmon
```

Or build and run in one step:

```bash
make run
```

### Example output

```
=== System Information ===
Hostname     : my-machine
Kernel       : 6.8.0-generic
Architecture : x86_64
Uptime       : 2h 14m 37s
Load Average : 0.15, 0.22, 0.19 (1m, 5m, 15m)
```

## Clean

```bash
make clean
```

Removes the `build/` directory.

## Design Notes

- **No dynamic allocation in this module.** All system-identity fields have
  known, bounded maximum lengths on Linux, so a fixed-size struct is used
  instead of `malloc`/`free` — simpler and impossible to leak.
- **`uname()` vs. `/proc` parsing.** Hostname, kernel version, and
  architecture are fetched with the `uname()` syscall rather than by reading
  `/proc/sys/kernel/hostname` etc. `uname()` is the POSIX-correct, atomic way
  to get this data in one call. Uptime and load average have no equivalent
  syscall, so they're read directly from `/proc/uptime` and `/proc/loadavg`.
- **Defensive parsing.** Every file read and syscall checks its return value.
  `fscanf`'s match count is checked explicitly — if `/proc`'s format ever
  changed, the program fails loudly instead of silently using zeroed or
  garbage data.

## License

MIT — see `LICENSE`.
# Linux System Monitor

A Linux System Monitoring tool written in C.

Author: Your Name
Language: C17
Platform: Linux
