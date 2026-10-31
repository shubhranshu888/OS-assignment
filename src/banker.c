/*
 * banker.c - Implementation of Banker's Safety Algorithm (FR5).
 */
#include "banker.h"
#include "common.h"
#include "logger.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int load_banker_file(const char *path, banker_system_t *sys)
{
    FILE *fp = fopen(path, "r");
    char line[512];
    int line_no = 0;

    memset(sys, 0, sizeof *sys);
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open banker file '%s': %s\n", path, strerror(errno));
        return -1;
    }

    int stage = 0; /* 0: dims, 1: res names, 2: alloc, 3: max, 4: avail */
    int proc_idx = 0;

    while (fgets(line, sizeof line, fp) != NULL) {
        line_no++;
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char *tokens[32];
        int count = 0;
        char *save = NULL;
        for (char *t = strtok_r(line, " \t\r\n", &save); t; t = strtok_r(NULL, " \t\r\n", &save)) {
            tokens[count++] = t;
        }
        if (count == 0) continue;

        if (stage == 0) {
            if (count < 2) {
                fprintf(stderr, "%s:%d: expected '<num_processes> <num_resources>'\n", path, line_no);
                fclose(fp);
                return -1;
            }
            sys->num_processes = atoi(tokens[0]);
            sys->num_resources = atoi(tokens[1]);
            if (sys->num_processes <= 0 || sys->num_processes > BANKER_MAX_PROCESSES ||
                sys->num_resources <= 0 || sys->num_resources > BANKER_MAX_RESOURCES) {
                fprintf(stderr, "%s:%d: invalid dimensions (%d procs, %d resources)\n",
                        path, line_no, sys->num_processes, sys->num_resources);
                fclose(fp);
                return -1;
            }
            for (int i = 0; i < sys->num_processes; i++)
                snprintf(sys->process_names[i], sizeof sys->process_names[i], "P%d", i);
            stage = 1;
        } else if (stage == 1) {
            /* Optional resource labels or first line of allocation */
            if (count == sys->num_resources && !isdigit((unsigned char)tokens[0][0])) {
                for (int i = 0; i < count; i++)
                    snprintf(sys->resource_names[i], sizeof sys->resource_names[i], "%s", tokens[i]);
                stage = 2;
                proc_idx = 0;
            } else {
                for (int i = 0; i < sys->num_resources; i++)
                    snprintf(sys->resource_names[i], sizeof sys->resource_names[i], "R%d", i);
                stage = 2;
                proc_idx = 0;
                /* Fallthrough to parse allocation line */
                goto parse_alloc;
            }
        } else if (stage == 2) {
        parse_alloc:
            if (count < sys->num_resources) {
                fprintf(stderr, "%s:%d: expected %d allocation values, got %d\n",
                        path, line_no, sys->num_resources, count);
                fclose(fp);
                return -1;
            }
            for (int j = 0; j < sys->num_resources; j++)
                sys->allocation[proc_idx][j] = atoi(tokens[j]);
            proc_idx++;
            if (proc_idx == sys->num_processes) {
                stage = 3;
                proc_idx = 0;
            }
        } else if (stage == 3) {
            if (count < sys->num_resources) {
                fprintf(stderr, "%s:%d: expected %d max values, got %d\n",
                        path, line_no, sys->num_resources, count);
                fclose(fp);
                return -1;
            }
            for (int j = 0; j < sys->num_resources; j++)
                sys->max[proc_idx][j] = atoi(tokens[j]);
            proc_idx++;
            if (proc_idx == sys->num_processes) {
                stage = 4;
            }
        } else if (stage == 4) {
            if (count < sys->num_resources) {
                fprintf(stderr, "%s:%d: expected %d available values, got %d\n",
                        path, line_no, sys->num_resources, count);
                fclose(fp);
                return -1;
            }
            for (int j = 0; j < sys->num_resources; j++)
                sys->available[j] = atoi(tokens[j]);
            stage = 5;
            break;
        }
    }
    fclose(fp);

    if (stage < 4) {
        fprintf(stderr, "Error: banker file '%s' incomplete (stopped at stage %d)\n", path, stage);
        return -1;
    }

    /* Compute Need Matrix: Need[i][j] = Max[i][j] - Allocation[i][j] */
    for (int i = 0; i < sys->num_processes; i++) {
        for (int j = 0; j < sys->num_resources; j++) {
            if (sys->allocation[i][j] > sys->max[i][j]) {
                fprintf(stderr, "Error: Process %s Allocation[%s] (%d) exceeds Max (%d)\n",
                        sys->process_names[i], sys->resource_names[j],
                        sys->allocation[i][j], sys->max[i][j]);
                return -1;
            }
            sys->need[i][j] = sys->max[i][j] - sys->allocation[i][j];
        }
    }

    return 0;
}

int check_banker_safety(const banker_system_t *sys, banker_result_t *res)
{
    int work[BANKER_MAX_RESOURCES];
    int finish[BANKER_MAX_PROCESSES];
    memset(res, 0, sizeof *res);
    memset(finish, 0, sizeof finish);

    for (int j = 0; j < sys->num_resources; j++) {
        work[j] = sys->available[j];
        res->work_matrix[0][j] = work[j];
    }

    int count = 0;
    while (count < sys->num_processes) {
        int found = 0;
        for (int i = 0; i < sys->num_processes; i++) {
            if (!finish[i]) {
                int possible = 1;
                for (int j = 0; j < sys->num_resources; j++) {
                    if (sys->need[i][j] > work[j]) {
                        possible = 0;
                        break;
                    }
                }
                if (possible) {
                    /* Process can finish */
                    for (int j = 0; j < sys->num_resources; j++) {
                        work[j] += sys->allocation[i][j];
                        res->work_matrix[count + 1][j] = work[j];
                    }
                    finish[i] = 1;
                    res->safe_sequence[count] = i;
                    count++;
                    found = 1;
                    break;
                }
            }
        }
        if (!found) {
            break; /* Deadlock hazard / Unsafe state */
        }
    }

    res->sequence_length = count;
    res->is_safe = (count == sys->num_processes);

    if (res->is_safe) {
        cc_log("BANKER", "System is in a SAFE state; safe sequence found");
    } else {
        cc_log("BANKER", "System is in an UNSAFE state (%d/%d processes completed)",
               count, sys->num_processes);
    }

    return res->is_safe ? 0 : -1;
}

void print_banker_system(const banker_system_t *sys)
{
    print_section("Banker's System Resource Matrices");

    printf("Processes: %d, Resources: %d\n", sys->num_processes, sys->num_resources);
    printf("Resource Types: ");
    for (int j = 0; j < sys->num_resources; j++)
        printf("%s ", sys->resource_names[j]);
    printf("\n\n");

    /* Header */
    printf("%-8s | ", "Process");
    for (int j = 0; j < sys->num_resources; j++) printf("%-3s ", sys->resource_names[j]);
    printf("| ");
    for (int j = 0; j < sys->num_resources; j++) printf("%-3s ", sys->resource_names[j]);
    printf("| ");
    for (int j = 0; j < sys->num_resources; j++) printf("%-3s ", sys->resource_names[j]);
    printf("| ");
    for (int j = 0; j < sys->num_resources; j++) printf("%-3s ", sys->resource_names[j]);
    printf("\n");

    printf("%-8s | %-11s | %-11s | %-11s | %-11s\n",
           "ID", "Allocation", "Maximum", "Need (M-A)", "Available");
    printf("--------------------------------------------------------------------\n");

    for (int i = 0; i < sys->num_processes; i++) {
        printf("%-8s | ", sys->process_names[i]);

        /* Allocation */
        for (int j = 0; j < sys->num_resources; j++)
            printf("%-3d ", sys->allocation[i][j]);
        printf("| ");

        /* Max */
        for (int j = 0; j < sys->num_resources; j++)
            printf("%-3d ", sys->max[i][j]);
        printf("| ");

        /* Need */
        for (int j = 0; j < sys->num_resources; j++)
            printf("%-3d ", sys->need[i][j]);
        printf("| ");

        /* Available (printed on row 0) */
        if (i == 0) {
            for (int j = 0; j < sys->num_resources; j++)
                printf("%-3d ", sys->available[j]);
        }
        printf("\n");
    }
}

void print_banker_result(const banker_system_t *sys, const banker_result_t *res)
{
    print_section("Banker's Safety Analysis Result");

    if (res->is_safe) {
        printf("STATUS: [SAFE STATE]\n");
        printf("A safe execution sequence exists where every process completes without deadlock.\n\n");
        printf("Safe Sequence:\n  < ");
        for (int i = 0; i < res->sequence_length; i++) {
            int p = res->safe_sequence[i];
            printf("%s%s", sys->process_names[p], (i == res->sequence_length - 1) ? "" : " -> ");
        }
        printf(" >\n\n");

        printf("Step-by-step Work Vector Evolution:\n");
        printf("  Initial Available Work: [ ");
        for (int j = 0; j < sys->num_resources; j++) printf("%d ", res->work_matrix[0][j]);
        printf("]\n");

        for (int i = 0; i < res->sequence_length; i++) {
            int p = res->safe_sequence[i];
            printf("  After %-4s completes : [ ", sys->process_names[p]);
            for (int j = 0; j < sys->num_resources; j++)
                printf("%d ", res->work_matrix[i + 1][j]);
            printf("]\n");
        }
    } else {
        printf("STATUS: [UNSAFE STATE]\n");
        printf("WARNING: No complete safe sequence could be found!\n");
        printf("Completed processes before deadlock stall: %d / %d\n",
               res->sequence_length, sys->num_processes);
        if (res->sequence_length > 0) {
            printf("Partial execution sequence: < ");
            for (int i = 0; i < res->sequence_length; i++) {
                int p = res->safe_sequence[i];
                printf("%s%s", sys->process_names[p], (i == res->sequence_length - 1) ? "" : " -> ");
            }
            printf(" > [BLOCKED]\n");
        }
        printf("System cannot satisfy resource demands of remaining processes with current Available vector.\n");
    }
}

int cmd_banker(int argc, char **argv)
{
    const char *path = (argc >= 1) ? argv[0] : CC_SAFE_BANKER;
    banker_system_t sys;
    banker_result_t res;

    if (argc > 1) {
        fprintf(stderr, "Usage: %s banker [BANKER_FILE]\n", CC_PROGRAM_NAME);
        return CC_ERR_USAGE;
    }

    print_banner("FR5: Banker's Algorithm - Resource Safety Analysis (Safe State)");
    printf("Resource file: %s\n", path);

    if (load_banker_file(path, &sys) != 0)
        return CC_ERR_RUNTIME;

    print_banker_system(&sys);
    check_banker_safety(&sys, &res);
    print_banker_result(&sys, &res);

    return res.is_safe ? CC_OK : CC_ERR_RUNTIME;
}

int cmd_banker_unsafe(int argc, char **argv)
{
    const char *path = (argc >= 1) ? argv[0] : CC_UNSAFE_BANKER;
    banker_system_t sys;
    banker_result_t res;

    if (argc > 1) {
        fprintf(stderr, "Usage: %s banker-unsafe [BANKER_FILE]\n", CC_PROGRAM_NAME);
        return CC_ERR_USAGE;
    }

    print_banner("FR5: Banker's Algorithm - Resource Safety Analysis (Unsafe State Demonstration)");
    printf("Resource file: %s\n", path);

    if (load_banker_file(path, &sys) != 0)
        return CC_ERR_RUNTIME;

    print_banker_system(&sys);
    check_banker_safety(&sys, &res);
    print_banker_result(&sys, &res);

    return CC_OK;
}
