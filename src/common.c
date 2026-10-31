/*
 * common.c - Utility helpers shared by all modules (parsing, timing,
 *            formatting, safe child termination).
 */
#include "common.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void print_banner(const char *title)
{
    printf("\n====================================================================\n");
    printf("  %s\n", title);
    printf("====================================================================\n");
}

void print_section(const char *title)
{
    printf("\n---- %s ----\n", title);
}

int parse_long(const char *text, long min, long max, long *out)
{
    char *end = NULL;
    long value;

    if (text == NULL || *text == '\0')
        return -1;

    errno = 0;
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0')
        return -1;                       /* not a (complete) integer */
    if (errno == ERANGE || value < min || value > max)
        return -2;                       /* integer, but outside the range */

    *out = value;
    return 0;
}

int parse_long_arg(const char *text, const char *what, long min, long max, long *out)
{
    int rc = parse_long(text, min, max, out);

    if (rc == -1) {
        fprintf(stderr, "Error: %s '%s' is not a valid integer.\n", what, text);
        return -1;
    }
    if (rc == -2) {
        fprintf(stderr, "Error: %s %s is out of range (allowed %ld..%ld).\n",
                what, text, min, max);
        return -1;
    }
    return 0;
}

int get_thread_count(void)
{
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];
    int threads = -1;

    if (fp == NULL)
        return -1;
    while (fgets(line, sizeof line, fp) != NULL) {
        if (strncmp(line, "Threads:", 8) == 0) {
            threads = atoi(line + 8);
            break;
        }
    }
    fclose(fp);
    return threads;
}

void sleep_ms(long ms)
{
    struct timespec req, rem;

    if (ms <= 0)
        return;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    /* nanosleep() returns early if a signal arrives; continue with the rest. */
    while (nanosleep(&req, &rem) == -1 && errno == EINTR)
        req = rem;
}

double monotonic_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void timestamp_now(char *buf, size_t len)
{
    struct timespec ts;
    struct tm tmv;
    char base[32];

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tmv);
    strftime(base, sizeof base, "%Y-%m-%d %H:%M:%S", &tmv);
    snprintf(buf, len, "%s.%03ld", base, (long)(ts.tv_nsec / 1000000L));
}

int ensure_log_dir(void)
{
    if (mkdir(CC_LOG_DIR, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "Error: cannot create directory '%s': %s\n",
                CC_LOG_DIR, strerror(errno));
        return -1;
    }
    return 0;
}

void flush_all_output(void)
{
    fflush(NULL);
}

_Noreturn void child_exit(int code)
{
    fflush(stdout);
    fflush(stderr);
    _exit(code);
}

const char *describe_wait_status(int status, char *buf, size_t len)
{
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == 0)
            snprintf(buf, len, "exited normally (exit status 0)");
        else
            snprintf(buf, len, "exited with NON-ZERO status %d", code);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        snprintf(buf, len, "terminated by signal %d (%s)", sig, strsignal(sig));
    } else {
        snprintf(buf, len, "unrecognised wait status 0x%x", (unsigned)status);
    }
    return buf;
}

