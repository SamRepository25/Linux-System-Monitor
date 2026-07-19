# Linux System Monitor

A modular terminal-based Linux system monitor written in **C**, built as a systems programming portfolio project. Inspired by tools like **top** and **htop**, it collects live system information directly from the Linux kernel using the **/proc** filesystem, **/sys**, POSIX APIs, and standard Linux system calls.

This project was developed incrementally with a modular architecture, making each subsystem independent, reusable, and easy to maintain.

---

## Features

### System Information
- Hostname
- Kernel version
- System architecture
- System uptime
- Load averages

### CPU Monitoring
- CPU model
- Number of cores
- Current frequency
- Real-time CPU utilization

### Memory Monitoring
- Total memory
- Used memory
- Free memory
- Available memory
- Swap usage

### Disk Monitoring
- Mounted filesystems
- Filesystem type
- Total space
- Used space
- Available space
- Disk usage percentage

### Process Monitoring
- Live process list
- PID
- Process state
- CPU usage
- Memory usage
- Resident Set Size (RSS)
- Sort by:
  - CPU usage
  - Memory usage
  - PID

### Network Monitoring
- Interface status
- Bytes received
- Bytes transmitted
- Receive throughput
- Transmit throughput

### Logged-in Users
- Displays currently logged-in users using the system.

### Live Refresh
- Automatic terminal refresh
- Configurable refresh interval
- Optional refresh count
- Clean Ctrl+C handling

### Command-Line Interface
Supports:

- Live refresh
- Process sorting
- Process row limits
- Help menu
- Kill by PID
- Logging
- Config file
- Disable colored output

### Colored Output
- ANSI terminal colors
- Automatically disabled when output is redirected
- Manual disable option (`-x`)

### Logging
- Timestamped log file generation
- Append-only logging

### Configuration File
Supports `sysmon.conf`

Example:

```text
refresh=2
rows=15
sort=m
color=off
```

Configuration precedence:

```
Built-in defaults
        ↓
Configuration file
        ↓
Command-line arguments
```

### Process Control
Supports:

- SIGTERM (`-k`)
- SIGKILL (`-K`)

---

# Technologies Used

- C17
- POSIX APIs
- Linux `/proc`
- Linux `/sys`
- Make
- GCC
- ANSI Escape Codes

---

# Project Structure

```
linux-system-monitor/
│
├── include/
│   ├── cli.h
│   ├── color.h
│   ├── config.h
│   ├── cpu.h
│   ├── disk.h
│   ├── log.h
│   ├── mem.h
│   ├── net.h
│   ├── process.h
│   ├── sysinfo.h
│   └── users.h
│
├── modules/
│   ├── cli.c
│   ├── color.c
│   ├── config.c
│   ├── cpu.c
│   ├── disk.c
│   ├── log.c
│   ├── mem.c
│   ├── net.c
│   ├── process.c
│   ├── sysinfo.c
│   └── users.c
│
├── src/
│   └── main.c
│
├── Makefile
├── LICENSE
└── README.md
```

---

# Building

Clone the repository:

```bash
git clone https://github.com/SamRepository25/Linux-System-Monitor.git
cd Linux-System-Monitor
```

Build:

```bash
make
```

Clean:

```bash
make clean
```

Run:

```bash
./build/sysmon
```

---

# Usage

Show help

```bash
./build/sysmon -h
```

Live refresh every second

```bash
./build/sysmon -r 1
```

Refresh every 2 seconds for 10 updates

```bash
./build/sysmon -r 2 -n 10
```

Display only 5 processes

```bash
./build/sysmon -t 5
```

Sort by memory

```bash
./build/sysmon -s m
```

Sort by PID

```bash
./build/sysmon -s p
```

Terminate a process gracefully

```bash
./build/sysmon -k <PID>
```

Force terminate a process

```bash
./build/sysmon -K <PID>
```

Write logs

```bash
./build/sysmon -l run.log
```

Disable colored output

```bash
./build/sysmon -x
```

---

# Example Output

```
Status: CPU 6.4% MEM 34.1% DISK(/) 52%

=== CPU Information ===
Model        : Intel Core i5
Usage        : 6.4%

=== Memory Information ===
Used RAM     : 5.4 GB (34%)

=== Processes ===
PID      NAME           CPU%    MEM%
1452     firefox        8.3%    6.1%
2270     code           3.2%    4.8%
```

---

# Skills Demonstrated

- Systems Programming
- Linux Programming
- POSIX System Calls
- Modular Software Design
- Process Management
- File Parsing
- Terminal Programming
- Command-Line Interface Design
- Build Automation
- Software Architecture
- Git & GitHub

---

# Future Improvements

Possible future enhancements include:

- Interactive keyboard navigation
- Process search/filter
- Multi-core CPU graphs
- Export to JSON/CSV
- Docker monitoring
- GPU statistics
- Temperature sensors
- ncurses-based interface

---

# License

This project is licensed under the MIT License.

---

# Author

**B SIMAK AHMED**

Computer Science & Engineering Student

GitHub: https://github.com/SamRepository25
