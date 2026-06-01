#include <stdint.h>
#include "../include/syscall_call.h"
#include "../include/test_util.h"
#include "../utils/utils.h"

#define TOTAL_PROCESSES 3

#define LOWEST  2
#define MEDIUM  1
#define HIGHEST 0

int64_t prio[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};

uint64_t max_value = 0;

void zero_to_max(void) {
  uint64_t value = 0;
  char buf[16];

  while (value++ != max_value);

  write("PROCESS ");
  uintToBase((uint64_t)getpid(), buf, 10);
  write(buf);
  println(" DONE!");
  exit_process();
}

static void await_process(uint64_t pid) {
  ProcessInfo buf[64];
  while (1) {
    int n = ps(buf, 64);
    int found = 0;
    for (int i = 0; i < n; i++) {
      if (buf[i].pid == pid) { found = 1; break; }
    }
    if (!found) return;
    yield();
  }
}

void cmd_test_prio(int argc, char *argv[]) {
  int64_t pids[TOTAL_PROCESSES];
  char *ztm_argv[] = {0};
  uint64_t i;
  char buf[16];

  if (argc != 2)
    return;

  if ((max_value = satoi(argv[1])) <= 0)
    return;

  println("");
  println("+----------------------------------------+");
  println("|         PRIORITY SCHEDULER TEST        |");
  println("|   Iniciando test, puede tardar unos    |");
  println("|   segundos...                          |");
  println("+----------------------------------------+");
  println("");

  println("SAME PRIORITY...");

  for (i = 0; i < TOTAL_PROCESSES; i++)
    pids[i] = create_process((void *)zero_to_max, "zero_to_max", 2, 0, 0, ztm_argv);

  for (i = 0; i < TOTAL_PROCESSES; i++)
    await_process((uint64_t)pids[i]);

  println("SAME PRIORITY, THEN CHANGE IT...");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = create_process((void *)zero_to_max, "zero_to_max", 2, 0, 0, ztm_argv);
    nice((uint64_t)pids[i], (int)prio[i]);
    write("  PROCESS ");
    uintToBase((uint64_t)pids[i], buf, 10);
    write(buf);
    write(" NEW PRIORITY: ");
    uintToBase((uint64_t)prio[i], buf, 10);
    write(buf);
    write("\n");
  }

  for (i = 0; i < TOTAL_PROCESSES; i++)
    await_process((uint64_t)pids[i]);

  println("SAME PRIORITY, THEN CHANGE IT WHILE BLOCKED...");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = create_process((void *)zero_to_max, "zero_to_max", 2, 0, 0, ztm_argv);
    block((uint64_t)pids[i]);
    nice((uint64_t)pids[i], (int)prio[i]);
    write("  PROCESS ");
    uintToBase((uint64_t)pids[i], buf, 10);
    write(buf);
    write(" NEW PRIORITY: ");
    uintToBase((uint64_t)prio[i], buf, 10);
    write(buf);
    write("\n");
  }

  for (i = 0; i < TOTAL_PROCESSES; i++)
    unblock((uint64_t)pids[i]);

  for (i = 0; i < TOTAL_PROCESSES; i++)
    await_process((uint64_t)pids[i]);

  println("");
  println("+----------------------------------------+");
  println("|            TEST FINALIZADO             |");
  println("+----------------------------------------+");
}
