/*
 * banker.h - FR5: Banker's Safety Algorithm for Deadlock Avoidance.
 */
#ifndef CAMPUSCOMPUTE_BANKER_H
#define CAMPUSCOMPUTE_BANKER_H

#define BANKER_MAX_PROCESSES 32
#define BANKER_MAX_RESOURCES 16

typedef struct {
    int num_processes;
    int num_resources;
    char process_names[BANKER_MAX_PROCESSES][16];
    char resource_names[BANKER_MAX_RESOURCES][16];

    int available[BANKER_MAX_RESOURCES];
    int max[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES];
    int allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES];
    int need[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES];
} banker_system_t;

typedef struct {
    int is_safe;
    int safe_sequence[BANKER_MAX_PROCESSES];
    int sequence_length;
    int work_matrix[BANKER_MAX_PROCESSES + 1][BANKER_MAX_RESOURCES];
} banker_result_t;

/* Load Banker's configuration from file */
int load_banker_file(const char *path, banker_system_t *sys);

/* Run Banker's Safety Check */
int check_banker_safety(const banker_system_t *sys, banker_result_t *res);

/* Print matrices and evaluation result */
void print_banker_system(const banker_system_t *sys);
void print_banker_result(const banker_system_t *sys, const banker_result_t *res);

/* CLI commands */
int cmd_banker(int argc, char **argv);
int cmd_banker_unsafe(int argc, char **argv);

#endif /* CAMPUSCOMPUTE_BANKER_H */
