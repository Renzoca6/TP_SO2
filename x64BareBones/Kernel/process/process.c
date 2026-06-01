#include "process.h"
#include "memory_manager.h"
#include "sem.h"
#include "scheduler.h"
#include <string.h>
#include "interrupts.h"

static PCB process_table[MAX_PROCESSES];
static uint64_t next_pid = 1;
static uint64_t shell_pid = (uint64_t)-1;

// ---------------------------------------------------------------------
// Helpers para el mecanismo de waitpid basado en semáforos.
// El semáforo "wait_<pid>" es creado en create_process (users=1, value=0)
// y destruido por wait_child (último sem_close lleva users a 0).
// ---------------------------------------------------------------------
static void uint64_to_dec_str(uint64_t n, char *buf) {
    char tmp[21];
    int i = 0;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (n > 0) { tmp[i++] = (char)('0' + n % 10); n /= 10; }
    int j;
    for (j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[j] = '\0';
}

char *build_wait_sem_name(uint64_t pid) {
    if (pid == 0) return NULL;
    char num[21];
    uint64_to_dec_str(pid, num);
    int num_len = 0;
    while (num[num_len]) num_len++;
    // "wait_" (5) + num + '\0'
    char *buf = (char *)mm_alloc((uint64_t)(6 + num_len));
    if (!buf) return NULL;
    buf[0]='w'; buf[1]='a'; buf[2]='i'; buf[3]='t'; buf[4]='_';
    for (int i = 0; i < num_len; i++) buf[5 + i] = num[i];
    buf[5 + num_len] = '\0';
    return buf;
}

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
    while (1) { _hlt(); }
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

    char *wait_sem_name = build_wait_sem_name(pcb->pid);
    if (wait_sem_name) {
        int sem_id = sem_open(wait_sem_name, 0);
        (void)sem_id;
        mm_free(wait_sem_name);
    }

    return pcb->pid;
}

void kill_process(uint64_t pid) {
    PCB *pcb = get_process_by_pid(pid);
    if (!pcb) return;
    if (pcb->state == KILLED) return;

    // Despertar a quien esté esperando este pid con waitpid (vale para kill
    // por syscall y para Ctrl+C).
    char *sem_name = build_wait_sem_name(pid);
    if (sem_name) {
        int sem_id = sem_open(sem_name, 0);
        if (sem_id >= 0) {
            sem_post(sem_id);
            sem_close(sem_id);
        }
        mm_free(sem_name);
    }

    // Sacarlo de las colas de semáforos por si moría bloqueado en uno.
    sem_remove_pid((int)pid);

    if (pcb == get_current_process()) {
        // Se mata a sí mismo: lo libera el scheduler en el próximo switch
        // (no podemos liberar el stack en uso).
        pcb->state = KILLED;
        return;
    }

    // No es el actual: si está READY sacarlo de la cola, y liberarlo ya
    // (así no quedan PCBs/stacks colgados al matar procesos bloqueados).
    if (pcb->state == READY) {
        remove_from_ready_queue(pcb);
    }
    pcb->state = KILLED;
    free_killed_process(pcb);
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

    if (pcb->state == READY && old_priority != new_priority) {
        // Importante: remover usando la prioridad VIEJA antes de cambiarla,
        // si no remove_from_ready_queue busca en la cola equivocada y deja
        // el proceso enganchado en la cola vieja con punteros rotos.
        remove_from_ready_queue(pcb);
        pcb->priority = new_priority;
        add_to_ready_queue(pcb);
    } else {
        pcb->priority = new_priority;
    }

    return 0;
}

void exit_current_process(void) {
    PCB *cur = get_current_process();
    if (!cur) return;

    // Despertar al padre bloqueado en wait_child antes de marcar como KILLED.
    char *sem_name = build_wait_sem_name(cur->pid);
    if (sem_name) {
        int sem_id = sem_open(sem_name, 0);
        if (sem_id >= 0) {
            sem_post(sem_id);
            sem_close(sem_id);
        }
        mm_free(sem_name);
    }

    cur->state = KILLED;
}

void set_shell_pid(uint64_t pid) {
    shell_pid = pid;
}

void kill_foreground_process(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != KILLED &&
            process_table[i].pid != 0 &&
            process_table[i].foreground == 1 &&
            process_table[i].pid != shell_pid) {
            kill_process(process_table[i].pid);
        }
    }
}

// ---------------------------------------------------------------------
// Espera a que el proceso hijo termine (bloquea al proceso actual).
//
// Ciclo de vida del semáforo "wait_<pid>":
//   - create_process: sem_open (users=1, value=0)
//   - kill/exit:      sem_open (users=2) → sem_post → sem_close (users=1)
//   - wait_child:     sem_open (users=2) → sem_wait → sem_close (users→0, destruido)
// ---------------------------------------------------------------------
int wait_child(uint64_t child_pid) {
    char *sem_name = build_wait_sem_name(child_pid);
    if (!sem_name) return 0;

    int sem_id = sem_open(sem_name, 0);
    mm_free(sem_name);
    if (sem_id < 0) return 0;

    sem_wait(sem_id);
    sem_close(sem_id);
    return 0;
}