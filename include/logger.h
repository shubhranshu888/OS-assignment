/*
 * logger.h - FR3: concurrent activity logger.
 *
 * A bounded circular buffer shared by many producer threads and one
 * consumer thread. A pthread mutex protects the buffer; two condition
 * variables ("not_full" and "not_empty") make producers wait while the
 * buffer is full and the consumer wait while it is empty. The consumer
 * thread is the only writer of the log file.
 */
#ifndef CAMPUSCOMPUTE_LOGGER_H
#define CAMPUSCOMPUTE_LOGGER_H

#define LOG_MSG_LEN       200
#define LOG_SRC_LEN       24
#define LOGGER_DEFAULT_CAPACITY 16
#define LOGGER_MAX_CAPACITY     1024

typedef struct {
    int  capacity;           /* number of slots in the bounded buffer        */
    int  verbose;            /* 1 = print producer/consumer activity on screen */
    long consumer_delay_ms;  /* artificial delay per entry (demo only)        */
} logger_config_t;

typedef struct {
    long produced;           /* entries accepted into the buffer             */
    long consumed;           /* entries written to the log file              */
    long producer_waits;     /* times a producer blocked on a FULL buffer    */
    long consumer_waits;     /* times the consumer blocked on an EMPTY buffer */
    int  max_occupancy;      /* highest number of entries ever in the buffer */
} logger_stats_t;

/* Start the consumer thread and open CC_LOG_FILE in append mode.
 * Returns 0 on success, -1 on failure (an error is printed). */
int  logger_start(const logger_config_t *cfg);

/* Stop accepting entries, let the consumer drain the buffer, join it and
 * close the log file. Safe to call when the logger is not running. */
void logger_stop(logger_stats_t *stats_out);

int  logger_is_running(void);

/*
 * Producer API: queue one log entry (blocks while the buffer is full).
 * Calls made from a forked child process, or while the logger is stopped,
 * are ignored, so it is always safe to call.
 */
void cc_log(const char *source, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* CLI: logger [PRODUCERS] [EVENTS_PER_PRODUCER] [CAPACITY] */
int  cmd_logger(int argc, char **argv);

#endif /* CAMPUSCOMPUTE_LOGGER_H */

