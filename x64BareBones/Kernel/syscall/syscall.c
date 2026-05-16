#include "syscall.h"
#include <stdint.h>
#include "video.h"
#include "keyboard_handler.h"
#include "realTimeClock.h"
#include "benchmark.h"
#include "timer.h"
#include "audio.h"
#include "io.h"
#include "memory_manager.h"
#include "process.h"
#include "scheduler.h"
#include "pipe.h"
#include "sem.h"

extern void enable_interrupts(void);
extern void disable_interrupts(void);

static void syscall_read(uint64_t *registers);
static void syscall_write(uint64_t *registers);
static void syscall_clearwindow(uint64_t *registers);
static void syscall_getDate(uint64_t *registers);
static void syscall_resize(uint64_t *registers);
static void syscall_benchmark(uint64_t *registers);
static void syscall_write_at_VRAM(uint64_t *registers);
static void syscall_write_at_BACK(uint64_t *registers);
static void syscall_present_fullframe(uint64_t *registers);
static void syscall_getScreen_Info(uint64_t *registers);
static void syscall_print_registers(uint64_t *registers);
static void syscall_getchar(uint64_t *registers);
static void syscall_putPixel(uint64_t *registers);
static void syscall_get_ms_since_boot(uint64_t *registers);
static void syscall_sleep_ms(uint64_t *registers);
static void syscall_kill_system(uint64_t *registers);
static void syscall_audio(uint64_t *registers);
static void sycall_put_frame(uint64_t *registers);
static void syscall_mm_alloc(uint64_t *registers);
static void syscall_mm_free(uint64_t *registers);
static void syscall_mm_state(uint64_t *registers);
static void syscall_create_process(uint64_t *registers);
static void syscall_exit(uint64_t *registers);
static void syscall_getpid(uint64_t *registers);
static void syscall_ps(uint64_t *registers);
static void syscall_kill(uint64_t *registers);
static void syscall_nice(uint64_t *registers);
static void syscall_block(uint64_t *registers);
static void syscall_unblock(uint64_t *registers);
static void syscall_yield(uint64_t *registers);
static void syscall_pipe_create(uint64_t *registers);
static void syscall_pipe_open(uint64_t *registers);
static void syscall_pipe_close(uint64_t *registers);
static void syscall_set_fd(uint64_t *registers);
static void syscall_sem_open(uint64_t *registers);
static void syscall_sem_close(uint64_t *registers);
static void syscall_sem_wait(uint64_t *registers);
static void syscall_sem_post(uint64_t *registers);
static void syscall_sem_value(uint64_t *registers);

typedef void (*SysCallHandler)(uint64_t *);

static SysCallHandler sysCallHandlers[MAX_SYSCALLS] = {
    syscall_read,               // 0: SYS_READ
    syscall_write,              // 1: SYS_WRITE
    syscall_clearwindow,        // 2: SYS_CLEAR_WINDOW
    syscall_getDate,            // 3: SYS_GET_DATE
    syscall_resize,             // 4: SYS_RESIZE
    syscall_benchmark,          // 5: SYS_BENCHMARK
    syscall_write_at_VRAM,      // 6: SYS_WRITE_AT_VRAM
    syscall_write_at_BACK,      // 7: SYS_WRITE_AT_BACK
    syscall_present_fullframe,  // 8: SYS_PRESENT_FULLFRAME
    syscall_getScreen_Info,     // 9: SYS_GET_SCREEN_INFO
    syscall_print_registers,    // 10: SYS_PRINT_REGISTERS
    syscall_getchar,            // 11: SYS_GETCHAR
    syscall_putPixel,           // 12: SYS_PUT_PIXEL
    syscall_get_ms_since_boot,  // 13: SYS_GET_MS_SINCE_BOOT
    syscall_sleep_ms,           // 14: SYS_SLEEP_MS
    syscall_kill_system,        // 15: SYS_KILL_SYSTEM
    syscall_audio,              // 16: SYS_AUDIO
    sycall_put_frame,           // 17: SYS_PUT_FRAME
    syscall_mm_alloc,           // 18: SYS_MM_ALLOC
    syscall_mm_free,            // 19: SYS_MM_FREE
    syscall_mm_state,           // 20: SYS_MM_STATE
    syscall_create_process,     // 21: SYS_CREATE_PROCESS
    syscall_exit,               // 22: SYS_EXIT
    syscall_getpid,             // 23: SYS_GETPID
    syscall_ps,                 // 24: SYS_PS
    syscall_kill,               // 25: SYS_KILL
    syscall_nice,               // 26: SYS_NICE
    syscall_block,              // 27: SYS_BLOCK
    syscall_unblock,            // 28: SYS_UNBLOCK
    syscall_yield,              // 29: SYS_YIELD
    syscall_pipe_create,        // 30: SYS_PIPE_CREATE
    syscall_pipe_open,          // 31: SYS_PIPE_OPEN
    syscall_pipe_close,         // 32: SYS_PIPE_CLOSE
    syscall_set_fd,             // 33: SYS_SET_FD
    syscall_sem_open,           // 34: SYS_SEM_OPEN
    syscall_sem_close,          // 35: SYS_SEM_CLOSE
    syscall_sem_wait,           // 36: SYS_SEM_WAIT
    syscall_sem_post,           // 37: SYS_SEM_POST
    syscall_sem_value,          // 38: SYS_SEM_VALUE
};

void syscall_handler(uint64_t rax, uint64_t *registers) {
    if (rax < MAX_SYSCALLS) {
        sysCallHandlers[rax](registers);
    } else {
        registers[14] = -1;
    }
}

static void sycall_put_frame(uint64_t *registers) {
    putFrame();
}

static void syscall_audio(uint64_t *registers) {
    uint64_t op   = registers[13];
    uint32_t freq = (uint32_t)registers[12];
    uint32_t dur  = (uint32_t)registers[11];

    switch (op) {
        case 0:
            if (freq == 0) {
                registers[14] = (uint64_t)-1;
                return;
            }
            play_sound(freq);
            registers[14] = 0;
            break;
        case 1:
            stop_sound();
            registers[14] = 0;
            break;
        case 2:
            beep(freq, dur);
            registers[14] = 0;
            break;
        default:
            registers[14] = (uint64_t)-1;
            break;
    }
}

static void syscall_write_at(uint64_t *registers, PixelTarget target) {
    const char *str   = (const char *)registers[13];
    int col           = (int)registers[12];
    int fil           = (int)registers[11];
    uint32_t fColor   = (uint32_t)registers[8];
    uint32_t bgColor  = (uint32_t)registers[9];

    vdPrintStyled_AT(str, col, fil, fColor, bgColor, target);
}

static void syscall_sleep_ms(uint64_t *registers) {
    uint64_t ms = registers[13];
    sleep_ms(ms);
}

static void syscall_get_ms_since_boot(uint64_t *registers) {
    registers[14] = timer_ms_since_boot();
}

static void syscall_putPixel(uint64_t *registers) {
    PixelTarget target;

    if (registers[8] == 0) {
        target = PIXEL_VRAM;
    } else {
        target = PIXEL_BACK;
    }

    putPixel(registers[13], registers[12], registers[11], target);
}

static void syscall_write_at_VRAM(uint64_t *registers) {
    syscall_write_at(registers, PIXEL_VRAM);
}

static void syscall_write_at_BACK(uint64_t *registers) {
    syscall_write_at(registers, PIXEL_BACK);
}

static void syscall_getScreen_Info(uint64_t *registers) {
    uint64_t which = registers[13];

    if (which == 0) {
        registers[14] = vdGetHeight();
    } else {
        registers[14] = vdGetWidth();
    }
}

static void syscall_present_fullframe(uint64_t *registers) {
    present_fullframe();
}

static void syscall_write(uint64_t *registers) {
    const char *str = (const char *)registers[12];

    // Si el proceso tiene stdout redirigido a un pipe, escribir en él.
    PCB *cur = get_current_process();
    if (cur && cur->fd[1] >= 0) {
        if (!str) { registers[14] = 0; return; }
        uint32_t len = 0;
        while (str[len]) len++;
        registers[14] = (uint64_t)pipe_write(cur->fd[1], str, len);
        return;
    }

    if (registers[13] == 1) {
        vdPrint(str, PIXEL_VRAM);
    } else {
        vdPrintStyled(str, 0x00FFFFFF, 0x00FF0000, PIXEL_VRAM);
    }
}

static void syscall_benchmark(uint64_t *registers) {
    enable_interrupts();

    uint64_t which = registers[13];
    uint64_t res = 0;

    switch (which) {
        case 0: res = benchmark_fps();             break;
        case 1: res = benchmark_floating_point();  break;
        case 2: res = benchmark_hardware_access(); break;
        case 3: res = benchmark_timer_latency();   break;
        default: res = (uint64_t)-1;               break;
    }

    registers[14] = res;
    disable_interrupts();
}

static void syscall_resize(uint64_t *registers) {
    int s = str_to_uint_ignore_sign((char *)registers[13]);

    if (s < 1) s = 1;
    if (s > 4) s = 4;

    vdSetFontScale(s);
}

static void syscall_getDate(uint64_t *registers) {
    if (registers[13] == 1) {
        vdPrint(getDateString(), PIXEL_VRAM);
        return;
    }
    vdPrint(getTimeString(), PIXEL_VRAM);
}

static void syscall_clearwindow(uint64_t *registers) {
    vdclearScreenDB(registers[13]);
}

static void syscall_read(uint64_t *registers) {
    char *buf = (char *)registers[13];

    // Si el proceso tiene stdin redirigido a un pipe, leer de él.
    PCB *cur = get_current_process();
    if (cur && cur->fd[0] >= 0) {
        int pipe_id = cur->fd[0];
        int size = 0;

        // Lectura línea a línea desde el pipe (hasta '\n' o EOF o 255 chars).
        while (size < 255) {
            char c;
            int n = pipe_read(pipe_id, &c, 1);
            if (n == 0) {                   // EOF
                buf[size] = '\0';
                registers[14] = (uint64_t)size;
                return;
            }
            if (n < 0) {
                buf[0] = '\0';
                registers[14] = 0;
                return;
            }
            if (c == '\n') {
                buf[size] = '\0';
                registers[14] = (uint64_t)size;
                return;
            }
            buf[size++] = c;
        }
        buf[size] = '\0';
        registers[14] = (uint64_t)size;
        return;
    }

    // Lectura desde teclado (comportamiento original + Ctrl+C / Ctrl+D).
    int size = 0;
    clearKeyboardBuffer();
    enable_interrupts();

    while (1) {
        if (hasNextKey()) {
            KeyBufferStruct k = getNextKey();
            if (k.is_pressed) {
                if (k.key == '\n') {
                    vdPrintChar('\n', PIXEL_VRAM);
                    buf[size] = '\0';
                    registers[14] = (uint64_t)size;
                    disable_interrupts();
                    return;
                } else if (k.key == '\b') {
                    if (size > 0) {
                        size--;
                        buf[size] = '\0';
                        vdBackSpace(PIXEL_VRAM);
                    }
                } else if (k.key == 4) {    // Ctrl+D (EOF)
                    buf[size] = '\0';
                    registers[14] = 0;
                    disable_interrupts();
                    return;
                } else if (k.key) {
                    if (size + 1 < 256) {
                        buf[size++] = k.key;
                        vdPrintChar(k.key, PIXEL_VRAM);
                    } else {
                        vdPrintChar('\n', PIXEL_VRAM);
                        buf[size] = '\0';
                        registers[14] = (uint64_t)size;
                        disable_interrupts();
                        return;
                    }
                }
            }
        }
    }
}

static void syscall_getchar(uint64_t *registers) {
    char *user_buf   = (char *)registers[13];
    uint64_t max_len = registers[12];
    const uint64_t timeout_ms = 10;

    enable_interrupts();

    uint64_t last_event_time = timer_ms_since_boot();
    uint64_t written = 0;

    while (1) {
        if (hasNextKey()) {
            KeyBufferStruct k = getNextKey();
            if (k.is_pressed) {
                if (written < max_len) {
                    char ch = (char)k.key;

                    if (ch >= 'A' && ch <= 'Z') {
                        ch = ch + ('a' - 'A');
                    }

                    user_buf[written++] = ch;
                }

                last_event_time = timer_ms_since_boot();
                if (written >= max_len) break;
            }
        }

        if (timer_ms_since_boot() - last_event_time > timeout_ms) {
            break;
        }
    }

    if (written < max_len)
        user_buf[written] = '\0';

    registers[14] = written;
    disable_interrupts();
}

static void syscall_print_registers(uint64_t *registers) {
    uint64_t *user_buffer = (uint64_t *)registers[13];
    
    if (areRegsSaved()) {
        uint64_t *saved = getSavedRegs();
        for (int i = 0; i < 20; i++) {
            user_buffer[i] = saved[i];
        }
        registers[14] = 0;
    } else {
        registers[14] = -1;
    }
}

static void syscall_kill_system(uint64_t *registers) {
    outb(0xF4, 0x00);
}

static void syscall_mm_alloc(uint64_t *registers) {
    uint64_t size = registers[13];
    registers[14] = (uint64_t)mm_alloc(size);
}

static void syscall_mm_free(uint64_t *registers) {
    mm_free((void *)registers[13]);
    registers[14] = 0;
}

static void syscall_mm_state(uint64_t *registers) {
    uint64_t *buf = (uint64_t *)registers[13];
    if (buf) {
        mm_state(&buf[0], &buf[1], &buf[2]);
        registers[14] = 0;
    } else {
        registers[14] = (uint64_t)-1;
    }
}

static void syscall_create_process(uint64_t *registers) {
    void *entry_point = (void *)registers[13];     // RBX
    const char *name   = (const char *)registers[12]; // RCX
    int priority       = (int)registers[11];         // RDX
    int foreground     = (int)registers[8];          // RSI
    int argc           = (int)registers[9];           // RDI
    char **argv        = (char **)registers[6];        // R9

    int pid = create_process(entry_point, name, priority, foreground, argc, argv);
    registers[14] = (uint64_t)pid;
}

static void syscall_exit(uint64_t *registers) {
    exit_current_process();
    yield_process();
    registers[14] = 0;
}

static void syscall_getpid(uint64_t *registers) {
    registers[14] = (uint64_t)get_current_pid();
}

static void syscall_ps(uint64_t *registers) {
    ProcessInfo *buf = (ProcessInfo *)registers[13]; // RBX
    int max_count     = (int)registers[12];           // RCX

    int count = get_process_list(buf, max_count);
    registers[14] = (uint64_t)count;
}

static void syscall_kill(uint64_t *registers) {
    uint64_t pid = registers[13]; // RBX
    kill_process(pid);
    registers[14] = 0;
}

static void syscall_nice(uint64_t *registers) {
    uint64_t pid          = registers[13]; // RBX
    int new_priority      = (int)registers[12]; // RCX

    int result = set_process_priority(pid, new_priority);
    registers[14] = (uint64_t)result;
}

static void syscall_block(uint64_t *registers) {
    uint64_t pid = registers[13]; // RBX

    if (pid == 0) {
        int cur = get_current_pid();
        if (cur > 0) pid = (uint64_t)cur;
    }

    PCB *pcb = get_process_by_pid(pid);
    if (pcb && pcb->state == RUNNING) {
        pcb->state = BLOCKED;
        yield_process();
    } else if (pcb && pcb->state == READY) {
        pcb->state = BLOCKED;
    }
    registers[14] = 0;
}

static void syscall_unblock(uint64_t *registers) {
    uint64_t pid = registers[13]; // RBX
    unblock_process(pid);
    registers[14] = 0;
}

static void syscall_yield(uint64_t *registers) {
    yield_process();
    registers[14] = 0;
}

static void syscall_pipe_create(uint64_t *registers) {
    registers[14] = (uint64_t)pipe_create();
}

static void syscall_pipe_open(uint64_t *registers) {
    const char *name = (const char *)registers[13]; // RBX
    registers[14] = (uint64_t)pipe_open(name);
}

static void syscall_pipe_close(uint64_t *registers) {
    int pipe_id = (int)registers[13];   // RBX
    int side    = (int)registers[12];   // RCX
    registers[14] = (uint64_t)pipe_close(pipe_id, side);
}

static void syscall_sem_open(uint64_t *registers) {
    const char *name  = (const char *)registers[13]; // RBX
    uint16_t    value = (uint16_t)registers[12];      // RCX
    registers[14] = (uint64_t)sem_open(name, value);
}

static void syscall_sem_close(uint64_t *registers) {
    int sem_id = (int)registers[13];                  // RBX
    registers[14] = (uint64_t)sem_close(sem_id);
}

static void syscall_sem_wait(uint64_t *registers) {
    int sem_id = (int)registers[13];                  // RBX
    registers[14] = (uint64_t)sem_wait(sem_id);
}

static void syscall_sem_post(uint64_t *registers) {
    int sem_id = (int)registers[13];                  // RBX
    registers[14] = (uint64_t)sem_post(sem_id);
}

static void syscall_sem_value(uint64_t *registers) {
    int sem_id = (int)registers[13];                  // RBX
    registers[14] = (uint64_t)sem_get_value(sem_id);
}

static void syscall_set_fd(uint64_t *registers) {
    int fd_index = (int)registers[13];  // RBX: 0=stdin, 1=stdout
    int pipe_id  = (int)registers[12];  // RCX: -1=terminal, >=0 pipe

    if (fd_index < 0 || fd_index > 1) {
        registers[14] = (uint64_t)-1;
        return;
    }

    PCB *cur = get_current_process();
    if (!cur) {
        registers[14] = (uint64_t)-1;
        return;
    }

    cur->fd[fd_index] = pipe_id;
    registers[14] = 0;
}