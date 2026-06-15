// Semáforos nombrados para IPC.
// registry_lock protege sem_open/sem_close. sems[id].lock protege value y
// blocked_pids en wait/post. En CPU única con interrupciones deshabilitadas
// los spinlocks no giran (los dejamos por si algún día se va a SMP).
#include "sem.h"
#include "process.h"
#include "scheduler.h"

// Declaraciones de las funciones ASM en libasm.asm
extern int  semLock(uint8_t *lock);
extern void semUnlock(uint8_t *lock);
extern void enable_interrupts(void);

static Sem     sems[MAX_SEMS];
static uint8_t registry_lock = 0;   // protege sem_open / sem_close

// ---------------------------------------------------------------------
static int sem_name_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return (a[i] == '\0' && b[i] == '\0');
}

static void sem_name_copy(char *dst, const char *src) {
    int i = 0;
    while (i < MAX_SEM_NAME - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

// ---------------------------------------------------------------------
void sem_init(void) {
    for (int i = 0; i < MAX_SEMS; i++) {
        sems[i].active        = 0;
        sems[i].value         = 0;
        sems[i].lock          = 0;
        sems[i].users         = 0;
        sems[i].blocked_count = 0;
        sems[i].name[0]       = '\0';
    }
    registry_lock = 0;
}

// ---------------------------------------------------------------------
int sem_open(const char *name, uint16_t value) {
    if (!name) return -1;

    semLock(&registry_lock);

    // Buscar semáforo existente con ese nombre
    for (int i = 0; i < MAX_SEMS; i++) {
        if (sems[i].active && sem_name_eq(sems[i].name, name)) {
            sems[i].users++;
            semUnlock(&registry_lock);
            return i;
        }
    }

    // Buscar slot libre
    for (int i = 0; i < MAX_SEMS; i++) {
        if (!sems[i].active) {
            sems[i].active        = 1;
            sems[i].value         = value;
            sems[i].lock          = 0;
            sems[i].users         = 1;
            sems[i].blocked_count = 0;
            sem_name_copy(sems[i].name, name);
            semUnlock(&registry_lock);
            return i;
        }
    }

    semUnlock(&registry_lock);
    return -1;  // sin slots disponibles
}

// ---------------------------------------------------------------------
int sem_close(int sem_id) {
    if (sem_id < 0 || sem_id >= MAX_SEMS) return -1;

    semLock(&registry_lock);

    if (!sems[sem_id].active) {
        semUnlock(&registry_lock);
        return -1;
    }

    if (sems[sem_id].users > 0) sems[sem_id].users--;

    if (sems[sem_id].users == 0) {
        // Despertar todos los procesos bloqueados para evitar deadlock permanente
        for (int i = 0; i < sems[sem_id].blocked_count; i++) {
            unblock_process((uint64_t)sems[sem_id].blocked_pids[i]);
        }
        sems[sem_id].active        = 0;
        sems[sem_id].blocked_count = 0;
        sems[sem_id].name[0]       = '\0';
    }

    semUnlock(&registry_lock);
    return 0;
}

// ---------------------------------------------------------------------
int sem_wait(int sem_id) {
    if (sem_id < 0 || sem_id >= MAX_SEMS) return -1;
    if (!sems[sem_id].active) return -1;

    semLock(&sems[sem_id].lock);

    if (sems[sem_id].value > 0) {
        sems[sem_id].value--;
        semUnlock(&sems[sem_id].lock);
        return 0;
    }

    // value == 0: encolar el proceso actual como bloqueado
    if (sems[sem_id].blocked_count < MAX_BLOCKED) {
        sems[sem_id].blocked_pids[sems[sem_id].blocked_count++] =
            get_current_pid();
    }

    // Soltar el lock ANTES de bloquear: si sem_post corre entre
    // semUnlock y block_current_process, agregará al proceso al ready
    // queue; cuando block_current_process() lo marque BLOCKED en el
    // mismo tick (single-CPU, interrupciones aún deshabilitadas en el
    // contexto de syscall), el scheduler lo sacará correctamente.
    semUnlock(&sems[sem_id].lock);

    PCB *cur = get_current_process();
    block_current_process();    // state = BLOCKED

    // Loop hasta que sem_post realmente desbloquee este proceso.
    // sti+hlt despierta con CUALQUIER interrupt (timer, teclado, etc.),
    // no solo el del scheduler. Si despertamos por un falso positivo
    // (keyboard, IRQ secundaria) state seguirá BLOCKED y reintentamos.
    // Solo cuando unblock_process() cambie state a READY/RUNNING saldremos.
    while (cur->state == BLOCKED) {
        yield_process();        // quantum = 0
        __asm__ __volatile__("sti; hlt");
    }

    return 0;
}

// ---------------------------------------------------------------------
int sem_post(int sem_id) {
    if (sem_id < 0 || sem_id >= MAX_SEMS) return -1;
    if (!sems[sem_id].active) return -1;

    semLock(&sems[sem_id].lock);

    if (sems[sem_id].blocked_count > 0) {
        // Despertar el primer proceso de la cola (FIFO)
        int pid = sems[sem_id].blocked_pids[0];

        // Compactar el array de bloqueados
        sems[sem_id].blocked_count--;
        for (int i = 0; i < sems[sem_id].blocked_count; i++) {
            sems[sem_id].blocked_pids[i] = sems[sem_id].blocked_pids[i + 1];
        }

        semUnlock(&sems[sem_id].lock);
        unblock_process((uint64_t)pid);
    } else {
        sems[sem_id].value++;
        semUnlock(&sems[sem_id].lock);
    }

    return 0;
}

// ---------------------------------------------------------------------
int sem_get_value(int sem_id) {
    if (sem_id < 0 || sem_id >= MAX_SEMS) return -1;
    if (!sems[sem_id].active) return -1;

    return (int)sems[sem_id].value;
}

// ---------------------------------------------------------------------
// Llamar desde kill_process() cuando un proceso muere mientras está
// bloqueado en un semáforo, para no dejar PIDs huérfanos en la cola.
void sem_remove_pid(int pid) {
    for (int i = 0; i < MAX_SEMS; i++) {
        if (!sems[i].active) continue;

        semLock(&sems[i].lock);

        for (int j = 0; j < sems[i].blocked_count; j++) {
            if (sems[i].blocked_pids[j] == pid) {
                sems[i].blocked_count--;
                for (int k = j; k < sems[i].blocked_count; k++) {
                    sems[i].blocked_pids[k] = sems[i].blocked_pids[k + 1];
                }
                j--;    // reajustar índice tras compactar
            }
        }

        semUnlock(&sems[i].lock);
    }
}
