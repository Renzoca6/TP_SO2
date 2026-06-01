#include "../include/syscall_call.h"
#include "../include/test_util.h"
#include "../utils/utils.h"

static volatile int64_t g_global;

static uint64_t g_n;
static int      g_use_sem;
static int      g_sem_id;

// LCG entero para evitar floating point. El kernel se compila con
// -mno-sse y no guarda el estado x87/SSE en context switch, por lo que
// GetUniform() (que usa FP) devuelve basura cuando hay procesos
// concurrentes. Esta versión es 100% entera y deterministicamente
// pseudoaleatoria.
static volatile uint32_t slow_seed = 0xC0FFEE;

static uint32_t slow_rand(void) {
    uint32_t s = slow_seed;
    s = s * 1664525u + 1013904223u;
    slow_seed = s;
    return s;
}

static void slowInc(volatile int64_t *p, int64_t inc) {
    int64_t aux = *p;
    if ((slow_rand() % 100) < 30)
        yield();
    aux += inc;
    *p = aux;
}

void sync_dec_worker(void) {
    uint64_t n       = g_n;
    int      use_sem = g_use_sem;
    int      sem_id  = g_sem_id;

    for (uint64_t i = 0; i < n; i++) {
        if (use_sem) sem_wait(sem_id);
        slowInc(&g_global, -1);
        if (use_sem) sem_post(sem_id);
    }
    exit_process();
}

void sync_inc_worker(void) {
    uint64_t n       = g_n;
    int      use_sem = g_use_sem;
    int      sem_id  = g_sem_id;

    for (uint64_t i = 0; i < n; i++) {
        if (use_sem) sem_wait(sem_id);
        slowInc(&g_global, 1);
        if (use_sem) sem_post(sem_id);
    }
    exit_process();
}

void cmd_test_sync(int argc, char **argv) {
    if (argc != 4) {
        println("Usage: test_sync <n> <pairs> <use_sem>");
        return;
    }

    uint64_t n       = (uint64_t)satoi(argv[1]);
    int      pairs   = (int)satoi(argv[2]);
    int      use_sem = (int)satoi(argv[3]);

    if (n == 0 || pairs <= 0 || pairs > 32) {
        println("test_sync: invalid arguments");
        return;
    }

    int sem_id = -1;
    if (use_sem) {
        sem_id = sem_open("tsync_sem", 1);
        if (sem_id < 0) {
            println("test_sync: ERROR opening semaphore");
            return;
        }
    }

    g_global  = 0;
    g_n       = n;
    g_use_sem = use_sem;
    g_sem_id  = sem_id;

    int pids[64];

    for (int i = 0; i < pairs; i++)
        pids[i] = create_process((void *)sync_dec_worker, "tsync_dec", 2, 0, 0, 0);

    for (int i = 0; i < pairs; i++)
        pids[pairs + i] = create_process((void *)sync_inc_worker, "tsync_inc", 2, 0, 0, 0);

    for (int i = 0; i < pairs * 2; i++) {
        if (pids[i] > 0)
            waitpid((uint64_t)pids[i]);
    }

    if (use_sem)
        sem_close(sem_id);

    char buf[32];
    write("Final value: ");
    if (g_global < 0) {
        write("-");
        uintToBase((uint64_t)(-g_global), buf, 10);
    } else {
        uintToBase((uint64_t)g_global, buf, 10);
    }
    write(buf);
    write("\n");
}
