// test_proc_cmds.c — test de creación, bloqueo y kill de procesos.
// Comando de shell: "test_proc <max_processes>"
#include <stdint.h>
#include "../include/syscall_call.h"
#include "../include/test_util.h"

// ── Tipos locales ─────────────────────────────────────────────────────

typedef enum {
    PROC_RUNNING = 0,
    PROC_BLOCKED,
    PROC_KILLED
} ProcState;

typedef struct {
    int       pid;
    ProcState state;
} p_rq;

// ── Implementación del test ───────────────────────────────────────────

static int64_t test_processes(uint64_t argc, char *argv[]) {
    uint64_t rq;
    uint64_t alive = 0;
    uint8_t  action;
    uint64_t max_processes;
    char    *argvAux[] = {0};

    if (argc != 1)
        return -1;

    if ((max_processes = (uint64_t)satoi(argv[0])) == 0)
        return -1;

    p_rq p_rqs[max_processes];

    while (1) {
        // Crear max_processes procesos endless_loop
        for (rq = 0; rq < max_processes; rq++) {
            p_rqs[rq].pid = my_create_process(endless_loop, 0, argvAux);
            if (p_rqs[rq].pid == -1) {
                println("test_processes: ERROR creating process");
                return -1;
            }
            p_rqs[rq].state = PROC_RUNNING;
            alive++;
        }

        // Aleatoriamente matar o bloquear procesos hasta que todos mueran
        while (alive > 0) {
            for (rq = 0; rq < max_processes; rq++) {
                action = (uint8_t)(GetUniform(100) % 2);
                switch (action) {
                    case 0:
                        if (p_rqs[rq].state == PROC_RUNNING ||
                            p_rqs[rq].state == PROC_BLOCKED) {
                            if (my_kill(p_rqs[rq].pid) == -1) {
                                println("test_processes: ERROR killing");
                                return -1;
                            }
                            p_rqs[rq].state = PROC_KILLED;
                            my_wait(p_rqs[rq].pid);
                            alive--;
                        }
                        break;
                    case 1:
                        if (p_rqs[rq].state == PROC_RUNNING) {
                            if (my_block(p_rqs[rq].pid) == -1) {
                                println("test_processes: ERROR blocking");
                                return -1;
                            }
                            p_rqs[rq].state = PROC_BLOCKED;
                        }
                        break;
                }
            }
            // Desbloquear con probabilidad 50 %
            for (rq = 0; rq < max_processes; rq++) {
                if (p_rqs[rq].state == PROC_BLOCKED &&
                    (uint8_t)(GetUniform(100) % 2)) {
                    if (my_unblock(p_rqs[rq].pid) == -1) {
                        println("test_processes: ERROR unblocking");
                        return -1;
                    }
                    p_rqs[rq].state = PROC_RUNNING;
                }
            }
        }
    }
}

// ── Wrapper registrado en commands[] ─────────────────────────────────

void cmd_test_proc(int argc, char **argv) {
    if (argc < 2) {
        println("Usage: test_proc <max_processes>");
        return;
    }
    // El test espera argv[0] = max_processes (sin el nombre del comando)
    test_processes((uint64_t)(argc - 1), argv + 1);
}
