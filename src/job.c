/*
 * job.c - FR1: job loading, validation and lifecycle helpers.
 *
 * Job file format (one job per line, '#' starts a comment):
 *     <id> <arrival_time> <cpu_burst> <priority>
 * Example:
 *     J1 0 6 3
 */
#include "job.h"
#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *job_status_name(job_status_t status)
{
    switch (status) {
    case JOB_NEW:       return "NEW";
    case JOB_READY:     return "READY";
    case JOB_RUNNING:   return "RUNNING";
    case JOB_COMPLETED: return "COMPLETED";
    }
    return "UNKNOWN";
}

/* A job ID may contain letters, digits, '_' and '-' only. */
static int valid_job_id(const char *id)
{
    size_t len = strlen(id);

    if (len == 0 || len >= JOB_ID_LEN)
        return 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)id[i];
        if (!isalnum(c) && c != '_' && c != '-')
            return 0;
    }
    return 1;
}

/* Parse one integer field of a job line; prints an error that includes the
 * file name and line number when the value is invalid. */
static int parse_job_field(const char *path, int line_no, const char *field_name,
                           const char *text, long min, long max, int *out)
{
    long value;
    int rc = parse_long(text, min, max, &value);

    if (rc == -1) {
        fprintf(stderr, "%s:%d: %s '%s' is not an integer\n",
                path, line_no, field_name, text);
        return -1;
    }
    if (rc == -2) {
        fprintf(stderr, "%s:%d: %s %s out of range (allowed %ld..%ld)\n",
                path, line_no, field_name, text, min, max);
        return -1;
    }
    *out = (int)value;
    return 0;
}

void reset_jobs(job_set_t *set)
{
    for (int i = 0; i < set->count; i++) {
        job_t *j = &set->jobs[i];
        j->status = JOB_NEW;
        j->remaining = j->burst;
        j->started = 0;
        j->start_time = -1;
        j->completion_time = -1;
        j->turnaround_time = 0;
        j->waiting_time = 0;
        j->response_time = 0;
    }
}

int load_jobs(const char *path, job_set_t *set)
{
    FILE *fp = fopen(path, "r");
    char line[512];
    int line_no = 0;
    int errors = 0;

    set->count = 0;
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open job file '%s': %s\n", path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof line, fp) != NULL) {
        char *fields[6];
        int nfields = 0;
        char *save = NULL;
        char *hash;
        job_t job;

        line_no++;

        if (strchr(line, '\n') == NULL && !feof(fp)) {
            int ch;
            fprintf(stderr, "%s:%d: line too long\n", path, line_no);
            while ((ch = fgetc(fp)) != '\n' && ch != EOF)
                ;   /* discard the rest of the over-long line */
            errors++;
            continue;
        }

        hash = strchr(line, '#');
        if (hash != NULL)
            *hash = '\0';                 /* strip comments */

        for (char *tok = strtok_r(line, " \t\r\n", &save);
             tok != NULL && nfields < 6;
             tok = strtok_r(NULL, " \t\r\n", &save))
            fields[nfields++] = tok;

        if (nfields == 0)
            continue;                     /* blank / comment-only line */

        if (nfields != 4) {
            fprintf(stderr, "%s:%d: expected 4 fields '<id> <arrival> <burst> <priority>', found %s%d\n",
                    path, line_no, nfields >= 6 ? "at least " : "", nfields);
            errors++;
            continue;
        }

        memset(&job, 0, sizeof job);
        if (!valid_job_id(fields[0])) {
            fprintf(stderr, "%s:%d: invalid job id '%s' (1-%d chars: letters, digits, '_' or '-')\n",
                    path, line_no, fields[0], JOB_ID_LEN - 1);
            errors++;
            continue;
        }
        snprintf(job.id, sizeof job.id, "%s", fields[0]);

        if (parse_job_field(path, line_no, "arrival time", fields[1], 0, JOB_MAX_ARRIVAL, &job.arrival) ||
            parse_job_field(path, line_no, "CPU burst", fields[2], 1, JOB_MAX_BURST, &job.burst) ||
            parse_job_field(path, line_no, "priority", fields[3], 1, JOB_MAX_PRIORITY, &job.priority)) {
            errors++;
            continue;
        }

        {
            int duplicate = 0;
            for (int i = 0; i < set->count; i++) {
                if (strcmp(set->jobs[i].id, job.id) == 0) {
                    fprintf(stderr, "%s:%d: duplicate job id '%s'\n", path, line_no, job.id);
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate) {
                errors++;
                continue;
            }
        }

        if (set->count >= MAX_JOBS) {
            fprintf(stderr, "%s:%d: too many jobs (maximum is %d)\n", path, line_no, MAX_JOBS);
            errors++;
            break;
        }

        job.input_order = set->count;
        set->jobs[set->count++] = job;
    }
    fclose(fp);

    if (errors > 0) {
        fprintf(stderr, "Error: %d invalid line(s) in '%s'; no jobs were loaded.\n", errors, path);
        set->count = 0;
        return -2;
    }
    if (set->count == 0) {
        fprintf(stderr, "Error: '%s' does not contain any jobs.\n", path);
        return -2;
    }

    reset_jobs(set);
    return 0;
}

void print_job_table(const job_set_t *set)
{
    printf("%-8s %8s %8s %9s   %-10s\n", "JobID", "Arrival", "Burst", "Priority", "Status");
    printf("%-8s %8s %8s %9s   %-10s\n", "-----", "-------", "-----", "--------", "------");
    for (int i = 0; i < set->count; i++) {
        const job_t *j = &set->jobs[i];
        printf("%-8s %8d %8d %9d   %-10s\n",
               j->id, j->arrival, j->burst, j->priority, job_status_name(j->status));
    }
    printf("(%d job(s); priority 1 = highest)\n", set->count);
}

int cmd_jobs(int argc, char **argv)
{
    const char *path = (argc >= 1) ? argv[0] : CC_DEFAULT_JOBS;
    job_set_t set;

    if (argc > 1) {
        fprintf(stderr, "Usage: %s jobs [JOB_FILE]\n", CC_PROGRAM_NAME);
        return CC_ERR_USAGE;
    }

    print_banner("FR1: Job management - load and validate jobs");
    printf("Job file: %s\n\n", path);
    if (load_jobs(path, &set) != 0)
        return CC_ERR_RUNTIME;

    print_job_table(&set);
    printf("\nAll %d job(s) passed validation (unique IDs, arrival 0..%d, burst 1..%d, priority 1..%d).\n",
           set.count, JOB_MAX_ARRIVAL, JOB_MAX_BURST, JOB_MAX_PRIORITY);
    return CC_OK;
}

