#include "process.h"
#include "memory_manager.h"
#include <string.h>
#include "interrupts.h"

static PCB process_table[MAX_PROCESSES];
static uint64_t next_pid = 1;

void init_processes(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = KILLED;
        process_table[i].pid = 0;
        process_table[i].next = NULL;
        process_table[i].prev = NULL;
        process_table[i].stack_base = 0;
    }
}

static PCB *get_free_pcb(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == KILLED && process_table[i].pid == 0) {
            return &process_table[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Trampolín: si un proceso retorna de _start(), cae acá             */
/* ------------------------------------------------------------------ */
void process_exit_trampoline(void) {
    while (1) {
        _hlt();
    }
}

/* ------------------------------------------------------------------ */
/*  Crear un proceso nuevo                                            */
/* ------------------------------------------------------------------ */
int create_process(void *entry_point, const char *name, int priority, int foreground) {
    if (priority < 0 || priority >= PRIORITY_LEVELS) {
        priority = 2;
    }

    PCB *pcb = get_free_pcb();
    if (!pcb) {
        return -1;
    }

    uint64_t stack = (uint64_t)mm_alloc(STACK_SIZE);
    if (!stack) {
        return -1;
    }

    pcb->pid = next_pid++;
    int i;
    for (i = 0; i < 31 && name[i]; i++) {
        pcb->name[i] = name[i];
    }
    pcb->name[i] = '\0';
    pcb->state = READY;
    pcb->priority = priority;
    pcb->foreground = foreground;
    pcb->stack_base = stack;
    pcb->parent_pid = 0;
    pcb->next = NULL;
    pcb->prev = NULL;

    /*
     * Armar un frame falso de interrupción en el stack del proceso para que
     * el primer context switch (vía iretq) arranque el proceso correctamente.
     *
     * Layout del stack esperado por popState + iretq:
     *   [+0]  R15        (dirección más baja)
     *   [+8]  R14
     *   ...
     *   [+112] RAX
     *   [+120] RIP       <- empujado por el CPU
     *   [+128] CS
     *   [+136] RFLAGS
     *   [+144] RSP       <- valor restaurado por el CPU en iretq
     *   [+152] SS        (dirección más alta del frame)
     *
     * También colocamos process_exit_trampoline en la palabra del tope
     * para que si _start() hace "leave; ret" caiga ahí de forma segura.
     */
    uint64_t *top = (uint64_t *)(stack + STACK_SIZE);
    top[-1] = (uint64_t)process_exit_trampoline;

    uint64_t *frame = top - 1 - 20;   // 20 qwords = 15 GPRs + 5 CPU fields

    for (int i = 0; i < 15; i++) {
        frame[i] = 0;
    }

    frame[15] = (uint64_t)entry_point;                // RIP
    frame[16] = 0x08;                                  // CS
    frame[17] = 0x202;                                 // RFLAGS (IF=1)
    frame[18] = (uint64_t)(stack + STACK_SIZE - 8);    // RSP after iretq
    frame[19] = 0x00;                                  // SS

    pcb->rsp = (uint64_t)frame;
    pcb->rbp = stack + STACK_SIZE - 8;

    add_to_ready_queue(pcb);
    return pcb->pid;
}

/* ------------------------------------------------------------------ */
/*  Matar un proceso (marcarlo; el scheduler limpia después)          */
/* ------------------------------------------------------------------ */
void kill_process(uint64_t pid) {
    PCB *pcb = get_process_by_pid(pid);
    if (!pcb) return;
    if (pcb->state == KILLED) return;
    pcb->state = KILLED;
}

/* ------------------------------------------------------------------ */
/*  Buscadores auxiliares                                             */
/* ------------------------------------------------------------------ */
PCB *get_process_by_pid(uint64_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

PCB *get_current_process(void) {
    extern PCB *current_process;
    return current_process;
}

void set_current_process(PCB *pcb) {
    extern PCB *current_process;
    current_process = pcb;
}
