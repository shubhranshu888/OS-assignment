/*
 * ipc.h - FR4: POSIX shared memory and named semaphore IPC between cooperating processes.
 */
#ifndef CAMPUSCOMPUTE_IPC_H
#define CAMPUSCOMPUTE_IPC_H

#define SHM_NAME_PREFIX   "/campuscompute_shm"
#define SEM_REQ_PREFIX    "/campuscompute_sreq"
#define SEM_RESP_PREFIX   "/campuscompute_sresp"

#define IPC_PAYLOAD_TEXT  128

typedef struct {
    int  iteration;
    int  worker_pid;
    int  task_id;
    int  input_value;
    int  computed_result;
    char status_message[IPC_PAYLOAD_TEXT];
} cc_ipc_payload_t;

/* Run the shared memory & semaphore demonstration */
int run_ipc_demo(int iterations);

/* Explicit cleanup function for program-created IPC resources */
void cleanup_ipc_resources(void);

/* CLI command */
int cmd_ipc(int argc, char **argv);

#endif /* CAMPUSCOMPUTE_IPC_H */
