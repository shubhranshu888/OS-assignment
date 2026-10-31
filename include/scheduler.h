/*
 * scheduler.h - FR2: CPU scheduling algorithms and metric calculations.
 *
 * Algorithms implemented:
 *   - First-Come, First-Served (FCFS)
 *   - Round Robin (RR) with configurable time quantum
 *   - Non-preemptive Priority Scheduling (lower integer = higher priority)
 *
 * Tie-breaking rule:
 *   When arrival times or priorities are identical, ties are broken by
 *   file input order (Job ID order).
 */
#ifndef CAMPUSCOMPUTE_SCHEDULER_H
#define CAMPUSCOMPUTE_SCHEDULER_H

#include "job.h"

#define MAX_TIMELINE_SLOTS 1024

typedef struct {
    char job_id[JOB_ID_LEN];
    int start_time;
    int end_time;
} timeline_slot_t;

typedef struct {
    timeline_slot_t slots[MAX_TIMELINE_SLOTS];
    int count;
} timeline_t;

typedef struct {
    double avg_turnaround;
    double avg_waiting;
    double avg_response;
    int total_time;
    int cpu_busy_time;
    double cpu_utilization;
} schedule_metrics_t;

/* Run algorithms */
int schedule_fcfs(job_set_t *set, timeline_t *timeline, schedule_metrics_t *metrics);
int schedule_rr(job_set_t *set, int quantum, timeline_t *timeline, schedule_metrics_t *metrics);
int schedule_priority(job_set_t *set, timeline_t *timeline, schedule_metrics_t *metrics);

/* Display helpers */
void print_schedule_results(const char *algo_name, const job_set_t *set,
                            const timeline_t *timeline, const schedule_metrics_t *metrics);
void print_comparison_table(const schedule_metrics_t *fcfs,
                            const schedule_metrics_t *rr,
                            const schedule_metrics_t *prio,
                            int quantum);

/* CLI entrypoints */
int cmd_fcfs(int argc, char **argv);
int cmd_rr(int argc, char **argv);
int cmd_priority(int argc, char **argv);
int cmd_compare(int argc, char **argv);

#endif /* CAMPUSCOMPUTE_SCHEDULER_H */
