/*
 * job.h - FR1: job representation, lifecycle states and job-file loading.
 */
#ifndef CAMPUSCOMPUTE_JOB_H
#define CAMPUSCOMPUTE_JOB_H

#define MAX_JOBS          64
#define JOB_ID_LEN        16      /* includes the terminating '\0' */
#define JOB_MAX_ARRIVAL   10000
#define JOB_MAX_BURST     1000
#define JOB_MAX_PRIORITY  99      /* 1 = highest priority */

/* Process lifecycle of a job inside the scheduler simulation. */
typedef enum {
    JOB_NEW = 0,      /* loaded from file, has not arrived yet          */
    JOB_READY,        /* arrived, waiting in the ready queue for the CPU */
    JOB_RUNNING,      /* currently on the (simulated) CPU                */
    JOB_COMPLETED     /* finished all of its CPU burst                   */
} job_status_t;

typedef struct {
    /* ---- input fields (from the job file) ---- */
    char id[JOB_ID_LEN];
    int  arrival;          /* arrival time (time units)                  */
    int  burst;            /* total CPU time needed (time units)         */
    int  priority;         /* smaller number = more important            */
    int  input_order;      /* line order in the file, used for ties      */

    /* ---- execution tracking (filled in by the scheduler) ---- */
    job_status_t status;
    int  remaining;        /* CPU time still needed                      */
    int  started;          /* 1 once the job has run for the first time  */
    int  start_time;       /* time of first dispatch                     */
    int  completion_time;
    int  turnaround_time;  /* completion - arrival                       */
    int  waiting_time;     /* turnaround - burst                         */
    int  response_time;    /* first start - arrival                      */
} job_t;

typedef struct {
    job_t jobs[MAX_JOBS];
    int   count;
} job_set_t;

/*
 * Load and validate a job file. Every invalid line is reported (with its
 * line number) on stderr. Returns 0 on success, -1 if the file cannot be
 * opened, -2 if the contents are invalid.
 */
int  load_jobs(const char *path, job_set_t *set);

/* Put every job back into state NEW with its full burst remaining. */
void reset_jobs(job_set_t *set);

const char *job_status_name(job_status_t status);
void print_job_table(const job_set_t *set);

/* CLI: jobs [FILE] */
int  cmd_jobs(int argc, char **argv);

#endif /* CAMPUSCOMPUTE_JOB_H */

