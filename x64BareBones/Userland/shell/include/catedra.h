#ifndef CATEDRA_H
#define CATEDRA_H

#include <stdint.h>

int64_t my_create_process(const char *name, uint64_t argc, char **argv);
int64_t my_kill(int32_t pid);
int64_t my_block(int32_t pid);
int64_t my_unblock(int32_t pid);
int32_t my_wait(int32_t pid);
int64_t my_nice(int32_t pid, int64_t new_prio);
int32_t my_getpid(void);
void my_yield(void);
int64_t my_sem_open(const char *name, uint64_t value);
int64_t my_sem_wait(const char *name);
int64_t my_sem_post(const char *name);
int64_t my_sem_close(const char *name);

void endless_loop(void);
void zero_to_max(void);
uint64_t my_process_inc(uint64_t argc, char **argv);

uint64_t test_processes(uint64_t argc, char **argv);
uint64_t test_prio(uint64_t argc, char **argv);
uint64_t test_sync(uint64_t argc, char **argv);

#endif