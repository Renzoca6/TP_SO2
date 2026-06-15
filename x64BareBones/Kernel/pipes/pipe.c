// Pipes anónimos y nombrados. read y write son bloqueantes: si no hay datos
// o el buffer está lleno, el proceso queda BLOCKED hasta que pipe_notify lo
// despierte.
#include "pipe.h"
#include "process.h"
#include "scheduler.h"

extern void enable_interrupts(void);
extern void disable_interrupts(void);

static Pipe pipes[MAX_PIPES];

// ---------------------------------------------------------------------
void pipe_init(void) {
    for (int i = 0; i < MAX_PIPES; i++) {
        pipes[i].state      = PIPE_FREE;
        pipes[i].id         = i;
        pipes[i].read_pos   = 0;
        pipes[i].write_pos  = 0;
        pipes[i].count      = 0;
        pipes[i].write_open = 0;
        pipes[i].read_open  = 0;
        pipes[i].name[0]    = '\0';
    }
}

// ---------------------------------------------------------------------
int pipe_create(void) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].state == PIPE_FREE) {
            pipes[i].state      = PIPE_ACTIVE;
            pipes[i].read_pos   = 0;
            pipes[i].write_pos  = 0;
            pipes[i].count      = 0;
            pipes[i].write_open = 1;
            pipes[i].read_open  = 1;
            pipes[i].name[0]    = '\0';
            return i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------
int pipe_open(const char *name) {
    if (!name) return -1;

    // Buscar pipe existente con ese nombre
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].state == PIPE_ACTIVE) {
            int j = 0;
            while (pipes[i].name[j] && name[j] && pipes[i].name[j] == name[j])
                j++;
            if (!pipes[i].name[j] && !name[j]) {
                pipes[i].write_open++;
                pipes[i].read_open++;
                return i;
            }
        }
    }

    // No existe: crear uno nuevo con ese nombre
    int id = pipe_create();
    if (id < 0) return -1;

    int k = 0;
    while (k < PIPE_NAME_LEN - 1 && name[k]) {
        pipes[id].name[k] = name[k];
        k++;
    }
    pipes[id].name[k] = '\0';
    return id;
}

// ---------------------------------------------------------------------
int pipe_close(int pipe_id, int side) {
    if (pipe_id < 0 || pipe_id >= MAX_PIPES) return -1;
    Pipe *p = &pipes[pipe_id];
    if (p->state == PIPE_FREE) return -1;

    if (side == 1) {
        // Cerrar extremo de escritura
        if (p->write_open > 0) p->write_open--;
        // Despertar lectores bloqueados para que reciban EOF
        if (p->write_open == 0) pipe_notify(pipe_id);
    } else {
        // Cerrar extremo de lectura
        if (p->read_open > 0) p->read_open--;
        // Despertar escritores bloqueados para que reciban broken pipe
        if (p->read_open == 0) pipe_notify(pipe_id);
    }

    // Liberar el pipe si ambos extremos están cerrados
    if (p->write_open == 0 && p->read_open == 0) {
        p->state = PIPE_FREE;
        p->name[0] = '\0';
    }

    return 0;
}

// ---------------------------------------------------------------------
// Despierta los procesos en BLOCKED que esperan este pipe_id.
// Se puede llamar con interrupciones habilitadas o deshabilitadas.
void pipe_notify(int pipe_id) {
    ProcessInfo infos[MAX_PROCESSES];
    int n = get_process_list(infos, MAX_PROCESSES);

    for (int i = 0; i < n; i++) {
        PCB *pcb = get_process_by_pid(infos[i].pid);
        if (pcb && pcb->state == BLOCKED && pcb->waiting_pipe == pipe_id) {
            pcb->waiting_pipe = -1;
            unblock_process(pcb->pid);
        }
    }
}

// ---------------------------------------------------------------------
// Lectura bloqueante por el mecanismo disable/check/block/enable:
//   - disable_interrupts: sección atómica - evita la race condition
//     entre "buffer vacío" y block_current_process().
//   - Si hay datos: leer y retornar (interrupciones ya habilitadas o
//     deshabilitadas según contexto de llamada - el caller es syscall.c
//     que las maneja).
//   - Si no hay datos: bloquear y hacer enable_interrupts para que el
//     timer pueda disparar y cambiar contexto; al volver, reintentar.
int pipe_read(int pipe_id, char *buf, uint32_t count) {
    if (pipe_id < 0 || pipe_id >= MAX_PIPES) return -1;
    Pipe *p = &pipes[pipe_id];
    if (p->state == PIPE_FREE || p->read_open <= 0) return -1;
    if (count == 0) return 0;

    while (1) {
        disable_interrupts();

        if (p->count > 0) break;        // datos disponibles → salir del loop

        if (p->write_open == 0) {       // EOF: nadie más va a escribir
            enable_interrupts();        // balancear el disable del while
            return 0;
        }

        // Buffer vacío con escritores activos: bloquear hasta que
        // pipe_notify nos despierte.
        PCB *cur = get_current_process();
        if (cur) {
            cur->waiting_pipe = pipe_id;
            block_current_process();    // state = BLOCKED
        }
        yield_process();                // quantum = 0

        // Habilitar interrupciones para que el timer dispare y el
        // scheduler cambie de contexto en el próximo tick.
        enable_interrupts();

        // Aquí el IRQ0 tomará el control, guardará este RSP y switcheará
        // a otro proceso. Al ser desbloqueados (pipe_notify), el scheduler
        // nos volverá a encolar. Cuando retomamos, volvemos al while.
    }

    // Sección crítica: interrupciones deshabilitadas, p->count > 0
    uint32_t to_read = (count < p->count) ? count : p->count;
    for (uint32_t i = 0; i < to_read; i++) {
        buf[i] = p->buffer[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
    }
    p->count -= to_read;

    enable_interrupts();

    // Despertar escritores bloqueados por buffer lleno
    pipe_notify(pipe_id);

    return (int)to_read;
}

// ---------------------------------------------------------------------
// Escritura bloqueante (mismo mecanismo que pipe_read).
int pipe_write(int pipe_id, const char *buf, uint32_t count) {
    if (pipe_id < 0 || pipe_id >= MAX_PIPES) return -1;
    Pipe *p = &pipes[pipe_id];
    if (p->state == PIPE_FREE) return -1;
    if (count == 0) return 0;

    uint32_t written = 0;

    while (written < count) {
        while (1) {
            disable_interrupts();

            if (p->read_open == 0) {    // broken pipe
                enable_interrupts();
                return -1;
            }

            if (p->count < PIPE_BUF_SIZE) break;   // hay espacio

            // Buffer lleno: bloquear hasta que pipe_notify nos despierte.
            PCB *cur = get_current_process();
            if (cur) {
                cur->waiting_pipe = pipe_id;
                block_current_process();
            }
            yield_process();

            enable_interrupts();
        }

        // Sección crítica: interrupciones deshabilitadas, hay espacio
        p->buffer[p->write_pos] = buf[written++];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
        p->count++;

        enable_interrupts();
    }

    // Despertar lectores bloqueados esperando datos
    pipe_notify(pipe_id);

    return (int)written;
}
