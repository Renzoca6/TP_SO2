// test_proc_cmds.c — test de creación, bloqueo y kill de procesos.
// Comando de shell: "test_proc <max_processes>"
#include <stdint.h>
#include "../include/syscall_call.h"
#include "../include/test_util.h"
#include "../utils/utils.h"

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

static int is_foreground(void) {
    ProcessInfo buf[64];
    int pid = getpid();
    int n = ps(buf, 64);
    for (int i = 0; i < n; i++) {
        if ((int)buf[i].pid == pid)
            return buf[i].foreground;
    }
    return 0;
}

static int64_t test_processes(uint64_t argc, char *argv[]) {
    uint64_t rq;
    uint64_t alive = 0;
    uint64_t iteration = 0;
    uint8_t  action;
    uint64_t max_processes;
    char    *argvAux[] = {0};

    if (argc != 1)
        return -1;

    if ((max_processes = (uint64_t)satoi(argv[0])) == 0)
        return -1;

    p_rq p_rqs[max_processes];

    while (1) {
        iteration++;

        // Crear max_processes procesos endless_loop con prioridad normal (2).
        // Con prioridad 0 (la más alta) dejarían sin CPU al test -> livelock.
        for (rq = 0; rq < max_processes; rq++) {
            p_rqs[rq].pid = my_create_process(endless_loop, 2, argvAux);
            if (p_rqs[rq].pid == -1) {
                println("test_processes: ERROR creating process");
                return -1;
            }
            p_rqs[rq].state = PROC_RUNNING;
            alive++;
        }

        if (iteration % 1000 == 0 && is_foreground()) {
            char nbuf[16];
            write("test_proc: ");
            uintToBase(iteration, nbuf, 10);
            write(nbuf);
            println(" iters OK (Ctrl+C to stop)");
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
