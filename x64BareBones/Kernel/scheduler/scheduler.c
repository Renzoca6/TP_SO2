#include <stddef.h>
#include "scheduler.h"
#include "memory_manager.h"
#include "timer.h"

static PCB *ready_queues[PRIORITY_LEVELS] = { NULL };
       PCB *current_process = NULL;
static uint64_t quantum = 0;
static PCB *to_free = NULL;

/* ------------------------------------------------------------------ */
/*  Inicialización                                                    */
/* ------------------------------------------------------------------ */
void init_scheduler(void) {
    for (int i = 0; i < PRIORITY_LEVELS; i++) {
        ready_queues[i] = NULL;
    }
    current_process = NULL;
    quantum = 0;
    to_free = NULL;
}

/* ------------------------------------------------------------------ */
/*  Ayudantes de cola de listos                                       */
/* ------------------------------------------------------------------ */
void add_to_ready_queue(PCB *pcb) {
    if (!pcb) return;
    if (pcb->state == KILLED) return;

    int prio = pcb->priority;
    if (prio < 0 || prio >= PRIORITY_LEVELS) {
        prio = 2;
    }

    pcb->state = READY;
    pcb->next = NULL;
    pcb->prev = NULL;

    if (!ready_queues[prio]) {
        ready_queues[prio] = pcb;
        return;
    }

    PCB *tail = ready_queues[prio];
    while (tail->next) {
        tail = tail->next;
    }
    tail->next = pcb;
    pcb->prev = tail;
}

void remove_from_ready_queue(PCB *pcb) {
    if (!pcb) return;

    int prio = pcb->priority;
    if (prio < 0 || prio >= PRIORITY_LEVELS) return;

    if (ready_queues[prio] == pcb) {
        ready_queues[prio] = pcb->next;
    }

    if (pcb->prev) {
        pcb->prev->next = pcb->next;
    }
    if (pcb->next) {
        pcb->next->prev = pcb->prev;
    }

    pcb->next = NULL;
    pcb->prev = NULL;
}

/* ------------------------------------------------------------------ */
/*  Elegir proceso listo de mayor prioridad                           */
/* ------------------------------------------------------------------ */
static PCB *pick_next_process(void) {
    for (int i = 0; i < PRIORITY_LEVELS; i++) {
        if (ready_queues[i]) {
            return ready_queues[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Punto de entrada principal del scheduler — llamado desde handler  */
/*  ASM del timer                                                     */
/*                                                                    */
/*  current_rsp : RSP después de pushState (apunta a regs guardados)  */
/*  retorno     : nuevo RSP al que cambiar, o 0 si no hay cambio     */
/* ------------------------------------------------------------------ */
uint64_t schedule(uint64_t current_rsp) {
    /* Liberar stack del proceso matado en el tick anterior */
    if (to_free) {
        if (to_free->stack_base) {
            mm_free((void *)to_free->stack_base);
            to_free->stack_base = 0;
        }
        to_free->pid = 0;
        to_free = NULL;
    }

    /* Si el proceso actual sigue corriendo y le resta quantum, no cambiar */
    if (current_process && current_process->state == RUNNING && quantum > 0) {
        quantum--;
        return 0;
    }

    /* Guardar contexto del proceso actualmente en ejecución */
    if (current_process) {
        current_process->rsp = current_rsp;

        if (current_process->state == RUNNING) {
            add_to_ready_queue(current_process);
        } else if (current_process->state == KILLED) {
            to_free = current_process;
        }
    }

    /* Elegir siguiente proceso, saltando los matados estando en cola */
    PCB *next = pick_next_process();
    while (next && next->state == KILLED) {
        remove_from_ready_queue(next);
        if (next->stack_base) {
            mm_free((void *)next->stack_base);
            next->stack_base = 0;
        }
        next->pid = 0;
        next = pick_next_process();
    }

    if (next) {
        remove_from_ready_queue(next);
        next->state = RUNNING;
        current_process = next;
        quantum = 5;                 // quantum de 5 ms a 1000 Hz
        return next->rsp;
    }

    /* No hay proceso listo — nunca debería pasar si existe idle */
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Bloquear / desbloquear                                            */
/* ------------------------------------------------------------------ */
void block_current_process(void) {
    if (current_process && current_process->state == RUNNING) {
        current_process->state = BLOCKED;
    }
}

void unblock_process(uint64_t pid) {
    PCB *pcb = get_process_by_pid(pid);
    if (pcb && pcb->state == BLOCKED) {
        add_to_ready_queue(pcb);
    }
}
