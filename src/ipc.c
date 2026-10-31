/*
 * ipc.c - Implementation of POSIX Shared Memory and Named Semaphores (FR4).
 */
#include "ipc.h"
#include "common.h"
#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char g_shm_name[64] = "";
static char g_sem_req_name[64] = "";
static char g_sem_resp_name[64] = "";

void cleanup_ipc_resources(void)
{
    if (g_shm_name[0] != '\0') {
        shm_unlink(g_shm_name);
        g_shm_name[0] = '\0';
    }
    if (g_sem_req_name[0] != '\0') {
        sem_unlink(g_sem_req_name);
        g_sem_req_name[0] = '\0';
    }
    if (g_sem_resp_name[0] != '\0') {
        sem_unlink(g_sem_resp_name);
        g_sem_resp_name[0] = '\0';
    }
}

int run_ipc_demo(int iterations)
{
    pid_t parent_pid = getpid();
    snprintf(g_shm_name, sizeof g_shm_name, "%s_%d", SHM_NAME_PREFIX, (int)parent_pid);
    snprintf(g_sem_req_name, sizeof g_sem_req_name, "%s_%d", SEM_REQ_PREFIX, (int)parent_pid);
    snprintf(g_sem_resp_name, sizeof g_sem_resp_name, "%s_%d", SEM_RESP_PREFIX, (int)parent_pid);

    /* Unlink any stale objects with same name */
    cleanup_ipc_resources();
    snprintf(g_shm_name, sizeof g_shm_name, "%s_%d", SHM_NAME_PREFIX, (int)parent_pid);
    snprintf(g_sem_req_name, sizeof g_sem_req_name, "%s_%d", SEM_REQ_PREFIX, (int)parent_pid);
    snprintf(g_sem_resp_name, sizeof g_sem_resp_name, "%s_%d", SEM_RESP_PREFIX, (int)parent_pid);

    print_section("IPC Resource Initialization");
    printf("[PARENT] Creating POSIX Shared Memory  : %s\n", g_shm_name);
    printf("[PARENT] Creating POSIX Named Semaphores: %s, %s\n", g_sem_req_name, g_sem_resp_name);

    int shm_fd = shm_open(g_shm_name, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (shm_fd == -1) {
        fprintf(stderr, "Error: shm_open failed: %s\n", strerror(errno));
        cleanup_ipc_resources();
        return -1;
    }

    if (ftruncate(shm_fd, sizeof(cc_ipc_payload_t)) == -1) {
        fprintf(stderr, "Error: ftruncate failed: %s\n", strerror(errno));
        close(shm_fd);
        cleanup_ipc_resources();
        return -1;
    }

    cc_ipc_payload_t *shared = mmap(NULL, sizeof(cc_ipc_payload_t),
                                     PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) {
        fprintf(stderr, "Error: mmap failed: %s\n", strerror(errno));
        close(shm_fd);
        cleanup_ipc_resources();
        return -1;
    }

    sem_t *sem_req = sem_open(g_sem_req_name, O_CREAT | O_EXCL, 0666, 0);
    sem_t *sem_resp = sem_open(g_sem_resp_name, O_CREAT | O_EXCL, 0666, 0);
    if (sem_req == SEM_FAILED || sem_resp == SEM_FAILED) {
        fprintf(stderr, "Error: sem_open failed: %s\n", strerror(errno));
        munmap(shared, sizeof(cc_ipc_payload_t));
        close(shm_fd);
        cleanup_ipc_resources();
        return -1;
    }

    printf("[PARENT] Shared memory & semaphores initialized successfully.\n");
    cc_log("IPC", "Initialized SHM (%s) and Semaphores (%s, %s)",
           g_shm_name, g_sem_req_name, g_sem_resp_name);

    flush_all_output();
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Error: fork failed: %s\n", strerror(errno));
        munmap(shared, sizeof(cc_ipc_payload_t));
        close(shm_fd);
        sem_close(sem_req);
        sem_close(sem_resp);
        cleanup_ipc_resources();
        return -1;
    }

    if (pid == 0) {
        /* Child Process (Cooperating Worker) */
        pid_t my_pid = getpid();
        for (int i = 1; i <= iterations; i++) {
            /* Wait for coordinator's request */
            if (sem_wait(sem_req) == -1) {
                if (errno == EINTR) { i--; continue; }
                break;
            }

            int val = shared->input_value;
            shared->worker_pid = (int)my_pid;
            shared->computed_result = val * val;  /* e.g., simulation matrix block trace */
            snprintf(shared->status_message, sizeof shared->status_message,
                     "Worker %d computed task %d (sq of %d = %d)",
                     (int)my_pid, shared->task_id, val, shared->computed_result);

            /* Signal coordinator that result is ready */
            sem_post(sem_resp);
        }

        munmap(shared, sizeof(cc_ipc_payload_t));
        close(shm_fd);
        sem_close(sem_req);
        sem_close(sem_resp);
        child_exit(0);
    }

    /* Parent Process (CampusCompute Coordinator) */
    print_section("Inter-Process Data Exchange");
    printf("%-6s %-12s %-14s %-16s %-30s\n",
           "Step", "Coordinator", "Input Sent", "Worker PID", "Worker Response (via SHM)");
    printf("%-6s %-12s %-14s %-16s %-30s\n",
           "----", "-----------", "----------", "----------", "------------------------------");

    for (int i = 1; i <= iterations; i++) {
        shared->iteration = i;
        shared->task_id = 100 + i;
        shared->input_value = i * 7;
        shared->worker_pid = 0;
        shared->computed_result = 0;
        snprintf(shared->status_message, sizeof shared->status_message, "Awaiting worker processing");

        /* Signal worker to proceed */
        sem_post(sem_req);

        /* Wait for worker to finish computation */
        sem_wait(sem_resp);

        printf("%-6d %-12d %-14d %-16d %-30s\n",
               i, (int)parent_pid, shared->input_value, shared->worker_pid, shared->status_message);
        cc_log("IPC", "Exchange #%d: input=%d -> result=%d from child %d",
               i, shared->input_value, shared->computed_result, shared->worker_pid);
    }

    /* Wait for child process termination */
    int status;
    waitpid(pid, &status, 0);

    /* Cleanup & unlinking verification */
    print_section("Resource Cleanup Verification");
    munmap(shared, sizeof(cc_ipc_payload_t));
    close(shm_fd);
    sem_close(sem_req);
    sem_close(sem_resp);

    int u1 = shm_unlink(g_shm_name);
    int u2 = sem_unlink(g_sem_req_name);
    int u3 = sem_unlink(g_sem_resp_name);

    printf("[CLEANUP] shm_unlink(\"%s\")   : %s\n", g_shm_name, u1 == 0 ? "SUCCESS (unlinked)" : strerror(errno));
    printf("[CLEANUP] sem_unlink(\"%s\") : %s\n", g_sem_req_name, u2 == 0 ? "SUCCESS (unlinked)" : strerror(errno));
    printf("[CLEANUP] sem_unlink(\"%s\"): %s\n", g_sem_resp_name, u3 == 0 ? "SUCCESS (unlinked)" : strerror(errno));

    /* Verify that objects are no longer present */
    int test_fd = shm_open(g_shm_name, O_RDWR, 0666);
    if (test_fd == -1 && errno == ENOENT) {
        printf("[VERIFY] Confirmed: Shared memory object successfully removed from system (ENOENT).\n");
    } else if (test_fd != -1) {
        close(test_fd);
    }

    g_shm_name[0] = '\0';
    g_sem_req_name[0] = '\0';
    g_sem_resp_name[0] = '\0';

    return 0;
}

int cmd_ipc(int argc, char **argv)
{
    long iterations = 4;
    if (argc > 1) {
        fprintf(stderr, "Usage: %s ipc [ITERATIONS 1-100]\n", CC_PROGRAM_NAME);
        return CC_ERR_USAGE;
    }
    if (argc == 1 && parse_long_arg(argv[0], "ITERATIONS", 1, 100, &iterations) != 0)
        return CC_ERR_USAGE;

    print_banner("FR4: Inter-Process Communication (POSIX Shared Memory & Named Semaphores)");
    printf("Iterations : %ld\n", iterations);
    printf("Mechanism  : POSIX shm_open(), mmap(), sem_open(), sem_wait(), sem_post()\n");

    if (run_ipc_demo((int)iterations) != 0)
        return CC_ERR_RUNTIME;

    return CC_OK;
}
