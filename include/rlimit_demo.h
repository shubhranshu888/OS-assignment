/*
 * rlimit_demo.h - FR7: Process resource limits (setrlimit) and system cleanup verification.
 */
#ifndef CAMPUSCOMPUTE_RLIMIT_DEMO_H
#define CAMPUSCOMPUTE_RLIMIT_DEMO_H

/* Demonstrate CPU resource limit using RLIMIT_CPU */
int run_rlimit_cpu_demo(void);

/* Demonstrate file size resource limit using RLIMIT_FSIZE */
int run_rlimit_fsize_demo(void);

/* Run cleanup verification check */
int run_cleanup_check(void);

/* CLI commands */
int cmd_rlimit(int argc, char **argv);
int cmd_cleanup(int argc, char **argv);

#endif /* CAMPUSCOMPUTE_RLIMIT_DEMO_H */
