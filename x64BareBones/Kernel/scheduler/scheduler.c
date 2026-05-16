#include <stddef.h>
#include "scheduler.h"
#include "memory_manager.h"
#include "timer.h"

static PCB *ready_queues[PRIORITY_LEVELS] = { NULL };
       PCB *current_process = NULL;
static uint64_t quantum = 0;
static PCB *to_free = NULL;

void init_scheduler(void) {
    for (int i = 0; i < PRIORITY_LEVELS; i++) {
        ready_queues[i] = NULL;
    }
    current_process = NULL;
    quantum = 0;
    to_free = NULL;
}

void remove_from_ready_queue(PCB *pcb) {
    if (!pcb) return;

    // Buscar en TODAS las colas, no solo en la de pcb->priority,
    // porque la prioridad puede haber cambiado mientras estaba encolado.
    for (int i = 0; i < PRIORITY_LEVELS; i++) {
        if (ready_queues[i] == pcb) {
            ready_queues[i] = pcb->next;
            break;
        }
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

void add_to_ready_queue(PCB *pcb) {
    if (!pcb) return;
    if (pcb->state == KILLED || pcb->state == ZOMBIE) return;

    // Idempotente: si ya estaba en alguna cola, lo sacamos primero
    // (esto evita corromper los punteros next/prev al re-encolar).
    remove_from_ready_queue(pcb);

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

static PCB *pick_next_process(void) {
    for (int i = 0; i < PRIORITY_LEVELS; i++) {
        if (ready_queues[i]) {
            return ready_queues[i];
        }
    }
    return NULL;
}

static void free_pcb_resources(PCB *pcb) {
    if (!pcb) return;
    if (pcb->stack_base) {
        mm_free((void *)pcb->stack_base);
        pcb->stack_base = 0;
    }
    if (pcb->argv_data) {
        mm_free((void *)pcb->argv_data);
        pcb->argv_data = 0;
    }
    pcb->pid = 0;
}

uint64_t schedule(uint64_t current_rsp) {
    if (to_free) {
        free_pcb_resources(to_free);
        to_free = NULL;
    }

    if (current_process && current_process->state == RUNNING && quantum > 0) {
        quantum--;
        return 0;
    }

    if (current_process) {
        current_process->rsp = current_rsp;

        if (current_process->state == RUNNING) {
            add_to_ready_queue(current_process);
        } else if (current_process->state == KILLED) {
            free_pcb_resources(current_process);
        }
        // ZOMBIE: no re-enqueue, but don't free either (parent will reap with wait_pid)
    }

    PCB *next = pick_next_process();
    while (next && (next->state == KILLED || next->state == ZOMBIE)) {
        if (next->state == KILLED) {
            remove_from_ready_queue(next);
            free_pcb_resources(next);
        } else {
            remove_from_ready_queue(next);
        }
        next = pick_next_process();
    }

    if (next) {
        remove_from_ready_queue(next);
        next->state = RUNNING;
        current_process = next;
        quantum = 5;
        return next->rsp;
    }

    return 0;
}

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

void yield_process(void) {
    quantum = 0;
}