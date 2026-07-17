# Linux System Monitor

![C](https://img.shields.io/badge/C-C17-blue)
![Platform](https://img.shields.io/badge/Platform-Linux-success)
![Build](https://img.shields.io/badge/Build-GNU%20Make-orange)
![License](https://img.shields.io/badge/License-MIT-green)

A terminal-based Linux system monitor written entirely in **C**, built as a
systems programming portfolio project. Inspired by tools like **top** and
**htop**, it reads live system information directly from the Linux kernel
through the **/proc filesystem** and **POSIX APIs**, without relying on
third-party libraries.

The project is developed incrementally using milestone-based development.
Every module is implemented, documented, tested, and kept free of compiler
warnings before moving on to the next feature.

---

## Features

- Written in modern **C17**
- Reads live system information from the Linux kernel
- Uses the **/proc filesystem** and **POSIX system calls**
- Modular architecture
- Zero compiler warnings (`-Wall -Wextra -Werror`)
- No external libraries
- Compatible with Linux and WSL2
- Designed for learning Linux systems programming

---

## Project Goals

This project aims to strengthen practical knowledge of:

- Linux internals
- Systems programming in C
- POSIX APIs
- `/proc` filesystem
- Defensive programming
- Modular software architecture

Rather than wrapping existing utilities, every module retrieves and processes
system information directly from Linux interfaces.

---

## Roadmap

| Version | Module | Status |
|----------|--------|--------|
| v0.1.0 | System Information | ✅ Complete |
| v0.2.0 | CPU Usage | 🚧 In Progress |
| v0.3.0 | Memory Monitoring | ⏳ Planned |
| v0.4.0 | Disk Statistics | ⏳ Planned |
| v0.5.0 | Process Monitoring | ⏳ Planned |
| v0.6.0 | Network Statistics | ⏳ Planned |
| v0.7.0 | Logged-in Users | ⏳ Planned |
| v0.8.0 | Live Refresh Dashboard | ⏳ Planned |
| v1.0.0 | Stable Release | 🎯 Target |

---

# Current Status

## Milestone 1 — System Information Module

### Implemented

- Hostname
- Kernel version
- CPU architecture
- System uptime
- Load average (1 / 5 / 15 minute)

### Coming Next

- CPU utilization
- Memory statistics
- Disk statistics
- Running processes
- Network statistics
- Logged-in users
- Live terminal dashboard
- CLI arguments

---

## Project Structure

```
linux-system-monitor/
├── include/
│   └── sysinfo.h
├── modules/
│   └── sysinfo.c
├── src/
│   └── main.c
├── build/                 # Generated during compilation
├── Makefile
├── .gitignore
├── LICENSE
└── README.md
```

---

## Requirements

- Linux (native or WSL2)
- GCC with C17 support
- GNU Make

---

## Build

```bash
make
```

The project is compiled with:

```bash
-std=c17 -Wall -Wextra -Werror
```

Any compiler warning is treated as an error to maintain clean,
production-quality code.

Object files and the executable are generated inside the `build/`
directory.

---

## Run

```bash
./build/sysmon
```

Or build and run together:

```bash
make run
```

---

## Example Output

```text
==============================
 Linux System Monitor v0.1.0
==============================

Hostname      : my-machine
Kernel        : Linux 6.8.0
Architecture  : x86_64
Uptime        : 2h 14m 37s
Load Average  : 0.15 0.22 0.19
```

---

## Clean

```bash
make clean
```

Removes the generated `build/` directory.

---

# Architecture

```
            Linux Kernel
                  │
      ┌───────────┴───────────┐
      │                       │
  POSIX APIs          /proc Filesystem
      │                       │
      └───────────┬───────────┘
                  │
          System Modules
                  │
          Terminal Interface
```

---

# Design Decisions

### No Dynamic Memory Allocation

The System Information module uses fixed-size structures instead of
`malloc()` because all fields have well-defined maximum lengths on Linux.

Benefits:

- No memory leaks
- Simpler implementation
- Predictable memory usage

---

### Why `uname()`?

System identity information is retrieved using the POSIX `uname()` syscall
instead of parsing `/proc`.

Advantages:

- POSIX-compliant
- Atomic retrieval
- Cleaner implementation
- More reliable than parsing multiple files

---

### Why `/proc`?

Some information, such as uptime and load average, has no equivalent POSIX
system call.

Therefore the project reads:

- `/proc/uptime`
- `/proc/loadavg`

directly from the Linux kernel.

---

### Defensive Programming

Every:

- system call
- file open
- file read
- parsing operation

checks its return value.

The program fails loudly instead of silently producing incorrect output.

---

# Development Principles

- Modular architecture
- One milestone at a time
- Zero compiler warnings
- Defensive programming
- Consistent code style
- Clear documentation
- Small, reviewable commits

---

# Future Improvements

- Interactive dashboard similar to `htop`
- Configurable refresh interval
- Process sorting
- Colored terminal output
- CPU usage graphs
- Memory usage bars
- JSON export
- Logging support
- Unit tests
- GitHub Actions CI

---

## License

This project is licensed under the **MIT License**.

See the `LICENSE` file for details.
