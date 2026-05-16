#include "../include/catedra.h"
#include "../include/syscall_call.h"
#include "../include/test_util.h"
#include "../utils/utils.h"

#define MAX_SEM_CACHE 16
#define MAX_PROCS_DISPATCH 16

typedef struct {
    char name[32];
    int  sem_id;
} sem_cache_entry_t;

static sem_cache_entry_t sem_cache[MAX_SEM_CACHE];
static int sem_cache_count = 0;

typedef void (*proc_entry_t)(void);

typedef struct {
    const char *name;
    proc_entry_t entry;
} dispatch_entry_t;

static dispatch_entry_t dispatch_table[MAX_PROCS_DISPATCH];
static int dispatch_count = 0;

static volatile uint64_t max_value_global = 0;
static volatile int64_t sync_global = 0;

static void my_puts(const char *s) {
    write(s);
}

static void my_put_int(int64_t n) {
    char buf[20];
    if (n < 0) {
        write("-");
        n = -n;
    }
    uintToBase((uint64_t)n, buf, 10);
    write(buf);
}

static void print_str(const char *s) { write(s); }
static void print_int(int64_t n) { my_put_int(n); }
static void print_endl(void) { write("\n"); }

void zero_to_max(void) {
    uint64_t value = 0;
    while (value++ != max_value_global)
        ;
    print_str("PROCESS ");
    print_int(my_getpid());
    print_str(" DONE!");
    print_endl();
    exit_process();
}

void slow_inc(volatile int64_t *p, int64_t inc) {
    uint64_t aux = (uint64_t)*p;
    if (GetUniform(100) < 30)
        yield();
    aux += inc;
    *p = (int64_t)aux;
}

uint64_t my_process_inc(uint64_t argc, char **argv) {
    uint64_t n;
    int8_t inc;
    int8_t use_sem;

    if (argc != 3)
        return 1;

    if ((n = satoi(argv[0])) <= 0)
        return 1;
    if ((inc = (int8_t)satoi(argv[1])) == 0)
        return 1;
    if ((use_sem = (int8_t)satoi(argv[2])) < 0)
        return 1;

    if (use_sem) {
        if (my_sem_open("sem_sync", 1) <= 0) {
            print_str("test_sync: ERROR opening semaphore");
            print_endl();
            return 1;
        }
    }

    for (uint64_t i = 0; i < n; i++) {
        if (use_sem)
            my_sem_wait("sem_sync");
        slow_inc(&sync_global, (int64_t)inc);
        if (use_sem)
            my_sem_post("sem_sync");
    }

    if (use_sem)
        my_sem_close("sem_sync");

    return 0;
}

static void register_process(const char *name, proc_entry_t entry) {
    if (dispatch_count >= MAX_PROCS_DISPATCH) return;
    dispatch_table[dispatch_count].name = name;
    dispatch_table[dispatch_count].entry = entry;
    dispatch_count++;
}

static void init_dispatch_table(void) {
    static int initialized = 0;
    if (initialized) return;
    initialized = 1;

    register_process("endless_loop", endless_loop);
    register_process("zero_to_max", zero_to_max);
    register_process("my_process_inc", (proc_entry_t)my_process_inc);
}

int64_t my_create_process(const char *name, uint64_t argc, char **argv) {
    init_dispatch_table();

    for (int i = 0; i < dispatch_count; i++) {
        int j = 0;
        while (dispatch_table[i].name[j] && name[j] && dispatch_table[i].name[j] == name[j])
            j++;
        if (dispatch_table[i].name[j] == '\0' && name[j] == '\0') {
            int pid = create_process((void *)dispatch_table[i].entry, name, 2, 0, (int)argc, argv);
            return (int64_t)pid;
        }
    }

    print_str("my_create_process: unknown process '");
    print_str(name);
    print_str("'");
    print_endl();
    return -1;
}

int64_t my_kill(int32_t pid) {
    kill_process((uint64_t)pid);
    return 1;
}

int64_t my_block(int32_t pid) {
    block((uint64_t)pid);
    return 1;
}

int64_t my_unblock(int32_t pid) {
    unblock((uint64_t)pid);
    return 1;
}

int32_t my_wait(int32_t pid) {
    return wait_pid((uint64_t)pid);
}

int64_t my_nice(int32_t pid, int64_t new_prio) {
    return (int64_t)nice((uint64_t)pid, (int)new_prio);
}

int32_t my_getpid(void) {
    return getpid();
}

void my_yield(void) {
    yield();
}

static int find_sem_id(const char *name) {
    for (int i = 0; i < sem_cache_count; i++) {
        int j = 0;
        while (sem_cache[i].name[j] && name[j] && sem_cache[i].name[j] == name[j])
            j++;
        if (sem_cache[i].name[j] == '\0' && name[j] == '\0')
            return sem_cache[i].sem_id;
    }
    return -1;
}

static void cache_sem(const char *name, int id) {
    if (sem_cache_count >= MAX_SEM_CACHE) return;
    int i = 0;
    while (name[i] && i < 31) {
        sem_cache[sem_cache_count].name[i] = name[i];
        i++;
    }
    sem_cache[sem_cache_count].name[i] = '\0';
    sem_cache[sem_cache_count].sem_id = id;
    sem_cache_count++;
}

int64_t my_sem_open(const char *name, uint64_t value) {
    int id = sem_open(name, (int)value);
    if (id < 0) return 0;
    cache_sem(name, id);
    return id + 1;
}

int64_t my_sem_wait(const char *name) {
    int id = find_sem_id(name);
    if (id < 0) return -1;
    return (int64_t)sem_wait(id);
}

int64_t my_sem_post(const char *name) {
    int id = find_sem_id(name);
    if (id < 0) return -1;
    return (int64_t)sem_post(id);
}

int64_t my_sem_close(const char *name) {
    int id = find_sem_id(name);
    if (id < 0) return -1;
    for (int i = 0; i < sem_cache_count; i++) {
        int j = 0;
        while (sem_cache[i].name[j] && name[j] && sem_cache[i].name[j] == name[j])
            j++;
        if (sem_cache[i].name[j] == '\0' && name[j] == '\0') {
            sem_cache[i] = sem_cache[sem_cache_count - 1];
            sem_cache_count--;
            break;
        }
    }
    return (int64_t)sem_close(id);
}

uint64_t test_processes(uint64_t argc, char **argv) {
    uint8_t rq;
    uint8_t alive = 0;
    uint8_t action;
    uint64_t max_processes;
    char *argvAux[] = {0};

    if (argc != 1)
        return 1;

    if ((max_processes = satoi(argv[0])) <= 0)
        return 1;

    if (max_processes > 64)
        max_processes = 64;

    int32_t *pids_buf = (int32_t *)malloc(max_processes * sizeof(int32_t) * 2 + max_processes);
    if (!pids_buf) {
        print_str("test_processes: ERROR allocating memory");
        print_endl();
        return 1;
    }
    int *states = (int *)(pids_buf + max_processes);
    for (uint64_t i = 0; i < max_processes; i++) {
        pids_buf[i] = 0;
        states[i] = 0;
    }

    while (1) {
        for (rq = 0; rq < max_processes; rq++) {
            pids_buf[rq] = my_create_process("endless_loop", 0, argvAux);
            if (pids_buf[rq] == -1) {
                print_str("test_processes: ERROR creating process");
                print_endl();
                free(pids_buf);
                return 1;
            } else {
                states[rq] = 1;
                alive++;
            }
        }

        while (alive > 0) {
            for (rq = 0; rq < max_processes; rq++) {
                if (states[rq] == 3)
                    continue;

                action = GetUniform(100) % 2;

                switch (action) {
                    case 0:
                        if (states[rq] == 1 || states[rq] == 2) {
                            if (my_kill(pids_buf[rq]) == -1) {
                                print_str("test_processes: ERROR killing process");
                                print_endl();
                                free(pids_buf);
                                return 1;
                            }
                            states[rq] = 3;
                            my_wait(pids_buf[rq]);
                            alive--;
                        }
                        break;

                    case 1:
                        if (states[rq] == 1) {
                            if (my_block(pids_buf[rq]) == -1) {
                                print_str("test_processes: ERROR blocking process");
                                print_endl();
                                free(pids_buf);
                                return 1;
                            }
                            states[rq] = 2;
                        }
                        break;
                }
            }

            for (rq = 0; rq < max_processes; rq++)
                if (states[rq] == 2 && GetUniform(100) % 2) {
                    if (my_unblock(pids_buf[rq]) == -1) {
                        print_str("test_processes: ERROR unblocking process");
                        print_endl();
                        free(pids_buf);
                        return 1;
                    }
                    states[rq] = 1;
                }
        }

        print_str("test_processes: PASS (all created, blocked, unblocked and killed)");
        print_endl();
        free(pids_buf);
        return 0;
    }
}

#define TOTAL_PROCESSES 3
#define LOWEST 4
#define MEDIUM 2
#define HIGHEST 0

static int64_t prio_values[3] = {LOWEST, MEDIUM, HIGHEST};

uint64_t test_prio(uint64_t argc, char **argv) {
    int64_t pids[TOTAL_PROCESSES];
    char *ztm_argv[] = {0};
    uint64_t i;

    if (argc != 1)
        return 1;

    if ((max_value_global = satoi(argv[0])) <= 0)
        return 1;

    print_str("SAME PRIORITY...");
    print_endl();

    for (i = 0; i < TOTAL_PROCESSES; i++)
        pids[i] = my_create_process("zero_to_max", 0, ztm_argv);

    for (i = 0; i < TOTAL_PROCESSES; i++)
        my_wait(pids[i]);

    print_str("SAME PRIORITY, THEN CHANGE IT...");
    print_endl();

    for (i = 0; i < TOTAL_PROCESSES; i++) {
        pids[i] = my_create_process("zero_to_max", 0, ztm_argv);
        my_nice(pids[i], prio_values[i]);
        print_str("  PROCESS ");
        print_int(pids[i]);
        print_str(" NEW PRIORITY: ");
        print_int(prio_values[i]);
        print_endl();
    }

    for (i = 0; i < TOTAL_PROCESSES; i++)
        my_wait(pids[i]);

    print_str("SAME PRIORITY, THEN CHANGE IT WHILE BLOCKED...");
    print_endl();

    for (i = 0; i < TOTAL_PROCESSES; i++) {
        pids[i] = my_create_process("zero_to_max", 0, ztm_argv);
        my_block(pids[i]);
        my_nice(pids[i], prio_values[i]);
        print_str("  PROCESS ");
        print_int(pids[i]);
        print_str(" NEW PRIORITY: ");
        print_int(prio_values[i]);
        print_endl();
    }

    for (i = 0; i < TOTAL_PROCESSES; i++)
        my_unblock(pids[i]);

    for (i = 0; i < TOTAL_PROCESSES; i++)
        my_wait(pids[i]);

    print_str("test_prio: DONE");
    print_endl();
    return 0;
}

uint64_t test_sync(uint64_t argc, char **argv) {
    uint64_t pids[4];

    if (argc != 2)
        return 1;

    char *argvDec[] = {argv[0], "-1", argv[1], (char *)0};
    char *argvInc[] = {argv[0], "1", argv[1], (char *)0};

    sync_global = 0;

    uint64_t i;
    for (i = 0; i < 2; i++) {
        pids[i] = my_create_process("my_process_inc", 3, argvDec);
        pids[i + 2] = my_create_process("my_process_inc", 3, argvInc);
    }

    for (i = 0; i < 4; i++)
        my_wait(pids[i]);

    print_str("Final value: ");
    print_int((int64_t)sync_global);
    print_endl();

    return 0;
}