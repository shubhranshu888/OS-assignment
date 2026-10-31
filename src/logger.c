/*
 * logger.c - FR3: bounded producer-consumer activity logger (POSIX threads).
 *
 *   producers (any thread) --cc_log()--> [ bounded circular buffer ] --> consumer thread --> logs/campuscompute.log
 *
 * Synchronisation:
 *   lock       : pthread mutex protecting every field of the shared buffer.
 *   not_full   : condition variable; producers wait on it while count == capacity.
 *   not_empty  : condition variable; the consumer waits on it while count == 0.
 * Waits are always inside a while-loop because condition variables allow
 * spurious wake-ups and the condition must be re-checked after waking.
 *
 * Clean termination: logger_stop() sets 'shutdown', broadcasts both
 * conditions, the consumer drains whatever is left in the buffer, writes a
 * final line, exits, and is joined with pthread_join().
 */
#include "logger.h"
#include "common.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    long seq;                       /* global sequence number of the entry */
    char timestamp[32];
    char source[LOG_SRC_LEN];
    char message[LOG_MSG_LEN];
} log_entry_t;

/* All shared logger state lives in this single structure. */
static struct {
    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;

    log_entry_t *buffer;            /* circular buffer of 'capacity' slots */
    int head;                       /* next slot to read  (consumer)       */
    int tail;                       /* next slot to write (producers)      */
    int count;                      /* entries currently in the buffer     */

    int accepting;                  /* 1 while producers may add entries   */
    int shutdown;                   /* 1 once logger_stop() was called     */
    long next_seq;

    logger_config_t cfg;
    logger_stats_t  stats;

    pthread_t consumer;
    FILE *fp;
    pid_t owner_pid;                /* process that started the logger     */
    int write_errors;
} L = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .not_full = PTHREAD_COND_INITIALIZER,
    .not_empty = PTHREAD_COND_INITIALIZER,
};

/* ------------------------------------------------------------------ */
/* Consumer thread                                                     */
/* ------------------------------------------------------------------ */

static void write_entry(const log_entry_t *e)
{
    if (fprintf(L.fp, "%s | #%05ld | %-12s | %s\n",
                e->timestamp, e->seq, e->source, e->message) < 0 ||
        fflush(L.fp) != 0) {
        if (L.write_errors++ == 0)
            fprintf(stderr, "Error: writing to %s failed: %s\n", CC_LOG_FILE, strerror(errno));
    }
}

static void *consumer_main(void *arg)
{
    (void)arg;

    for (;;) {
        log_entry_t entry;
        int occupancy;

        pthread_mutex_lock(&L.lock);
        while (L.count == 0 && !L.shutdown) {
            L.stats.consumer_waits++;
            if (L.cfg.verbose)
                printf("  [CONSUMER ] buffer EMPTY (0/%d) -> waiting on not_empty\n", L.cfg.capacity);
            pthread_cond_wait(&L.not_empty, &L.lock);
        }
        if (L.count == 0 && L.shutdown) {       /* drained and asked to stop */
            pthread_mutex_unlock(&L.lock);
            break;
        }

        entry = L.buffer[L.head];               /* remove one entry */
        L.head = (L.head + 1) % L.cfg.capacity;
        L.count--;
        occupancy = L.count;
        L.stats.consumed++;
        pthread_cond_signal(&L.not_full);       /* a slot is now free */
        pthread_mutex_unlock(&L.lock);

        /* File I/O is done OUTSIDE the lock so producers are not blocked by disk writes. */
        write_entry(&entry);
        if (L.cfg.verbose)
            printf("  [CONSUMER ] wrote   #%05ld %-12s to log   (buffer %d/%d)\n",
                   entry.seq, entry.source, occupancy, L.cfg.capacity);
        sleep_ms(L.cfg.consumer_delay_ms);
    }

    {
        log_entry_t last;
        memset(&last, 0, sizeof last);
        timestamp_now(last.timestamp, sizeof last.timestamp);
        last.seq = L.next_seq;
        snprintf(last.source, sizeof last.source, "LOGGER");
        snprintf(last.message, sizeof last.message,
                 "consumer thread exiting cleanly: %ld entries written, buffer empty", L.stats.consumed);
        write_entry(&last);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int logger_is_running(void)
{
    int running;

    pthread_mutex_lock(&L.lock);
    running = L.accepting && L.owner_pid == getpid();
    pthread_mutex_unlock(&L.lock);
    return running;
}

int logger_start(const logger_config_t *cfg)
{
    int rc;

    if (cfg->capacity < 1 || cfg->capacity > LOGGER_MAX_CAPACITY) {
        fprintf(stderr, "Error: logger capacity must be 1..%d\n", LOGGER_MAX_CAPACITY);
        return -1;
    }
    if (logger_is_running()) {
        fprintf(stderr, "Error: logger is already running\n");
        return -1;
    }
    if (ensure_log_dir() != 0)
        return -1;

    L.fp = fopen(CC_LOG_FILE, "a");
    if (L.fp == NULL) {
        fprintf(stderr, "Error: cannot open log file '%s': %s\n", CC_LOG_FILE, strerror(errno));
        return -1;
    }
    L.buffer = calloc((size_t)cfg->capacity, sizeof *L.buffer);
    if (L.buffer == NULL) {
        fprintf(stderr, "Error: out of memory allocating log buffer\n");
        fclose(L.fp);
        L.fp = NULL;
        return -1;
    }

    L.cfg = *cfg;
    memset(&L.stats, 0, sizeof L.stats);
    L.head = L.tail = L.count = 0;
    L.next_seq = 1;
    L.shutdown = 0;
    L.write_errors = 0;
    L.owner_pid = getpid();

    rc = pthread_create(&L.consumer, NULL, consumer_main, NULL);
    if (rc != 0) {
        fprintf(stderr, "Error: pthread_create(consumer) failed: %s\n", strerror(rc));
        free(L.buffer);
        L.buffer = NULL;
        fclose(L.fp);
        L.fp = NULL;
        return -1;
    }

    pthread_mutex_lock(&L.lock);
    L.accepting = 1;
    pthread_mutex_unlock(&L.lock);

    cc_log("LOGGER", "logger started (pid %d, buffer capacity %d)", (int)getpid(), cfg->capacity);
    return 0;
}

void logger_stop(logger_stats_t *stats_out)
{
    int rc;

    pthread_mutex_lock(&L.lock);
    if (!L.accepting || L.owner_pid != getpid()) {
        pthread_mutex_unlock(&L.lock);
        if (stats_out != NULL)
            memset(stats_out, 0, sizeof *stats_out);
        return;
    }
    L.accepting = 0;                      /* refuse new entries               */
    L.shutdown = 1;                       /* tell the consumer to finish      */
    pthread_cond_broadcast(&L.not_empty); /* wake the consumer if it sleeps   */
    pthread_cond_broadcast(&L.not_full);  /* wake any producer still waiting  */
    pthread_mutex_unlock(&L.lock);

    rc = pthread_join(L.consumer, NULL);  /* consumer drains the buffer first */
    if (rc != 0)
        fprintf(stderr, "Error: pthread_join(consumer) failed: %s\n", strerror(rc));

    if (L.fp != NULL && fclose(L.fp) != 0)
        fprintf(stderr, "Error: closing %s failed: %s\n", CC_LOG_FILE, strerror(errno));
    L.fp = NULL;
    free(L.buffer);
    L.buffer = NULL;

    if (stats_out != NULL)
        *stats_out = L.stats;
}

void cc_log(const char *source, const char *fmt, ...)
{
    log_entry_t entry;
    va_list ap;
    int waited = 0;

    /* Build the entry before taking the lock to keep the critical section short. */
    memset(&entry, 0, sizeof entry);
    timestamp_now(entry.timestamp, sizeof entry.timestamp);
    snprintf(entry.source, sizeof entry.source, "%s", source);
    va_start(ap, fmt);
    vsnprintf(entry.message, sizeof entry.message, fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&L.lock);
    /* Ignore calls from forked children (they have no consumer thread) and
     * calls made while the logger is stopped. */
    if (!L.accepting || L.owner_pid != getpid()) {
        pthread_mutex_unlock(&L.lock);
        return;
    }

    while (L.count == L.cfg.capacity && !L.shutdown) {
        if (!waited) {
            L.stats.producer_waits++;
            waited = 1;
        }
        if (L.cfg.verbose)
            printf("  [%-9s] buffer FULL  (%d/%d) -> waiting on not_full\n",
                   source, L.count, L.cfg.capacity);
        pthread_cond_wait(&L.not_full, &L.lock);
    }
    if (L.shutdown) {                         /* stopped while we were waiting */
        pthread_mutex_unlock(&L.lock);
        return;
    }

    entry.seq = L.next_seq++;
    L.buffer[L.tail] = entry;                 /* insert one entry */
    L.tail = (L.tail + 1) % L.cfg.capacity;
    L.count++;
    L.stats.produced++;
    if (L.count > L.stats.max_occupancy)
        L.stats.max_occupancy = L.count;
    if (L.cfg.verbose)
        printf("  [%-9s] queued  #%05ld \"%s\"   (buffer %d/%d)\n",
               source, entry.seq, entry.message, L.count, L.cfg.capacity);
    pthread_cond_signal(&L.not_empty);        /* wake the consumer */
    pthread_mutex_unlock(&L.lock);
}

/* ------------------------------------------------------------------ */
/* CLI command: logger                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    int id;
    int events;
} producer_arg_t;

/* Each producer simulates one lab machine reporting job activity. */
static void *producer_main(void *arg)
{
    const producer_arg_t *p = arg;
    static const char *actions[] = { "JOB_SUBMIT", "JOB_START", "CPU_SAMPLE", "JOB_DONE" };
    char source[LOG_SRC_LEN];

    snprintf(source, sizeof source, "PRODUCER-%d", p->id);
    for (int i = 0; i < p->events; i++) {
        cc_log(source, "lab-node-%02d %-10s job=J%d%02d", p->id,
               actions[i % 4], p->id, i + 1);
        sleep_ms(5 + p->id * 2);            /* producers are faster than the consumer */
    }
    return NULL;
}

/* Print the last 'n' lines of the log file. */
static void print_log_tail(int n)
{
    FILE *fp = fopen(CC_LOG_FILE, "r");
    char (*lines)[320];
    char buf[320];
    long total = 0;

    if (fp == NULL) {
        fprintf(stderr, "Error: cannot read %s: %s\n", CC_LOG_FILE, strerror(errno));
        return;
    }
    lines = calloc((size_t)n, sizeof *lines);
    if (lines == NULL) {
        fclose(fp);
        return;
    }
    while (fgets(buf, sizeof buf, fp) != NULL) {
        snprintf(lines[total % n], sizeof lines[0], "%s", buf);
        total++;
    }
    fclose(fp);

    for (long i = (total > n ? total - n : 0); i < total; i++)
        printf("    %s", lines[i % n]);
    free(lines);
}

int cmd_logger(int argc, char **argv)
{
    long producers = 3, events = 6, capacity = 4;
    logger_config_t demo_cfg;
    logger_config_t session_cfg = { LOGGER_DEFAULT_CAPACITY, 0, 0 };
    logger_stats_t stats;
    pthread_t threads[16];
    producer_arg_t args[16];
    int created = 0, was_running, ok;

    if (argc > 3) {
        fprintf(stderr, "Usage: %s logger [PRODUCERS 1-16] [EVENTS_PER_PRODUCER 1-1000] [CAPACITY 1-%d]\n",
                CC_PROGRAM_NAME, LOGGER_MAX_CAPACITY);
        return CC_ERR_USAGE;
    }
    if ((argc >= 1 && parse_long_arg(argv[0], "PRODUCERS", 1, 16, &producers)) ||
        (argc >= 2 && parse_long_arg(argv[1], "EVENTS_PER_PRODUCER", 1, 1000, &events)) ||
        (argc >= 3 && parse_long_arg(argv[2], "CAPACITY", 1, LOGGER_MAX_CAPACITY, &capacity)))
        return CC_ERR_USAGE;

    print_banner("FR3: Concurrent activity logger (bounded producer-consumer, pthreads)");

    /* Stop the session logger so this demo controls the logger's lifetime. */
    was_running = logger_is_running();
    if (was_running)
        logger_stop(NULL);

    demo_cfg.capacity = (int)capacity;
    demo_cfg.verbose = 1;
    demo_cfg.consumer_delay_ms = 25;      /* slow consumer -> buffer becomes FULL */

    printf("Producers: %ld | events per producer: %ld | buffer capacity: %ld | consumer delay: %ld ms\n",
           producers, events, capacity, demo_cfg.consumer_delay_ms);
    printf("Sync: 1 pthread mutex + 2 condition variables (not_full, not_empty)\n");
    printf("Log file: %s\n", CC_LOG_FILE);
    printf("[MAIN] threads in process before start : %d\n", get_thread_count());

    if (logger_start(&demo_cfg) != 0)
        return CC_ERR_RUNTIME;
    printf("[MAIN] consumer thread started; threads now: %d\n", get_thread_count());

    for (int i = 0; i < producers; i++) {
        int rc;
        args[i].id = i + 1;
        args[i].events = (int)events;
        rc = pthread_create(&threads[i], NULL, producer_main, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "Error: pthread_create(producer %d) failed: %s\n", i + 1, strerror(rc));
            break;
        }
        created++;
    }
    printf("[MAIN] %d producer thread(s) started; threads now: %d\n", created, get_thread_count());

    for (int i = 0; i < created; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc != 0)
            fprintf(stderr, "Error: pthread_join(producer %d) failed: %s\n", i + 1, strerror(rc));
        else
            printf("[MAIN] producer %d finished and joined\n", i + 1);
    }

    logger_stop(&stats);
    printf("[MAIN] consumer drained the buffer and was joined\n");
    printf("[MAIN] threads in process after stop  : %d (only the main thread remains)\n",
           get_thread_count());

    print_section("Logger statistics");
    printf("Entries produced          : %ld\n", stats.produced);
    printf("Entries consumed (written): %ld\n", stats.consumed);
    printf("Producer waits (buffer FULL) : %ld\n", stats.producer_waits);
    printf("Consumer waits (buffer EMPTY): %ld\n", stats.consumer_waits);
    printf("Maximum buffer occupancy  : %d / %ld\n", stats.max_occupancy, capacity);

    ok = (created == producers) && (stats.produced == stats.consumed) &&
         (stats.produced == producers * events + 1) && stats.max_occupancy <= capacity;
    printf("Check: %s (every queued entry written exactly once, buffer never exceeded capacity)\n",
           ok ? "PASS" : "FAIL");

    print_section("Last log entries");
    print_log_tail(10);

    if (was_running)
        logger_start(&session_cfg);
    return ok ? CC_OK : CC_ERR_RUNTIME;
}

