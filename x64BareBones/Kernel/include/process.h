#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 64
#define STACK_SIZE    0x4000
#define PRIORITY_LEVELS 5
#define MAX_PROCESS_NAME 32

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    ZOMBIE,
    KILLED
} ProcessState;

typedef struct PCB {
    uint64_t pid;
    char     name[MAX_PROCESS_NAME];
    uint64_t rsp;
    uint64_t rbp;
    ProcessState state;
    int      priority;
    int      foreground;
    uint64_t stack_base;
    uint64_t parent_pid;
    uint64_t argv_data;
    struct PCB *next;
    struct PCB *prev;
} PCB;

typedef struct {
    uint64_t pid;
    char     name[MAX_PROCESS_NAME];
    uint64_t rsp;
    uint64_t rbp;
    int      priority;
    int      state;
    int      foreground;
} ProcessInfo;

void init_processes(void);
int  create_process(void *entry_point, const char *name, int priority, int foreground, int argc, char **argv);
void kill_process(uint64_t pid);
PCB *get_process_by_pid(uint64_t pid);
PCB *get_current_process(void);
void set_current_process(PCB *pcb);
int  get_current_pid(void);
int  get_process_list(ProcessInfo *buf, int max_count);
int  set_process_priority(uint64_t pid, int new_priority);
void exit_current_process(void);

void process_exit_trampoline(void);

void add_to_ready_queue(PCB *pcb);
void remove_from_ready_queue(PCB *pcb);

#endif