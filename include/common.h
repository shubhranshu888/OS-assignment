/*
 * common.h - Shared constants and small utility helpers used by every
 *            CampusCompute module.
 */
#ifndef CAMPUSCOMPUTE_COMMON_H
#define CAMPUSCOMPUTE_COMMON_H

#include <stddef.h>

#define CC_PROGRAM_NAME     "campuscompute"
#define CC_VERSION          "1.0"

/* Default files (paths are relative to the project directory). */
#define CC_LOG_DIR          "logs"
#define CC_LOG_FILE         "logs/campuscompute.log"
#define CC_DEFAULT_JOBS     "data/jobs.txt"
#define CC_SAFE_BANKER      "data/banker_safe.txt"
#define CC_UNSAFE_BANKER    "data/banker_unsafe.txt"
#define CC_DEFAULT_QUANTUM  2

/* Process exit codes used by every command. */
#define CC_OK               0   /* command completed successfully          */
#define CC_ERR_RUNTIME      1   /* bad input data or a system call failed  */
#define CC_ERR_USAGE        2   /* wrong command-line syntax               */

/* Pretty headings for terminal output. */
void print_banner(const char *title);
void print_section(const char *title);

/*
 * Strict integer parser (rejects "12abc", "", " 5").
 * Returns 0 on success, -1 if the text is not an integer, -2 if out of range.
 */
int parse_long(const char *text, long min, long max, long *out);

/* Same as parse_long() but prints a friendly error message naming the
 * argument. Returns 0 on success, -1 on any error. */
int parse_long_arg(const char *text, const char *what, long min, long max, long *out);

/* Number of threads in this process, read from /proc/self/status (-1 if unknown). */
int get_thread_count(void);

/* Sleep for the given number of milliseconds (restarts after signals). */
void sleep_ms(long ms);

/* Seconds from a monotonic clock - only differences are meaningful. */
double monotonic_seconds(void);

/* Wall-clock time as "YYYY-MM-DD HH:MM:SS.mmm". */
void timestamp_now(char *buf, size_t len);

/* Create the logs/ directory if it does not exist. Returns 0 on success. */
int ensure_log_dir(void);

/* Flush every stdio stream. Always called before fork() so buffered text
 * is not duplicated into the child process. */
void flush_all_output(void);

/* Terminate a forked child: flush stdout/stderr, then _exit() so the child
 * never runs the parent's atexit handlers or flushes the parent's buffers. */
_Noreturn void child_exit(int code);

/* Human-readable description of a waitpid() status value. */
const char *describe_wait_status(int status, char *buf, size_t len);

#endif /* CAMPUSCOMPUTE_COMMON_H */

