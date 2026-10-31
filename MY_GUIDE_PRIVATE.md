# CampusCompute: Student Private Master Execution & Viva Guide

> [!CAUTION]
> **CONFIDENTIAL STUDENT MASTER COPY**
> **DO NOT LEAVE THIS FILE IN THE REPOSITORY WHEN MAKING IT PUBLIC!**
> This file is tailored specifically for your coursework execution, exact screenshot collection (S01–S24), code explanation, and viva defense. Delete this file (`rm MY_GUIDE_PRIVATE.md`) before making your GitHub repository public.

---

## Table of Contents
1. [Assignment & Coursework Context](#1-assignment--coursework-context)
2. [Zero-Local-Footprint & Disposable VM Workflow](#2-zero-local-footprint--disposable-vm-workflow)
3. [Configuring Your Terminal & SRN Prompt (MANDATORY)](#3-configuring-your-terminal--srn-prompt-mandatory)
4. [File-by-File & Function-by-Function Architecture](#4-file-by-file--function-by-function-architecture)
5. [Complete Screenshot Execution Script (S01 to S24)](#5-complete-screenshot-execution-script-s01-to-s24)
6. [Packaging the Submission PDF & ZIP](#6-packaging-the-submission-pdf--zip)
7. [Viva Voce & Technical Defense Q&A](#7-viva-voce--technical-defense-qa)

---

## 1. Assignment & Coursework Context

- **Course**: OPERATING SYSTEMS (UE25CS242A)
- **Title**: Level 3 Mini-Project: CampusCompute – Intelligent Job Scheduling and Resource Management
- **Primary Objective**: Demonstrate fundamental operating systems concepts (job management, CPU scheduling, POSIX thread synchronization, POSIX shared memory/semaphores IPC, Banker's deadlock safety algorithm, worker failure detection via `fork()`/`waitpid()`, and kernel resource limits via `setrlimit()`) inside a unified C program on Ubuntu Linux.
- **Submission Requirements**:
  1. Source code and Makefile.
  2. One single PDF named exactly with your SRN (e.g., `PES1UG23CS999.pdf`), containing screenshots **S01 to S24** with one short sentence explaining each screenshot.
  3. Submission ZIP containing the code and build files if required.

---

## 2. Setting Up on Your Ubuntu Virtual Machine

### Step 2.1: Transfer or Clone the Repository to Your Ubuntu VM
Inside your Ubuntu terminal:
```bash
# Option A: Clone directly using Git
git clone https://github.com/shubhranshu888/OS-assignment.git
cd OS-assignment

# Option B: If you transferred a ZIP file from Windows
unzip OS-assignment.zip -d OS-assignment
cd OS-assignment
```

### Step 2.2: Verify Required Build Tools
Install `gcc`, `make`, `zip`, and POSIX development utilities if not already present:
```bash
sudo apt update
sudo apt install -y build-essential gcc make zip
```

---

## 3. Configuring Your Terminal & SRN Prompt (MANDATORY)

> [!IMPORTANT]
> The assignment rules state:
> *"Every terminal screenshot must show the student's complete, actual SRN in the terminal prompt. Printing the SRN inside a file is not sufficient. Before capturing screenshots, configure the prompt with your own full SRN."*

Run these two commands in your Ubuntu terminal before taking any screenshots (replace `PES1UGXXXXXXXX` with your actual university SRN):

```bash
export SRN='PES1UGXXXXXXXX'
export PS1='[$SRN] \w\$ '
```

Your prompt will immediately transform into:
```text
[PES1UGXXXXXXXX] ~/OS-assignment$ 
```
Every screenshot you take from this moment onward will clearly display your SRN in the prompt.

---

## 4. File-by-File & Function-by-Function Architecture

### `include/common.h` & `src/common.c`
- **Purpose**: Core utility library shared across all modules.
- **Key Functions**:
  - `print_banner()`, `print_section()`: Outputs clean visual headers.
  - `parse_long()`, `parse_long_arg()`: Secure integer parsing with bounds validation (rejects invalid input like `12abc`).
  - `get_thread_count()`: Parses `/proc/self/status` to accurately report the number of active threads in the process.
  - `monotonic_seconds()`, `sleep_ms()`: High-precision timing utilities using `clock_gettime(CLOCK_MONOTONIC)`.
  - `child_exit()`: Flushes stdio buffers and calls `_exit()`, preventing forked child processes from re-executing parent `atexit` handlers.
  - `describe_wait_status()`: Decodes raw `waitpid()` integer statuses into human-readable strings (`WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED`, `WTERMSIG`).

### `include/job.h` & `src/job.c` (FR1)
- **Purpose**: Process lifecycle model and job input loader.
- **Key Structures & Functions**:
  - `job_t`: Represents a job with `id`, `arrival`, `burst`, `priority`, and lifecycle state (`JOB_NEW`, `JOB_READY`, `JOB_RUNNING`, `JOB_COMPLETED`).
  - `load_jobs()`: Parses the job specification file (`data/jobs.txt`). Validates that arrival $\ge 0$, burst $> 0$, priorities are valid (1 = highest), and job IDs are alphanumeric and unique.
  - `print_job_table()`: Renders the validated job set in a table.
  - `cmd_jobs()`: Standalone CLI entrypoint.

### `include/scheduler.h` & `src/scheduler.c` (FR2)
- **Purpose**: CPU scheduling policies and performance metrics.
- **Algorithms**:
  - `schedule_fcfs()`: Non-preemptive First-Come, First-Served scheduling. Dispatches jobs based on arrival time, resolving ties by job input order.
  - `schedule_rr()`: Preemptive Round Robin scheduling. Implements an explicit circular ready queue where jobs run for at most `quantum` time units before being placed back at the tail.
  - `schedule_priority()`: Non-preemptive priority scheduling. Dispatches the ready job with the lowest priority integer (highest priority).
- **Metrics Calculated**:
  - Turnaround Time ($TAT = Completion - Arrival$)
  - Waiting Time ($WT = TAT - Burst$)
  - Response Time ($RT = FirstDispatch - Arrival$)
  - CPU Utilization ($\% = (TotalBurst / TotalElapsed) \times 100$)
- **Visualization**: Generates an ASCII Gantt timeline chart (`| J1 | J2 | ... |`).
- **CLI Commands**: `fcfs`, `rr`, `priority`, `compare`.

### `include/logger.h` & `src/logger.c` (FR3)
- **Purpose**: Multi-threaded bounded activity logging queue.
- **Synchronization Design**:
  - Bounded circular buffer with capacity $C$ slots.
  - `pthread_mutex_t lock`: Guards all operations on the queue (insertion, removal, counters).
  - `pthread_cond_t not_full`: Producer threads sleep on this condition variable when `count == capacity`.
  - `pthread_cond_t not_empty`: Consumer thread sleeps on this condition variable when `count == 0`.
  - Both condition waits are wrapped in `while` loops to guard against spurious wakeups.
- **Thread Lifecycle**:
  - Dedicated background consumer thread consumes entries, performs file I/O outside the mutex lock (to avoid stalling producers), and writes to `logs/campuscompute.log`.
  - Orderly shutdown via `logger_stop()` drains remaining messages, writes a final termination audit entry, and joins via `pthread_join()`.
- **CLI Command**: `logger [PRODUCERS] [EVENTS] [CAPACITY]`.

### `include/ipc.h` & `src/ipc.c` (FR4)
- **Purpose**: Inter-process communication using POSIX shared memory and named semaphores.
- **System Calls Used**:
  - `shm_open()`, `ftruncate()`, `mmap()`, `munmap()`, `shm_unlink()`: Manages shared memory segment under `/dev/shm/campuscompute_shm_<pid>`.
  - `sem_open()`, `sem_wait()`, `sem_post()`, `sem_close()`, `sem_unlink()`: Coordinates parent-child handshaking without busy waiting.
- **Workflow**:
  1. Parent writes a computational task into the mapped structure.
  2. Parent signals `sem_req`.
  3. Forked child waits on `sem_req`, processes the input, writes the result, and signals `sem_resp`.
  4. Parent waits on `sem_resp`, displays the result, and proceeds to the next iteration.
  5. Parent unlinks all shared memory and semaphores upon completion.
- **CLI Command**: `ipc [ITERATIONS]`.

### `include/banker.h` & `src/banker.c` (FR5)
- **Purpose**: Banker's Algorithm for deadlock avoidance and resource safety verification.
- **Matrices Tracked**:
  - $Allocation[i][j]$: Instances of resource $j$ currently held by process $i$.
  - $Max[i][j]$: Maximum instances of resource $j$ process $i$ may claim.
  - $Need[i][j] = Max[i][j] - Allocation[i][j]$: Remaining instances process $i$ requires.
  - $Available[j]$: Instances of resource $j$ currently free.
- **Key Functions**:
  - `check_banker_safety()`: Simulates system execution. Computes Work vector evolution and produces the safe execution sequence (e.g. `<P1 -> P3 -> P0 -> P2 -> P4>`).
  - Identifies unsafe states when no process's remaining need can be satisfied by the available pool, preventing potential deadlock.
- **CLI Commands**: `banker` (safe state) and `banker-unsafe` (unsafe state).

### `include/worker.h` & `src/worker.c` (FR6)
- **Purpose**: Worker process lifecycle monitoring and failure detection.
- **System Calls**: `fork()`, `waitpid()`, `WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED`, `WTERMSIG`.
- **Modes**:
  - `worker-normal`: Child executes a simulated simulation job, exits with status 0. Parent verifies normal termination.
  - `worker-fail`: Child simulates a controlled non-zero exit code (e.g. 42) or abnormal signal (`SIGABRT`). Parent catches and diagnoses the exact failure reason.

### `include/rlimit_demo.h` & `src/rlimit_demo.c` (FR7)
- **Purpose**: Linux kernel process resource limits and cleanup audit.
- **Mechanisms**:
  - `getrlimit()` and `setrlimit()`: Applies `RLIMIT_CPU` (1 second soft limit) to a worker child.
  - The worker enters an intensive CPU loop. The Linux kernel delivers `SIGXCPU` (signal 24). Parent catches the signal termination and confirms OS-level quota enforcement.
  - `cleanup`: Audits `/proc/self/status` for residual threads and checks `/dev/shm` to verify zero dangling IPC handles.

### `src/main.c` (FR8)
- **Purpose**: Top-level CLI dispatcher, interactive menu, signal handler (SIGINT/SIGTERM), and end-to-end automated demonstration (`demo`).

---

## 5. Complete Screenshot Execution Script (S01 to S24)

Execute each command below in your Ubuntu terminal. After each command, take a clean screenshot showing your prompt with your SRN. Under each screenshot in your submission PDF, paste the provided one-line explanation.

---

### **S01 – Environment and SRN prompt**
- **Command**:
  ```bash
  export SRN='YOUR_ACTUAL_SRN'
  export PS1='[$SRN] \w\$ '
  uname -a && pwd
  ```
- **One-line explanation**:
  > Ubuntu Linux environment initialized with the student's full SRN configured in the terminal prompt.

---

### **S02 – Compiler and build tools**
- **Command**:
  ```bash
  gcc --version | head -n 1 && make --version | head -n 1
  ```
- **One-line explanation**:
  > Verification of installed GCC C compiler and GNU Make build automation tools.

---

### **S03 – Project files**
- **Command**:
  ```bash
  ls -la && ls -la include src data
  ```
- **One-line explanation**:
  > Complete project directory structure showing modular header files, C source implementations, datasets, and Makefile.

---

### **S04 – Clean build**
- **Command**:
  ```bash
  make clean && make
  ```
- **One-line explanation**:
  > Clean compilation of CampusCompute from source using GCC with strict warning flags (-Wall -Wextra).

---

### **S05 – CLI help/usage**
- **Command**:
  ```bash
  ./campuscompute help
  ```
- **One-line explanation**:
  > CampusCompute command-line help interface displaying all supported commands, parameters, and usage instructions.

---

### **S06 – FCFS scheduling**
- **Command**:
  ```bash
  ./campuscompute fcfs data/jobs.txt
  ```
- **One-line explanation**:
  > First-Come First-Served CPU scheduling showing execution timeline, completion times, and computed TAT and WT metrics.

---

### **S07 – Round Robin scheduling**
- **Command**:
  ```bash
  ./campuscompute rr data/jobs.txt 2
  ```
- **One-line explanation**:
  > Round Robin scheduling with time quantum 2 displaying time-sliced Gantt execution and improved response times.

---

### **S08 – Priority scheduling**
- **Command**:
  ```bash
  ./campuscompute priority data/jobs.txt
  ```
- **One-line explanation**:
  > Non-preemptive Priority scheduling showing dispatch order based on job rank and tie-breaking by arrival time.

---

### **S09 – Scheduling comparison**
- **Command**:
  ```bash
  ./campuscompute compare data/jobs.txt 2
  ```
- **One-line explanation**:
  > Comprehensive comparative evaluation of FCFS, Round Robin, and Priority scheduling algorithms on an identical job set.

---

### **S10 – Producer-consumer logger**
- **Command**:
  ```bash
  ./campuscompute logger 3 5 4
  ```
- **One-line explanation**:
  > Multi-threaded activity logger demonstrating concurrent producer threads queuing events into a bounded circular buffer.

---

### **S11 – Thread synchronization and termination**
- **Command**:
  ```bash
  ./campuscompute logger 2 4 3
  ```
- **One-line explanation**:
  > Producer and consumer thread synchronization via mutex and condition variables, with orderly shutdown and thread joining.

---

### **S12 – Shared-memory IPC**
- **Command**:
  ```bash
  ./campuscompute ipc 3
  ```
- **One-line explanation**:
  > Inter-process communication between coordinator and worker process exchanging computational data via POSIX shared memory.

---

### **S13 – Semaphore coordination and cleanup**
- **Command**:
  ```bash
  ./campuscompute ipc 4
  ```
- **One-line explanation**:
  > Process synchronization using POSIX named semaphores followed by verified unlinking and release of created IPC resources.

---

### **S14 – Banker's safe state**
- **Command**:
  ```bash
  ./campuscompute banker data/banker_safe.txt
  ```
- **One-line explanation**:
  > Banker's safety algorithm evaluating Allocation and Max matrices to determine a safe state and a valid safe sequence.

---

### **S15 – Banker's unsafe state**
- **Command**:
  ```bash
  ./campuscompute banker-unsafe data/banker_unsafe.txt
  ```
- **One-line explanation**:
  > Banker's algorithm identifying an unsafe resource allocation state and warning against potential deadlock.

---

### **S16 – Normal worker process**
- **Command**:
  ```bash
  ./campuscompute worker-normal
  ```
- **One-line explanation**:
  > Child worker creation via fork() and parent status collection via waitpid() confirming normal exit with status 0.

---

### **S17 – Simulated worker failure**
- **Command**:
  ```bash
  ./campuscompute worker-fail 42
  ```
- **One-line explanation**:
  > Controlled simulated worker failure with parent-side waitpid() diagnostics accurately catching non-zero exit code 42.

---

### **S18 – Resource-limit demonstration**
- **Command**:
  ```bash
  ./campuscompute rlimit
  ```
- **One-line explanation**:
  > Process CPU quota enforcement using setrlimit(RLIMIT_CPU) with kernel termination via signal 24 (SIGXCPU).

---

### **S19 – Invalid input/command**
- **Command**:
  ```bash
  ./campuscompute invalid_cmd_xyz
  ```
- **One-line explanation**:
  > Robust error handling and usage feedback when an unrecognized command or malformed argument is supplied.

---

### **S20 – Cleanup verification**
- **Command**:
  ```bash
  ./campuscompute cleanup
  ```
- **One-line explanation**:
  > System audit confirming all child processes and threads have terminated and zero leaked IPC objects remain in /dev/shm.

---

### **S21 – Integrated execution**
- **Command**:
  ```bash
  ./campuscompute demo
  ```
- **One-line explanation**:
  > End-to-end automated integrated demonstration executing the full CampusCompute workflow across all functional modules.

---

### **S22 – Generated logs**
- **Command**:
  ```bash
  tail -n 15 logs/campuscompute.log
  ```
- **One-line explanation**:
  > Inspecting the persistent campuscompute.log file showing structured, timestamped entries generated by the consumer thread.

---

### **S23 – Final source and build files**
- **Command**:
  ```bash
  ls -lh campuscompute Makefile include/*.h src/*.c
  ```
- **One-line explanation**:
  > Final verified source files, header interfaces, Makefile, and the built campuscompute binary.

---

### **S24 – Submission ZIP**
- **Command**:
  ```bash
  make zip && unzip -t campuscompute_submission.zip
  ```
- **One-line explanation**:
  > Packaging the project source files into campuscompute_submission.zip with a successful archive integrity check.

---

## 6. Packaging the Submission PDF & ZIP

### 1. Preparing the PDF
1. Open LibreOffice Writer, Google Docs, or Microsoft Word.
2. For each number from **S01 to S24**:
   - Insert the corresponding terminal screenshot (ensuring your prompt `[$SRN]` is clearly visible).
   - Below the screenshot, paste the exact one-line explanation provided above.
3. Export the document as a PDF.
4. Name the PDF strictly with your own SRN:
   ```text
   PES1UGXXXXXXXX.pdf
   ```

### 2. Preparing the ZIP
Inside your Ubuntu terminal, run:
```bash
make zip
```
This generates `campuscompute_submission.zip` containing all code, headers, Makefile, and datasets.

---

## 7. Viva Voce & Technical Defense Q&A

### Q1: How does the Banker's algorithm determine if a state is safe?
**Answer**:
The algorithm tracks Available resources and builds the Need matrix ($Need[i][j] = Maximum[i][j] - Allocation[i][j]$). It initializes $Work = Available$ and finds a process $i$ whose remaining need satisfies $Need[i] \le Work$. It assumes process $i$ finishes and releases its allocated resources ($Work = Work + Allocation[i]$). If all processes can be added to the sequence, the state is SAFE. If at any point no remaining process can be satisfied, the state is UNSAFE (potential deadlock).

### Q2: Why are condition variable waits (`pthread_cond_wait`) enclosed in a `while` loop rather than an `if` statement?
**Answer**:
To protect against **spurious wakeups** and race conditions. A thread may be woken up by the kernel even if no signal occurred, or another producer/consumer thread may have acquired the lock first and consumed the available slot. Re-evaluating the condition predicate in a `while` loop ensures the thread only proceeds when the condition is genuinely satisfied.

### Q3: What is the difference between POSIX unnamed semaphores and named semaphores?
**Answer**:
- **Unnamed semaphores** (`sem_init`) reside in shared process memory and require memory sharing attributes.
- **Named semaphores** (`sem_open`) have a system-wide name in `/dev/shm` (e.g. `/campuscompute_sem`), making them ideal for coordinating unrelated or forked processes across the operating system.

### Q4: In worker failure detection, why do you check `WIFEXITED()` before `WEXITSTATUS()`?
**Answer**:
`WEXITSTATUS(status)` is only defined and valid if the child process exited normally (e.g., via `exit()` or `return`). If the process was terminated by an unhandled signal (such as `SIGSEGV` or `SIGKILL`), reading `WEXITSTATUS` yields invalid data. Therefore, `WIFEXITED(status)` must be checked first; if false, `WIFSIGNALED(status)` and `WTERMSIG(status)` are used.

### Q5: How did you implement and verify `RLIMIT_CPU` in FR7?
**Answer**:
We used `setrlimit(RLIMIT_CPU, &limit)` inside a forked child process to set a soft limit of 1 second CPU time. The child entered a computation loop. When the CPU time accumulated by the process reached 1 second, the Linux kernel generated and delivered the `SIGXCPU` signal. The parent detected this via `waitpid()` with `WIFSIGNALED` returning true and `WTERMSIG == SIGXCPU` (signal 24).
