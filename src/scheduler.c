/*
 * scheduler.c - Implementation of FCFS, Round Robin, and Priority Scheduling.
 */
#include "scheduler.h"
#include "common.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_timeline_slot(timeline_t *timeline, const char *id, int start, int end)
{
    if (timeline->count > 0) {
        timeline_slot_t *last = &timeline->slots[timeline->count - 1];
        if (strcmp(last->job_id, id) == 0 && last->end_time == start) {
            last->end_time = end;
            return;
        }
    }
    if (timeline->count < MAX_TIMELINE_SLOTS) {
        timeline_slot_t *slot = &timeline->slots[timeline->count++];
        snprintf(slot->job_id, sizeof slot->job_id, "%s", id);
        slot->start_time = start;
        slot->end_time = end;
    }
}

static void calculate_metrics(job_set_t *set, const timeline_t *timeline, schedule_metrics_t *metrics)
{
    double sum_ta = 0.0, sum_wt = 0.0, sum_rt = 0.0;
    int min_start = 1000000, max_end = 0;
    int total_burst = 0;

    for (int i = 0; i < set->count; i++) {
        job_t *j = &set->jobs[i];
        j->turnaround_time = j->completion_time - j->arrival;
        j->waiting_time = j->turnaround_time - j->burst;
        j->response_time = j->start_time - j->arrival;

        sum_ta += j->turnaround_time;
        sum_wt += j->waiting_time;
        sum_rt += j->response_time;
        total_burst += j->burst;
    }

    for (int i = 0; i < timeline->count; i++) {
        if (timeline->slots[i].start_time < min_start)
            min_start = timeline->slots[i].start_time;
        if (timeline->slots[i].end_time > max_end)
            max_end = timeline->slots[i].end_time;
    }

    metrics->avg_turnaround = set->count > 0 ? sum_ta / set->count : 0.0;
    metrics->avg_waiting = set->count > 0 ? sum_wt / set->count : 0.0;
    metrics->avg_response = set->count > 0 ? sum_rt / set->count : 0.0;
    metrics->total_time = max_end;
    metrics->cpu_busy_time = total_burst;
    metrics->cpu_utilization = max_end > 0 ? ((double)total_burst / max_end) * 100.0 : 0.0;
}

int schedule_fcfs(job_set_t *set, timeline_t *timeline, schedule_metrics_t *metrics)
{
    int current_time = 0;
    int completed = 0;
    timeline->count = 0;
    reset_jobs(set);

    cc_log("SCHEDULER", "Starting FCFS scheduling for %d jobs", set->count);

    while (completed < set->count) {
        int best = -1;
        for (int i = 0; i < set->count; i++) {
            if (set->jobs[i].status == JOB_COMPLETED)
                continue;
            if (set->jobs[i].arrival <= current_time) {
                if (best == -1) {
                    best = i;
                } else if (set->jobs[i].arrival < set->jobs[best].arrival) {
                    best = i;
                } else if (set->jobs[i].arrival == set->jobs[best].arrival &&
                           set->jobs[i].input_order < set->jobs[best].input_order) {
                    best = i;
                }
            }
        }

        if (best == -1) {
            /* CPU is idle, advance to earliest arrival among remaining jobs */
            int next_arrival = 1000000;
            for (int i = 0; i < set->count; i++) {
                if (set->jobs[i].status != JOB_COMPLETED && set->jobs[i].arrival < next_arrival)
                    next_arrival = set->jobs[i].arrival;
            }
            add_timeline_slot(timeline, "IDLE", current_time, next_arrival);
            current_time = next_arrival;
            continue;
        }

        job_t *j = &set->jobs[best];
        j->status = JOB_RUNNING;
        j->start_time = current_time;
        j->started = 1;
        current_time += j->burst;
        j->remaining = 0;
        j->completion_time = current_time;
        j->status = JOB_COMPLETED;
        completed++;

        add_timeline_slot(timeline, j->id, j->start_time, j->completion_time);
        cc_log("SCHEDULER", "Job %s executed [T=%d -> T=%d]", j->id, j->start_time, j->completion_time);
    }

    calculate_metrics(set, timeline, metrics);
    return 0;
}

int schedule_rr(job_set_t *set, int quantum, timeline_t *timeline, schedule_metrics_t *metrics)
{
    int current_time = 0;
    int completed = 0;
    int queue[MAX_JOBS * 64];
    int q_head = 0, q_tail = 0;
    int in_queue[MAX_JOBS];
    memset(in_queue, 0, sizeof in_queue);

    timeline->count = 0;
    reset_jobs(set);

    cc_log("SCHEDULER", "Starting Round Robin scheduling (quantum=%d) for %d jobs", quantum, set->count);

    /* Enqueue jobs that arrived at time 0 */
    for (int i = 0; i < set->count; i++) {
        if (set->jobs[i].arrival <= current_time) {
            queue[q_tail++] = i;
            in_queue[i] = 1;
            set->jobs[i].status = JOB_READY;
        }
    }

    while (completed < set->count) {
        if (q_head == q_tail) {
            /* Queue empty; jump clock to next arriving job */
            int next_arrival = 1000000;
            for (int i = 0; i < set->count; i++) {
                if (set->jobs[i].status != JOB_COMPLETED && set->jobs[i].arrival < next_arrival)
                    next_arrival = set->jobs[i].arrival;
            }
            if (next_arrival > current_time) {
                add_timeline_slot(timeline, "IDLE", current_time, next_arrival);
                current_time = next_arrival;
            }
            for (int i = 0; i < set->count; i++) {
                if (set->jobs[i].status != JOB_COMPLETED && !in_queue[i] && set->jobs[i].arrival <= current_time) {
                    queue[q_tail++] = i;
                    in_queue[i] = 1;
                    set->jobs[i].status = JOB_READY;
                }
            }
            continue;
        }

        int idx = queue[q_head++];
        in_queue[idx] = 0;
        job_t *j = &set->jobs[idx];

        if (!j->started) {
            j->started = 1;
            j->start_time = current_time;
        }
        j->status = JOB_RUNNING;

        int run_slice = (j->remaining > quantum) ? quantum : j->remaining;
        int slice_start = current_time;
        current_time += run_slice;
        j->remaining -= run_slice;

        add_timeline_slot(timeline, j->id, slice_start, current_time);

        /* Check for newly arriving jobs during this time slice */
        for (int i = 0; i < set->count; i++) {
            if (i != idx && set->jobs[i].status != JOB_COMPLETED && !in_queue[i] &&
                set->jobs[i].arrival <= current_time) {
                queue[q_tail++] = i;
                in_queue[i] = 1;
                set->jobs[i].status = JOB_READY;
            }
        }

        if (j->remaining == 0) {
            j->completion_time = current_time;
            j->status = JOB_COMPLETED;
            completed++;
            cc_log("SCHEDULER", "Job %s completed in RR at T=%d", j->id, current_time);
        } else {
            j->status = JOB_READY;
            queue[q_tail++] = idx;
            in_queue[idx] = 1;
        }
    }

    calculate_metrics(set, timeline, metrics);
    return 0;
}

int schedule_priority(job_set_t *set, timeline_t *timeline, schedule_metrics_t *metrics)
{
    int current_time = 0;
    int completed = 0;
    timeline->count = 0;
    reset_jobs(set);

    cc_log("SCHEDULER", "Starting Non-Preemptive Priority Scheduling for %d jobs", set->count);

    while (completed < set->count) {
        int best = -1;
        for (int i = 0; i < set->count; i++) {
            if (set->jobs[i].status == JOB_COMPLETED)
                continue;
            if (set->jobs[i].arrival <= current_time) {
                if (best == -1) {
                    best = i;
                } else if (set->jobs[i].priority < set->jobs[best].priority) {
                    best = i;
                } else if (set->jobs[i].priority == set->jobs[best].priority) {
                    /* Tie-break: earliest arrival, then file input order */
                    if (set->jobs[i].arrival < set->jobs[best].arrival)
                        best = i;
                    else if (set->jobs[i].arrival == set->jobs[best].arrival &&
                             set->jobs[i].input_order < set->jobs[best].input_order)
                        best = i;
                }
            }
        }

        if (best == -1) {
            int next_arrival = 1000000;
            for (int i = 0; i < set->count; i++) {
                if (set->jobs[i].status != JOB_COMPLETED && set->jobs[i].arrival < next_arrival)
                    next_arrival = set->jobs[i].arrival;
            }
            add_timeline_slot(timeline, "IDLE", current_time, next_arrival);
            current_time = next_arrival;
            continue;
        }

        job_t *j = &set->jobs[best];
        j->status = JOB_RUNNING;
        j->start_time = current_time;
        j->started = 1;
        current_time += j->burst;
        j->remaining = 0;
        j->completion_time = current_time;
        j->status = JOB_COMPLETED;
        completed++;

        add_timeline_slot(timeline, j->id, j->start_time, j->completion_time);
        cc_log("SCHEDULER", "Job %s (Priority %d) executed [T=%d -> T=%d]",
               j->id, j->priority, j->start_time, j->completion_time);
    }

    calculate_metrics(set, timeline, metrics);
    return 0;
}

void print_schedule_results(const char *algo_name, const job_set_t *set,
                            const timeline_t *timeline, const schedule_metrics_t *metrics)
{
    print_section("Execution Order & Gantt Timeline");
    printf("Timeline slots:\n");
    for (int i = 0; i < timeline->count; i++) {
        const timeline_slot_t *s = &timeline->slots[i];
        printf("[%3d - %3d] : %s\n", s->start_time, s->end_time, s->job_id);
    }

    printf("\nASCII Gantt Chart:\n|");
    for (int i = 0; i < timeline->count; i++) {
        printf(" %-4s |", timeline->slots[i].job_id);
    }
    printf("\n");
    for (int i = 0; i < timeline->count; i++) {
        if (i == 0)
            printf("%-6d", timeline->slots[i].start_time);
        printf("%-7d", timeline->slots[i].end_time);
    }
    printf("\n");

    print_section("Per-Job Metrics Table");
    printf("%-8s %8s %8s %9s %12s %15s %13s %14s\n",
           "JobID", "Arrival", "Burst", "Priority", "Completion", "Turnaround(TAT)", "Waiting(WT)", "Response(RT)");
    printf("%-8s %8s %8s %9s %12s %15s %13s %14s\n",
           "-----", "-------", "-----", "--------", "----------", "---------------", "-----------", "------------");
    for (int i = 0; i < set->count; i++) {
        const job_t *j = &set->jobs[i];
        printf("%-8s %8d %8d %9d %12d %15d %13d %14d\n",
               j->id, j->arrival, j->burst, j->priority,
               j->completion_time, j->turnaround_time, j->waiting_time, j->response_time);
    }

    print_section("Summary Statistics");
    printf("Policy                    : %s\n", algo_name);
    printf("Total Elapsed Time        : %d time units\n", metrics->total_time);
    printf("CPU Busy Time             : %d time units\n", metrics->cpu_busy_time);
    printf("CPU Utilization           : %.2f %%\n", metrics->cpu_utilization);
    printf("Average Turnaround Time   : %.2f time units\n", metrics->avg_turnaround);
    printf("Average Waiting Time      : %.2f time units\n", metrics->avg_waiting);
    printf("Average Response Time     : %.2f time units\n", metrics->avg_response);
}

void print_comparison_table(const schedule_metrics_t *fcfs,
                            const schedule_metrics_t *rr,
                            const schedule_metrics_t *prio,
                            int quantum)
{
    print_section("Policy Comparison Summary (Same Job Set)");
    printf("%-30s %12s %12s %14s\n", "Policy", "Avg TAT", "Avg WT", "Avg RT");
    printf("%-30s %12s %12s %14s\n", "------------------------------", "-------", "------", "------");
    printf("%-30s %12.2f %12.2f %14.2f\n", "FCFS (First-Come First-Served)",
           fcfs->avg_turnaround, fcfs->avg_waiting, fcfs->avg_response);

    char rr_label[40];
    snprintf(rr_label, sizeof rr_label, "Round Robin (Quantum = %d)", quantum);
    printf("%-30s %12.2f %12.2f %14.2f\n", rr_label,
           rr->avg_turnaround, rr->avg_waiting, rr->avg_response);

    printf("%-30s %12.2f %12.2f %14.2f\n", "Priority (Non-Preemptive)",
           prio->avg_turnaround, prio->avg_waiting, prio->avg_response);

    printf("\nObservations:\n");
    if (rr->avg_response <= fcfs->avg_response) {
        printf(" - Round Robin provides superior interactive responsiveness (lower average response time).\n");
    }
    printf(" - Priority scheduling prioritizes critical workloads based on rank (1 = highest priority).\n");
    printf(" - FCFS execution is simple and avoids starvation, but can suffer from convoy effects.\n");
}

int cmd_fcfs(int argc, char **argv)
{
    const char *path = (argc >= 1) ? argv[0] : CC_DEFAULT_JOBS;
    job_set_t set;
    timeline_t timeline;
    schedule_metrics_t metrics;

    if (argc > 1) {
        fprintf(stderr, "Usage: %s fcfs [JOB_FILE]\n", CC_PROGRAM_NAME);
        return CC_ERR_USAGE;
    }

    print_banner("FR2: CPU Scheduling - First-Come, First-Served (FCFS)");
    printf("Job file : %s\n", path);
    printf("Tie-break: Arrival time first, then input order in job file\n");

    if (load_jobs(path, &set) != 0)
        return CC_ERR_RUNTIME;

    schedule_fcfs(&set, &timeline, &metrics);
    print_schedule_results("First-Come, First-Served (FCFS)", &set, &timeline, &metrics);
    return CC_OK;
}

int cmd_rr(int argc, char **argv)
{
    const char *path = (argc >= 1) ? argv[0] : CC_DEFAULT_JOBS;
    long quantum = CC_DEFAULT_QUANTUM;
    job_set_t set;
    timeline_t timeline;
    schedule_metrics_t metrics;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s rr [JOB_FILE] [TIME_QUANTUM]\n", CC_PROGRAM_NAME);
        return CC_ERR_USAGE;
    }
    if (argc >= 2 && parse_long_arg(argv[1], "TIME_QUANTUM", 1, 100, &quantum) != 0)
        return CC_ERR_USAGE;

    print_banner("FR2: CPU Scheduling - Round Robin (RR)");
    printf("Job file    : %s\n", path);
    printf("Time Quantum: %ld time units\n", quantum);
    printf("Preemption  : Time-sliced round-robin ready queue\n");

    if (load_jobs(path, &set) != 0)
        return CC_ERR_RUNTIME;

    schedule_rr(&set, (int)quantum, &timeline, &metrics);
    print_schedule_results("Round Robin (RR)", &set, &timeline, &metrics);
    return CC_OK;
}

int cmd_priority(int argc, char **argv)
{
    const char *path = (argc >= 1) ? argv[0] : CC_DEFAULT_JOBS;
    job_set_t set;
    timeline_t timeline;
    schedule_metrics_t metrics;

    if (argc > 1) {
        fprintf(stderr, "Usage: %s priority [JOB_FILE]\n", CC_PROGRAM_NAME);
        return CC_ERR_USAGE;
    }

    print_banner("FR2: CPU Scheduling - Non-preemptive Priority Scheduling");
    printf("Job file : %s\n", path);
    printf("Rule     : Lower integer = higher priority (1 = highest)\n");
    printf("Tie-break: If priorities match, earlier arrival, then input order\n");

    if (load_jobs(path, &set) != 0)
        return CC_ERR_RUNTIME;

    schedule_priority(&set, &timeline, &metrics);
    print_schedule_results("Non-Preemptive Priority", &set, &timeline, &metrics);
    return CC_OK;
}

int cmd_compare(int argc, char **argv)
{
    const char *path = (argc >= 1) ? argv[0] : CC_DEFAULT_JOBS;
    long quantum = CC_DEFAULT_QUANTUM;
    job_set_t set;
    timeline_t timeline;
    schedule_metrics_t m_fcfs, m_rr, m_prio;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s compare [JOB_FILE] [QUANTUM]\n", CC_PROGRAM_NAME);
        return CC_ERR_USAGE;
    }
    if (argc >= 2 && parse_long_arg(argv[1], "QUANTUM", 1, 100, &quantum) != 0)
        return CC_ERR_USAGE;

    print_banner("FR2: CPU Scheduling Comparison (FCFS vs Round Robin vs Priority)");
    printf("Job file    : %s\n", path);
    printf("RR Quantum  : %ld time units\n", quantum);

    if (load_jobs(path, &set) != 0)
        return CC_ERR_RUNTIME;

    schedule_fcfs(&set, &timeline, &m_fcfs);
    schedule_rr(&set, (int)quantum, &timeline, &m_rr);
    schedule_priority(&set, &timeline, &m_prio);

    print_comparison_table(&m_fcfs, &m_rr, &m_prio, (int)quantum);
    return CC_OK;
}
