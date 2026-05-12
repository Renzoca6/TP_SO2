#ifndef SYSCALL_CALL_H
#define SYSCALL_CALL_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t pid;
    char     name[32];
    uint64_t rsp;
    uint64_t rbp;
    int      priority;
    int      state;
    int      foreground;
} ProcessInfo;

void do_resize(char *N_Times);
int write(const char *buf);
int println(const char *buf);
int read(char *buf);
int printError(const char *buf);
void clearwindow(uint32_t *color);

void get_time(void);
void get_date(void);

void printRegisters(void);

uint64_t do_benchmark_fps(void);
uint64_t do_benchmark_floating_point(void);
uint64_t do_benchmark_hardware_access(void);
uint64_t do_benchmark_timer_latency(void);

void write_at_back(const char *str, int col, int fil, uint32_t fColor, uint32_t bgColor);
void write_at_vram(const char *str, int col, int fil, uint32_t fColor, uint32_t bgColor);
void get_screen_info(uint64_t *height, uint64_t *width);
void present_fullframe(void);

uint64_t get_screen_height(void);
uint64_t get_screen_width(void);

char getchar(void);
void sleep_ms(uint64_t ms);
uint64_t get_ms_since_boot(void);

void put_frame(void);
void putPixel(uint32_t color, uint32_t x, uint32_t y, int target);

int get_multiple_chars_sys(char *buf, uint64_t max_len);

void audio_play(uint32_t freq_hz);
void audio_stop(void);
void audio_beep(uint32_t freq_hz, uint32_t duration_ms);

void *malloc(uint64_t size);
void free(void *ptr);
void mem_state(uint64_t *total, uint64_t *used, uint64_t *free_mem);

int create_process(void *entry, const char *name, int priority, int foreground, int argc, char **argv);
void exit_process(void);
int getpid(void);
int ps(ProcessInfo *buf, int max_count);
void kill_process(uint64_t pid);
int nice(uint64_t pid, int new_priority);
void block(uint64_t pid);
void unblock(uint64_t pid);
void yield(void);

#endif