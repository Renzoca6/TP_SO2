#include "process.h"
#include "memory_manager.h"
#include "sem.h"
#include "scheduler.h"
#include <string.h>
#include "interrupts.h"

static PCB process_table[MAX_PROCESSES];
static uint64_t next_pid = 1;
static uint64_t shell_pid = (uint64_t)-1;

void init_processes(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = KILLED;
        process_table[i].pid = 0;
        process_table[i].next = NULL;
        process_table[i].prev = NULL;
        process_table[i].stack_base = 0;
        process_table[i].argv_data = 0;
        process_table[i].fd[0] = -1;
        process_table[i].fd[1] = -1;
        process_table[i].waiting_pipe = -1;
        process_table[i].waiting_for = 0;
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

void process_exit_trampoline(void) {
    exit_current_process();
    yield_process();
    while (1) {
        _hlt();
    }
}

int create_process(void *entry_point, const char *name, int priority, int foreground, int argc, char **argv) {
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
    for (i = 0; i < MAX_PROCESS_NAME - 1 && name[i]; i++) {
        pcb->name[i] = name[i];
    }
    pcb->name[i] = '\0';
    pcb->state = READY;
    pcb->priority = priority;
    pcb->foreground = foreground;
    pcb->stack_base = stack;
    pcb->parent_pid = get_current_pid();
    pcb->next = NULL;
    pcb->prev = NULL;
    pcb->argv_data = 0;
    pcb->fd[0] = -1;        // stdin = terminal por defecto
    pcb->fd[1] = -1;        // stdout = terminal por defecto
    pcb->waiting_pipe = -1;
    pcb->waiting_for = 0;

    uint64_t *top = (uint64_t *)(stack + STACK_SIZE);
    top[-1] = (uint64_t)process_exit_trampoline;

    uint64_t *frame = top - 1 - 20;

    for (int j = 0; j < 15; j++) {
        frame[j] = 0;
    }

    frame[15] = (uint64_t)entry_point;
    frame[16] = 0x08;
    frame[17] = 0x202;
    frame[18] = (uint64_t)(stack + STACK_SIZE - 8);
    frame[19] = 0x00;

    if (argc > 0 && argv != NULL) {
        uint64_t strings_size = 0;
        for (int j = 0; j < argc; j++) {
            const char *s = argv[j];
            if (s) {
                while (*s) {
                    strings_size++;
                    s++;
                }
                strings_size++;
            } else {
                strings_size++;
            }
        }

        uint64_t ptr_array_size = (uint64_t)(argc + 1) * sizeof(char *);
        uint64_t total_size = ptr_array_size + strings_size;
        uint64_t align = total_size % 16;
        if (align != 0) {
            total_size += 16 - align;
        }

        char *buf = (char *)mm_alloc(total_size);
        if (buf) {
            pcb->argv_data = (uint64_t)buf;

            char **ptrs = (char **)buf;
            char *str_area = buf + ptr_array_size;

            uint64_t offset = 0;
            for (int j = 0; j < argc; j++) {
                ptrs[j] = str_area + offset;
                if (argv[j]) {
                    const char *src = argv[j];
                    while (*src) {
                        str_area[offset++] = *src++;
                    }
                    str_area[offset++] = '\0';
                } else {
                    str_area[offset++] = '\0';
                }
            }
            ptrs[argc] = NULL;

            frame[9] = (uint64_t)argc;
            frame[8] = (uint64_t)ptrs;
        } else {
            frame[9] = 0;
            frame[8] = 0;
        }
    } else {
        frame[9] = 0;
        frame[8] = 0;
    }

    pcb->rsp = (uint64_t)frame;
    pcb->rbp = stack + STACK_SIZE - 8;

    add_to_ready_queue(pcb);
    return pcb->pid;
}

void kill_process(uint64_t pid) {
    PCB *pcb = get_process_by_pid(pid);
    if (!pcb) return;
    if (pcb->state == KILLED || pcb->state == ZOMBIE) return;
    // Sacarlo de la ready queue MIENTRAS los punteros prev/next siguen
    // siendo válidos. Si lo dejamos adentro, cuando el slot del PCB se
    // recicle vía get_free_pcb, los PCB vecinos seguirán apuntando acá
    // con next/prev colgados, y add_to_ready_queue terminará creando un
    // self-loop al recorrer la cola en busca de la tail.
    remove_from_ready_queue(pcb);
    pcb->state = ZOMBIE;
    sem_remove_pid((int)pid);
    PCB *parent = get_process_by_pid(pcb->parent_pid);
    if (parent && parent->waiting_for == pid) {
        parent->waiting_for = 0;
        unblock_process(parent->pid);
    }
}

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

int get_current_pid(void) {
    PCB *cur = get_current_process();
    return cur ? (int)cur->pid : -1;
}

int get_process_list(ProcessInfo *buf, int max_count) {
    if (!buf || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < MAX_PROCESSES && count < max_count; i++) {
        if (process_table[i].pid > 0 && process_table[i].state != KILLED) {
            buf[count].pid = process_table[i].pid;
            for (int j = 0; j < MAX_PROCESS_NAME; j++) {
                buf[count].name[j] = process_table[i].name[j];
            }
            buf[count].rsp = process_table[i].rsp;
            buf[count].rbp = process_table[i].rbp;
            buf[count].priority = process_table[i].priority;
            buf[count].state = (int)process_table[i].state;
            buf[count].foreground = process_table[i].foreground;
            count++;
        }
    }
    return count;
}

int set_process_priority(uint64_t pid, int new_priority) {
    if (new_priority < 0 || new_priority >= PRIORITY_LEVELS) {
        return -1;
    }

    PCB *pcb = get_process_by_pid(pid);
    if (!pcb || pcb->state == KILLED) {
        return -1;
    }

    int old_priority = pcb->priority;
    pcb->priority = new_priority;

    if (pcb->state == READY && old_priority != new_priority) {
        remove_from_ready_queue(pcb);
        add_to_ready_queue(pcb);
    }

    return 0;
}

void exit_current_process(void) {
    PCB *cur = get_current_process();
    if (cur) {
        // current_process no debería estar en la ready queue (se sacó al
        // ser elegido), pero llamamos por las dudas — remove es idempotente.
        remove_from_ready_queue(cur);
        cur->state = ZOMBIE;
        PCB *parent = get_process_by_pid(cur->parent_pid);
        if (parent && parent->waiting_for == cur->pid) {
            parent->waiting_for = 0;
            unblock_process(parent->pid);
        }
    }
}

void set_shell_pid(uint64_t pid) {
    shell_pid = pid;
}

void kill_foreground_process(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != KILLED &&
            process_table[i].state != ZOMBIE &&
            process_table[i].pid != 0 &&
            process_table[i].foreground == 1 &&
            process_table[i].pid != shell_pid) {
            kill_process(process_table[i].pid);
        }
    }
}

static void free_pcb_if_zombie(PCB *pcb) {
    if (!pcb || pcb->state != ZOMBIE) return;
    // Defensa adicional: ya debería haberse sacado al pasar a ZOMBIE,
    // pero garantizamos que no queden referencias colgadas antes de
    // que el slot se recicle vía get_free_pcb.
    remove_from_ready_queue(pcb);
    if (pcb->stack_base) {
        mm_free((void *)pcb->stack_base);
        pcb->stack_base = 0;
    }
    if (pcb->argv_data) {
        mm_free((void *)pcb->argv_data);
        pcb->argv_data = 0;
    }
    pcb->pid = 0;
    pcb->state = KILLED;
}

int wait_pid(uint64_t pid) {
    if (pid == 0) {
        PCB *parent = get_current_process();
        if (!parent) return -1;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].parent_pid == parent->pid &&
                process_table[i].state == ZOMBIE) {
                int child_pid = (int)process_table[i].pid;
                free_pcb_if_zombie(&process_table[i]);
                return child_pid;
            }
        }
        return -1;
    }

    PCB *child = get_process_by_pid(pid);
    if (!child) return -1;

    if (child->parent_pid != (uint64_t)get_current_pid()) return -1;

    if (child->state == ZOMBIE) {
        free_pcb_if_zombie(child);
        return (int)pid;
    }

    PCB *parent = get_current_process();
    if (!parent) return -1;
    parent->waiting_for = pid;
    block_current_process();
    yield_process();

    // El gate del int 0x80 es interrupt gate → IF=0 durante la syscall.
    // yield_process() sólo setea quantum=0; el switch real necesita un
    // tick del timer, pero las interrupciones están deshabilitadas.
    // Hacemos sti+hlt en loop hasta que el scheduler nos vuelva a elegir
    // (estado != BLOCKED), garantizando que cuando salgamos del loop el
    // hijo ya está muerto y podemos reapearlo sin leakear su stack.
    while (parent->state == BLOCKED) {
        _hlt();
    }

    child = get_process_by_pid(pid);
    if (child && child->state == ZOMBIE) {
        free_pcb_if_zombie(child);
    }
    parent->waiting_for = 0;

    return (int)pid;
}