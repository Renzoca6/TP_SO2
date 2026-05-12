#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 64
#define STACK_SIZE    0x4000   // 16 KB de stack por proceso
#define PRIORITY_LEVELS 5

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    ZOMBIE,
    KILLED
} ProcessState;

typedef struct PCB {
    uint64_t pid;
    char     name[32];
    uint64_t rsp;            // puntero a stack guardado (apunta al frame de registros)
    uint64_t rbp;
    ProcessState state;
    int      priority;       // 0 = más alta, 4 = más baja
    int      foreground;     // 1 = primer plano, 0 = fondo
    uint64_t stack_base;
    uint64_t parent_pid;
    struct PCB *next;        // enlace de cola de listos
    struct PCB *prev;        // enlace de cola de listos
} PCB;

/* Administración de la tabla de procesos */
void init_processes(void);
int  create_process(void *entry_point, const char *name, int priority, int foreground);
void kill_process(uint64_t pid);
PCB *get_process_by_pid(uint64_t pid);
PCB *get_current_process(void);
void set_current_process(PCB *pcb);

/* Trampolín para procesos que retornan de _start() */
void process_exit_trampoline(void);

/* Declaraciones adelantadas — definidas en scheduler.c */
void add_to_ready_queue(PCB *pcb);
void remove_from_ready_queue(PCB *pcb);

#endif
