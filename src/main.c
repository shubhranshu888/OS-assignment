/*
 * main.c - CampusCompute: Command-Line Interface and Integrated Demonstration (FR8).
 */
#include "common.h"
#include "job.h"
#include "scheduler.h"
#include "logger.h"
#include "ipc.h"
#include "banker.h"
#include "worker.h"
#include "rlimit_demo.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_usage(void)
{
    printf("CampusCompute - Intelligent Job Scheduling and Resource Management (v%s)\n", CC_VERSION);
    printf("Operating Systems Level-3 Project Prototype\n\n");
    printf("Usage: %s <command> [arguments...]\n\n", CC_PROGRAM_NAME);
    printf("Available Commands:\n");
    printf("  help                         Display this help message\n");
    printf("  jobs        [FILE]           Load and validate job specification file (FR1)\n");
    printf("  fcfs        [FILE]           First-Come, First-Served CPU scheduling (FR2)\n");
    printf("  rr          [FILE] [QUANTUM] Round Robin CPU scheduling with time quantum (FR2)\n");
    printf("  priority    [FILE]           Non-preemptive Priority CPU scheduling (FR2)\n");
    printf("  compare     [FILE] [QUANTUM] Compare FCFS, RR, and Priority on same job set (FR2)\n");
    printf("  logger      [P] [E] [C]      Producer-consumer thread logger stress test (FR3)\n");
    printf("  ipc         [ITERATIONS]     POSIX Shared Memory & Named Semaphores demo (FR4)\n");
    printf("  banker      [FILE]           Banker's Safety Algorithm - Safe state test (FR5)\n");
    printf("  banker-unsafe [FILE]         Banker's Safety Algorithm - Unsafe state test (FR5)\n");
    printf("  worker-normal                Demonstrate normal worker child process (FR6)\n");
    printf("  worker-fail [code|signal]    Demonstrate simulated worker failure detection (FR6)\n");
    printf("  rlimit                       Demonstrate process resource limits via setrlimit (FR7)\n");
    printf("  cleanup                      Perform resource cleanup verification & audit (FR7)\n");
    printf("  demo                         Run complete integrated CampusCompute workflow (S21)\n");
    printf("\nInteractive Mode: Run without arguments to enter the interactive menu.\n");
}

static void sigint_handler(int sig)
{
    (void)sig;
    cleanup_ipc_resources();
    if (logger_is_running())
        logger_stop(NULL);
    _exit(1);
}

static int run_integrated_demo(void)
{
    print_banner("CampusCompute: Integrated System Demonstration");
    printf("Executing comprehensive automated test pipeline across all modules...\n");

    /* 1. Job Validation */
    printf("\n>>> [STEP 1/7] FR1: Validating Job Dataset <<<\n");
    char *argv_jobs[] = { CC_DEFAULT_JOBS };
    cmd_jobs(1, argv_jobs);

    /* 2. CPU Scheduling Comparison */
    printf("\n>>> [STEP 2/7] FR2: Evaluating CPU Scheduling Policies <<<\n");
    char *argv_cmp[] = { CC_DEFAULT_JOBS, "2" };
    cmd_compare(2, argv_cmp);

    /* 3. Concurrent Logger */
    printf("\n>>> [STEP 3/7] FR3: Concurrent Activity Logger <<<\n");
    char *argv_log[] = { "2", "3", "3" };
    cmd_logger(3, argv_log);

    /* 4. IPC */
    printf("\n>>> [STEP 4/7] FR4: Inter-Process Communication <<<\n");
    char *argv_ipc[] = { "2" };
    cmd_ipc(1, argv_ipc);

    /* 5. Banker's Algorithm */
    printf("\n>>> [STEP 5/7] FR5: Deadlock Safety Analysis <<<\n");
    cmd_banker(0, NULL);
    cmd_banker_unsafe(0, NULL);

    /* 6. Worker Lifecycle & Failure */
    printf("\n>>> [STEP 6/7] FR6: Worker Monitoring & Failure Detection <<<\n");
    cmd_worker_normal(0, NULL);
    char *argv_fail[] = { "42" };
    cmd_worker_fail(1, argv_fail);

    /* 7. Resource Limit & Cleanup */
    printf("\n>>> [STEP 7/7] FR7: Resource Limits & Final System Audit <<<\n");
    cmd_rlimit(0, NULL);
    cmd_cleanup(0, NULL);

    print_banner("Integrated Demonstration Complete: All Systems Verified");
    return CC_OK;
}

static void run_interactive_menu(void)
{
    char line[128];
    while (1) {
        printf("\n=======================================================\n");
        printf("         CampusCompute Interactive Menu\n");
        printf("=======================================================\n");
        printf(" 1. Job Management (Validate Jobs File)\n");
        printf(" 2. First-Come First-Served (FCFS) Scheduling\n");
        printf(" 3. Round Robin (RR) Scheduling\n");
        printf(" 4. Priority Scheduling\n");
        printf(" 5. Compare All Scheduling Algorithms\n");
        printf(" 6. Producer-Consumer Activity Logger\n");
        printf(" 7. Shared Memory & Semaphore IPC\n");
        printf(" 8. Banker's Algorithm (Safe State)\n");
        printf(" 9. Banker's Algorithm (Unsafe State)\n");
        printf("10. Worker Process (Normal Completion)\n");
        printf("11. Worker Process (Simulated Failure)\n");
        printf("12. Resource Limit Demonstration (setrlimit)\n");
        printf("13. System Resource Audit & Cleanup Verification\n");
        printf("14. Run Full Integrated Demonstration\n");
        printf(" 0. Exit\n");
        printf("-------------------------------------------------------\n");
        printf("Enter selection [0-14]: ");
        fflush(stdout);

        if (fgets(line, sizeof line, stdin) == NULL)
            break;

        int choice = -1;
        if (sscanf(line, "%d", &choice) != 1) {
            printf("Invalid input. Please enter a number between 0 and 14.\n");
            continue;
        }

        switch (choice) {
        case 0: printf("Exiting CampusCompute. Goodbye!\n"); return;
        case 1: cmd_jobs(0, NULL); break;
        case 2: cmd_fcfs(0, NULL); break;
        case 3: cmd_rr(0, NULL); break;
        case 4: cmd_priority(0, NULL); break;
        case 5: cmd_compare(0, NULL); break;
        case 6: cmd_logger(0, NULL); break;
        case 7: cmd_ipc(0, NULL); break;
        case 8: cmd_banker(0, NULL); break;
        case 9: cmd_banker_unsafe(0, NULL); break;
        case 10: cmd_worker_normal(0, NULL); break;
        case 11: cmd_worker_fail(0, NULL); break;
        case 12: cmd_rlimit(0, NULL); break;
        case 13: cmd_cleanup(0, NULL); break;
        case 14: run_integrated_demo(); break;
        default:
            printf("Unrecognized menu option '%d'. Allowed range: 0-14.\n", choice);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    atexit(cleanup_ipc_resources);

    /* Background activity logger for session auditing */
    logger_config_t log_cfg = { LOGGER_DEFAULT_CAPACITY, 0, 0 };
    logger_start(&log_cfg);

    int rc = CC_OK;

    if (argc < 2) {
        run_interactive_menu();
        if (logger_is_running())
            logger_stop(NULL);
        return CC_OK;
    }

    const char *cmd = argv[1];
    int sub_argc = argc - 2;
    char **sub_argv = &argv[2];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage();
    } else if (strcmp(cmd, "jobs") == 0) {
        rc = cmd_jobs(sub_argc, sub_argv);
    } else if (strcmp(cmd, "fcfs") == 0) {
        rc = cmd_fcfs(sub_argc, sub_argv);
    } else if (strcmp(cmd, "rr") == 0) {
        rc = cmd_rr(sub_argc, sub_argv);
    } else if (strcmp(cmd, "priority") == 0) {
        rc = cmd_priority(sub_argc, sub_argv);
    } else if (strcmp(cmd, "compare") == 0) {
        rc = cmd_compare(sub_argc, sub_argv);
    } else if (strcmp(cmd, "logger") == 0) {
        rc = cmd_logger(sub_argc, sub_argv);
    } else if (strcmp(cmd, "ipc") == 0) {
        rc = cmd_ipc(sub_argc, sub_argv);
    } else if (strcmp(cmd, "banker") == 0) {
        rc = cmd_banker(sub_argc, sub_argv);
    } else if (strcmp(cmd, "banker-unsafe") == 0) {
        rc = cmd_banker_unsafe(sub_argc, sub_argv);
    } else if (strcmp(cmd, "worker-normal") == 0) {
        rc = cmd_worker_normal(sub_argc, sub_argv);
    } else if (strcmp(cmd, "worker-fail") == 0) {
        rc = cmd_worker_fail(sub_argc, sub_argv);
    } else if (strcmp(cmd, "rlimit") == 0) {
        rc = cmd_rlimit(sub_argc, sub_argv);
    } else if (strcmp(cmd, "cleanup") == 0) {
        rc = cmd_cleanup(sub_argc, sub_argv);
    } else if (strcmp(cmd, "demo") == 0 || strcmp(cmd, "all") == 0) {
        rc = run_integrated_demo();
    } else {
        fprintf(stderr, "Error: Unknown command '%s'.\n", cmd);
        fprintf(stderr, "Run '%s help' to see the list of valid commands.\n", CC_PROGRAM_NAME);
        rc = CC_ERR_USAGE;
    }

    if (logger_is_running())
        logger_stop(NULL);

    return rc;
}
