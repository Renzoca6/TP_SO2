#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdint.h>
#include "syscall_call.h"

uint32_t GetUint();
uint32_t GetUniform(uint32_t max);
uint8_t  memcheck(void *start, uint8_t value, uint32_t size);
int64_t  satoi(char *str);
void     bussy_wait(uint64_t n);
void     endless_loop();
void     endless_loop_print(uint64_t wait);

void    *memset(void *dest, int32_t c, uint64_t length);

// ── Aliases para test_processes.c ────────────────────────────────────
// my_create_process: lanza fn como proceso background con prioridad prio.
// Nombre fijo "endless" (visible en ps).
#define my_create_process(fn, prio, argv) \
    create_process((void *)(fn), "endless", (prio), 0, 0, (argv))

// kill/block/unblock devuelven void en el repo; el operador coma les da
// valor 0 para que los if(...==-1) del test compilen sin cambiar firmas.
#define my_kill(pid)    (kill_process((uint64_t)(pid)), 0)
#define my_block(pid)   (block((uint64_t)(pid)), 0)
#define my_unblock(pid) (unblock((uint64_t)(pid)), 0)
#define my_wait(pid)    waitpid((uint64_t)(pid))

#endif
