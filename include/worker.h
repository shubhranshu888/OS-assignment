/*
 * worker.h - FR6: Worker process creation and failure detection (fork + waitpid).
 */
#ifndef CAMPUSCOMPUTE_WORKER_H
#define CAMPUSCOMPUTE_WORKER_H

typedef enum {
    WORKER_NORMAL = 0,
    WORKER_SIMULATED_FAIL_EXIT,
    WORKER_SIMULATED_FAIL_SIGNAL
} worker_fail_type_t;

/* Run worker child demonstration */
int run_worker_demo(worker_fail_type_t type, int custom_exit_code);

/* CLI commands */
int cmd_worker_normal(int argc, char **argv);
int cmd_worker_fail(int argc, char **argv);

#endif /* CAMPUSCOMPUTE_WORKER_H */
