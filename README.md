# CampusCompute: Intelligent Job Scheduling and Resource Management

CampusCompute is a modular C-based command-line operating systems prototype engineered for university computing clusters and shared laboratory environments. It demonstrates core operating system mechanisms including process lifecycle tracking, CPU scheduling policies, multi-threaded synchronized logging, POSIX inter-process communication (shared memory and named semaphores), deadlock avoidance via Banker's safety algorithm, parent-child worker process monitoring with controlled failure handling, and Linux process resource limits.

---

## Features & Architectural Overview

CampusCompute is structured around eight key functional requirements:

1. **FR1 – Job Management & Lifecycle Tracking**:
   - Represents computing jobs with Job ID, Arrival Time, CPU Burst Time, Priority, and Status (`NEW`, `READY`, `RUNNING`, `COMPLETED`).
   - Strict input validation against malformed lines, out-of-range bursts, negative arrivals, and duplicate identifiers.
2. **FR2 – CPU Scheduling Policies**:
   - **First-Come, First-Served (FCFS)**: Non-preemptive scheduling with tie-breaking by arrival time and input order.
   - **Round Robin (RR)**: Preemptive scheduling using a round-robin ready queue with a configurable time quantum.
   - **Non-preemptive Priority**: Schedules highest-priority jobs first (lower integer indicates higher priority).
   - Generates ASCII Gantt timeline charts and computes Turnaround Time (TAT), Waiting Time (WT), Response Time (RT), and CPU utilization metrics.
   - Policy comparison view evaluates all three algorithms on an identical dataset.
3. **FR3 – Concurrent Activity Logger**:
   - Bounded producer-consumer circular buffer implemented with POSIX Threads (`pthreads`).
   - Mutual exclusion protected by `pthread_mutex_t`.
   - Full and empty buffer condition coordination using two condition variables (`pthread_cond_t`: `not_full` and `not_empty`) within guarded predicate loops.
   - Dedicated consumer worker thread writes structured entries to `logs/campuscompute.log` and terminates cleanly upon drained shutdown.
4. **FR4 – Inter-Process Communication (IPC)**:
   - POSIX Shared Memory (`shm_open`, `ftruncate`, `mmap`, `munmap`, `shm_unlink`).
   - Synchronized process handshaking using POSIX Named Semaphores (`sem_open`, `sem_wait`, `sem_post`, `sem_close`, `sem_unlink`).
   - Clean unlinking and removal of all IPC objects upon completion.
5. **FR5 – Resource-Safety Analysis (Banker's Algorithm)**:
   - Deadlock avoidance safety algorithm tracking Allocation, Maximum, Need ($Need = Max - Allocation$), and Available vectors.
   - Computes and displays valid safe execution sequences for safe states.
   - Detects and reports unsafe states and stalled processes.
6. **FR6 – Worker Process Monitoring & Failure Detection**:
   - Child process instantiation via `fork()`.
   - Status collection and diagnostic reporting via `waitpid()`.
   - Distinguishes normal termination (`WIFEXITED`, exit code 0) from simulated controlled failures (`WEXITSTATUS` non-zero) and signal terminations (`WIFSIGNALED`).
7. **FR7 – Resource Limits & System Cleanup**:
   - Enforces kernel-level process quotas using `setrlimit()` (e.g. `RLIMIT_CPU` triggering `SIGXCPU`).
   - Audits system state to ensure zero orphaned child processes or leaked IPC handles in `/dev/shm`.
8. **FR8 – Unified CLI & Interactive Interface**:
   - Complete command-line argument dispatching with strict syntax validation and informative error messages.
   - Interactive terminal menu for manual exploration.

---

## Directory Structure

```text
.
├── Makefile               # Build automation and packaging script
├── README.md              # Public documentation
├── data/                  # Benchmark datasets
│   ├── banker_safe.txt    # Banker's safe state test configuration
│   ├── banker_unsafe.txt  # Banker's unsafe state test configuration
│   └── jobs.txt           # Shared laboratory job specification
├── include/               # Header interfaces
│   ├── banker.h           # Banker's algorithm declarations
│   ├── common.h           # System utilities, formatting, and timing
│   ├── ipc.h              # Shared memory and semaphore interface
│   ├── job.h              # Job data structures and lifecycle states
│   ├── logger.h           # Multi-threaded bounded queue logger
│   ├── rlimit_demo.h      # Process resource limits interface
│   ├── scheduler.h        # CPU scheduling declarations
│   └── worker.h           # Worker lifecycle and failure detection
└── src/                   # C source implementations
    ├── banker.c           # Banker's algorithm implementation
    ├── common.c           # Shared utility implementations
    ├── ipc.c              # POSIX IPC coordination implementation
    ├── job.c              # Job loader and table display
    ├── logger.c           # Producer-consumer thread logger
    ├── main.c             # CLI dispatcher and interactive menu
    ├── rlimit_demo.c      # Resource limit & audit routines
    ├── scheduler.c        # FCFS, Round Robin, and Priority logic
    └── worker.c           # Fork/waitpid lifecycle implementation
```

---

## Prerequisites & Compilation

CampusCompute is designed for Linux / Ubuntu (including WSL and native Linux VMs).

### Requirements
- GCC compiler (`gcc` 9.0+)
- GNU Make (`make`)
- POSIX Real-Time & Thread Libraries (`-pthread`, `-lrt`)

### Build Instructions

To build the executable from source:
```bash
make clean
make
```
The compiled binary `campuscompute` will be generated in the root directory.

---

## Command Reference

CampusCompute supports both standalone subcommands and an interactive menu.

| Command | Arguments | Description |
|---|---|---|
| `./campuscompute` | *(none)* | Launches the interactive terminal menu |
| `./campuscompute help` | | Displays CLI syntax and supported commands |
| `./campuscompute jobs` | `[FILE]` | Loads and validates job specifications (FR1) |
| `./campuscompute fcfs` | `[FILE]` | Executes First-Come, First-Served scheduling (FR2) |
| `./campuscompute rr` | `[FILE] [QUANTUM]` | Executes Round Robin scheduling with time quantum (FR2) |
| `./campuscompute priority` | `[FILE]` | Executes non-preemptive Priority scheduling (FR2) |
| `./campuscompute compare` | `[FILE] [QUANTUM]` | Compares FCFS, RR, and Priority on the same job set (FR2) |
| `./campuscompute logger` | `[P] [E] [C]` | Bounded producer-consumer thread activity logger (FR3) |
| `./campuscompute ipc` | `[ITERATIONS]` | POSIX Shared Memory & Named Semaphore IPC demo (FR4) |
| `./campuscompute banker` | `[FILE]` | Banker's safety check showing safe sequence (FR5) |
| `./campuscompute banker-unsafe` | `[FILE]` | Banker's safety check identifying unsafe state (FR5) |
| `./campuscompute worker-normal` | | Child process creation and normal exit monitoring (FR6) |
| `./campuscompute worker-fail` | `[code\|signal]` | Child process simulated failure detection (FR6) |
| `./campuscompute rlimit` | | Demonstrates kernel resource limits via `setrlimit` (FR7) |
| `./campuscompute cleanup` | | Verifies thread termination & IPC cleanup audit (FR7) |
| `./campuscompute demo` | | Runs complete automated end-to-end demonstration |

---

## Log Output

All operations performed by CampusCompute are recorded by the background consumer thread into:
```text
logs/campuscompute.log
```
Each entry includes timestamps, microsecond sequence numbers, subsystem origin tags, and message details.

---

## Clean-up

To clean up object files, generated binaries, and temporary logs:
```bash
make clean
```
