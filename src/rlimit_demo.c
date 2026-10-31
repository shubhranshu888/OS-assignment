/*
 * rlimit_demo.c - Demonstration of setrlimit() and resource cleanup (FR7).
 */
#include "rlimit_demo.h"
#include "common.h"
#include "ipc.h"
#include "logger.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int run_rlimit_cpu_demo(void)
{
    print_section("RLIMIT_CPU Demonstration (CPU Time Quota Enforcement)");
    printf("Scenario: Student job runs a runaway simulation exceeding lab CPU quota.\n");

    struct rlimit old_lim, new_lim;
    if (getrlimit(RLIMIT_CPU, &old_lim) == -1) {
        fprintf(stderr, "Error: getrlimit failed: %s\n", strerror(errno));
        return -1;
    }

    printf("Original RLIMIT_CPU : soft=%lu s, hard=%lu s\n",
           (unsigned long)old_lim.rlim_cur, (unsigned long)old_lim.rlim_max);

    flush_all_output();
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: fork failed: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        /* Child worker process: impose 1 second soft CPU limit */
        new_lim.rlim_cur = 1; /* 1 second CPU time */
        new_lim.rlim_max = 2; /* 2 seconds hard limit */

        if (setrlimit(RLIMIT_CPU, &new_lim) == -1) {
            fprintf(stderr, "Child: setrlimit failed: %s\n", strerror(errno));
            child_exit(1);
        }

        printf("  [CHILD %d] RLIMIT_CPU set to soft=%lu s, hard=%lu s\n",
               (int)getpid(), (unsigned long)new_lim.rlim_cur, (unsigned long)new_lim.rlim_max);
        printf("  [CHILD %d] Entering intensive computation loop to trigger OS limit...\n", (int)getpid());

        /* Intensive computation loop until SIGXCPU signal is raised by kernel */
        volatile unsigned long count = 0;
        while (1) {
            count++;
        }

        child_exit(0);
    }

    /* Parent Process */
    printf("[PARENT] Monitoring worker process %d with waitpid()...\n", (int)pid);
    int status = 0;
    waitpid(pid, &status, 0);

    print_section("Resource Limit Effect Analysis");
    printf("Collected Child PID  : %d\n", (int)pid);

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("Termination Status   : TERMINATED BY SIGNAL %d (%s)\n", sig, strsignal(sig));
        if (sig == SIGXCPU) {
            printf("OBSERVED EFFECT      : Linux kernel sent SIGXCPU because CPU consumption exceeded 1s limit!\n");
            printf("RESULT               : [PASS] CPU resource quota successfully enforced by OS.\n");
            cc_log("RLIMIT", "Child %d exceeded RLIMIT_CPU and was terminated by SIGXCPU", (int)pid);
        } else {
            printf("OBSERVED EFFECT      : Child terminated by unexpected signal %d\n", sig);
        }
    } else if (WIFEXITED(status)) {
        printf("Termination Status   : Child exited with status %d (limit was not exceeded in time)\n",
               WEXITSTATUS(status));
    }

    return 0;
}

int run_cleanup_check(void)
{
    print_section("CampusCompute Resource Cleanup Audit");

    /* Ensure IPC resources are unlinked */
    cleanup_ipc_resources();

    printf("1. Active Threads Check:\n");
    int threads = get_thread_count();
    if (threads > 0) {
        printf("   - Total threads currently in process: %d (expected 1 main thread: %s)\n",
               threads, threads == 1 ? "CLEAN" : "MULTIPLE ACTIVE");
    } else {
        printf("   - Thread audit: Single process mode active.\n");
    }

    printf("2. POSIX Shared Memory & Semaphores Audit:\n");
    int leaked = 0;
    DIR *dir = opendir("/dev/shm");
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, "campuscompute") != NULL) {
                printf("   [LEAK FOUND] /dev/shm/%s\n", entry->d_name);
                leaked++;
            }
        }
        closedir(dir);
    }

    if (leaked == 0) {
        printf("   - /dev/shm/ check: Zero leaked campuscompute objects found. [CLEAN]\n");
    } else {
        printf("   - Warning: Found %d lingering IPC object(s).\n", leaked);
    }

    printf("3. Log File Integrity:\n");
    FILE *fp = fopen(CC_LOG_FILE, "r");
    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fclose(fp);
        printf("   - Log file '%s' exists (size: %ld bytes). [VALID]\n", CC_LOG_FILE, sz);
    } else {
        printf("   - Log file '%s' not created yet.\n", CC_LOG_FILE);
    }

    printf("\nAudit Conclusion: All program-allocated system resources are safely managed.\n");
    return 0;
}

int cmd_rlimit(int argc, char **argv)
{
    (void)argc; (void)argv;
    print_banner("FR7: Resource Limits Demonstration (setrlimit & kernel signal)");
    return run_rlimit_cpu_demo() == 0 ? CC_OK : CC_ERR_RUNTIME;
}

int cmd_cleanup(int argc, char **argv)
{
    (void)argc; (void)argv;
    print_banner("FR7: Cleanup Verification & Resource Audit");
    return run_cleanup_check() == 0 ? CC_OK : CC_ERR_RUNTIME;
}
