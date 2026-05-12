#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_READ              0
#define SYS_WRITE             1
#define SYS_CLEAR_WINDOW      2
#define SYS_GET_DATE          3
#define SYS_RESIZE            4
#define SYS_BENCHMARK         5
#define SYS_WRITE_AT_VRAM     6
#define SYS_WRITE_AT_BACK     7
#define SYS_PRESENT_FULLFRAME 8
#define SYS_GET_SCREEN_INFO   9
#define SYS_PRINT_REGISTERS   10
#define SYS_GETCHAR           11
#define SYS_PUT_PIXEL         12
#define SYS_GET_MS_SINCE_BOOT 13
#define SYS_SLEEP_MS          14
#define SYS_KILL_SYSTEM       15
#define SYS_AUDIO             16
#define SYS_PUT_FRAME         17
#define SYS_MM_ALLOC          18
#define SYS_MM_FREE           19
#define SYS_MM_STATE          20

#define MAX_SYSCALLS          21

void syscall_handler(uint64_t rax, uint64_t *registers);

#endif
