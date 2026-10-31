/*
 * worker.c - Implementation of worker process creation and failure detection (FR6).
 */
#include "worker.h"
#include "common.h"
#include "logger.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int run_worker_demo(worker_fail_type_t type, int custom_exit_code)
{
    print_section("Worker Process Lifecycle");

    flush_all_output();
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Error: fork failed: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        /* Child Worker Process */
        pid_t child_pid = getpid();
        printf("  [WORKER %d] Child process started.\n", (int)child_pid);

        if (type == WORKER_NORMAL) {
            printf("  [WORKER %d] Executing simulated university compute job (e.g., student simulation)...\n", (int)child_pid);
            sleep_ms(60);
            printf("  [WORKER %d] Simulation finished successfully. Exiting with status 0.\n", (int)child_pid);
            child_exit(0);
        } else if (type == WORKER_SIMULATED_FAIL_EXIT) {
            printf("  [WORKER %d] SIMULATED FAILURE: Encountered out-of-bounds simulation parameter.\n", (int)child_pid);
            printf("  [WORKER %d] Terminating with controlled non-zero exit code %d.\n", (int)child_pid, custom_exit_code);
            child_exit(custom_exit_code);
        } else if (type == WORKER_SIMULATED_FAIL_SIGNAL) {
            printf("  [WORKER %d] SIMULATED FAILURE: Raising SIGABRT to simulate abnormal fault.\n", (int)child_pid);
            raise(SIGABRT);
            child_exit(1);
        }
    }

    /* Parent Process */
    printf("[PARENT %d] Created worker child with PID %d; waiting with waitpid()...\n",
           (int)getpid(), (int)pid);

    int status = 0;
    pid_t waited_pid = waitpid(pid, &status, 0);

    if (waited_pid == -1) {
        fprintf(stderr, "Error: waitpid failed: %s\n", strerror(errno));
        return -1;
    }

    char desc[128];
    describe_wait_status(status, desc, sizeof desc);

    print_section("Parent-Side Detection Report");
    printf("Collected Child PID       : %d\n", (int)waited_pid);
    printf("Raw Status Value          : 0x%04x\n", (unsigned)status);
    printf("Status Analysis           : %s\n", desc);

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == 0) {
            printf("Diagnosis                 : [NORMAL TERMINATION] Worker completed successfully.\n");
            cc_log("WORKER", "Worker child %d terminated normally with status 0", (int)waited_pid);
        } else {
            printf("Diagnosis                 : [SIMULATED CONTROLLED FAILURE] Non-zero exit status %d caught.\n", code);
            cc_log("WORKER", "Worker child %d reported controlled failure exit code %d", (int)waited_pid, code);
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("Diagnosis                 : [SIMULATED ABNORMAL TERMINATION] Child killed by signal %d (%s).\n",
               sig, strsignal(sig));
        cc_log("WORKER", "Worker child %d killed by signal %d (%s)", (int)waited_pid, sig, strsignal(sig));
    }

    return 0;
}

int cmd_worker_normal(int argc, char **argv)
{
    (void)argc; (void)argv;
    print_banner("FR6: Worker Process - Normal Execution & Lifecycle Monitoring");
    return run_worker_demo(WORKER_NORMAL, 0) == 0 ? CC_OK : CC_ERR_RUNTIME;
}

int cmd_worker_fail(int argc, char **argv)
{
    worker_fail_type_t type = WORKER_SIMULATED_FAIL_EXIT;
    long code = 42;

    if (argc >= 1) {
        if (strcmp(argv[0], "signal") == 0 || strcmp(argv[0], "sigabrt") == 0) {
            type = WORKER_SIMULATED_FAIL_SIGNAL;
        } else if (parse_long_arg(argv[0], "EXIT_CODE", 1, 255, &code) == 0) {
            type = WORKER_SIMULATED_FAIL_EXIT;
        } else {
            return CC_ERR_USAGE;
        }
    }

    print_banner("FR6: Worker Process - Simulated Controlled Failure Detection");
    printf("Mode: Controlled simulated failure (%s)\n",
           type == WORKER_SIMULATED_FAIL_SIGNAL ? "Signal SIGABRT" : "Non-zero exit code");
    printf("Note: This is a controlled pedagogical simulation, not an unhandled OS fault.\n");

    return run_worker_demo(type, (int)code) == 0 ? CC_OK : CC_ERR_RUNTIME;
}
