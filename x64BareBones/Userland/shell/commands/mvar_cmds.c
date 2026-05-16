#include "../include/syscall_call.h"
#include "../utils/utils.h"

static int sem_empty, sem_full, sem_mutex;
static volatile char mvar_val;
static int writer_pids[10], reader_pids[10];
static int n_writers, n_readers;

static uint64_t mvar_rand(void) {
    static uint64_t seed = 12345;
    seed = (1103515245 * seed + 12345) & 0x7FFFFFFF;
    return seed;
}

static void mvar_busy_wait(void) {
    uint64_t delay = 8000000 + (mvar_rand() % 12000000);
    for (volatile uint64_t i = 0; i < delay; i++);
}

void mvar_writer_entry(void) {
    int pid = getpid();
    int id = -1;
    for (int i = 0; i < n_writers; i++) {
        if (writer_pids[i] == pid) { id = i; break; }
    }
    if (id < 0) { exit_process(); return; }

    char val = 'A' + (char)id;
    while (1) {
        mvar_busy_wait();
        sem_wait(sem_empty);
        sem_wait(sem_mutex);
        mvar_val = val;
        sem_post(sem_mutex);
        sem_post(sem_full);
    }
}

void mvar_reader_entry(void) {
    int pid = getpid();
    int id = -1;
    for (int i = 0; i < n_readers; i++) {
        if (reader_pids[i] == pid) { id = i; break; }
    }
    if (id < 0) { exit_process(); return; }

    while (1) {
        mvar_busy_wait();
        sem_wait(sem_full);
        sem_wait(sem_mutex);
        char c = mvar_val;
        char buf[2] = { c, '\0' };
        write(buf);
        sem_post(sem_mutex);
        sem_post(sem_empty);
    }
}

void cmd_mvar(int argc, char **argv) {
    if (argc < 3 || !is_numeric(argv[1]) || !is_numeric(argv[2])) {
        println("Usage: mvar <n_writers> <n_readers>");
        return;
    }

    n_writers = string_to_int(argv[1]);
    n_readers = string_to_int(argv[2]);

    if (n_writers < 1 || n_writers > 10 || n_readers < 1 || n_readers > 10) {
        println("Error: values must be between 1 and 10.");
        return;
    }

    sem_empty = sem_open("mvar_empty", 1);
    sem_full  = sem_open("mvar_full", 0);
    sem_mutex = sem_open("mvar_mutex", 1);
    mvar_val  = 0;

    for (int i = 0; i < n_writers; i++) {
        int pid = create_process((void *)mvar_writer_entry, "mvar_w", 2, 0, 0, NULL);
        if (pid > 0) writer_pids[i] = pid;
    }

    for (int i = 0; i < n_readers; i++) {
        int pid = create_process((void *)mvar_reader_entry, "mvar_r", 2, 0, 0, NULL);
        if (pid > 0) reader_pids[i] = pid;
    }
}
