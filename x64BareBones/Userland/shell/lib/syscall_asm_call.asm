SECTION .text
GLOBAL sys_resize
GLOBAL sys_write
GLOBAL sys_read
GLOBAL sys_clearwindow
GLOBAL sys_date_time
GLOBAL sys_benchmark
GLOBAL sys_write_at_vram
GLOBAL sys_write_at_back
GLOBAL sys_get_screen_info
GLOBAL sys_present_fullframe
GLOBAL sys_print_registers
global sys_getchar
global sys_putPixel
global sys_get_ms_since_boot
global sys_sleep_ms
global sys_kill_system
GLOBAL sys_audio
GLOBAL sys_putframe
GLOBAL sys_mm_alloc
GLOBAL sys_mm_free
GLOBAL sys_mm_state
GLOBAL sys_create_process
GLOBAL sys_exit
GLOBAL sys_getpid
GLOBAL sys_ps
GLOBAL sys_kill
GLOBAL sys_nice
GLOBAL sys_block
GLOBAL sys_unblock
GLOBAL sys_yield
GLOBAL sys_pipe_create
GLOBAL sys_pipe_open
GLOBAL sys_pipe_close
GLOBAL sys_set_fd
GLOBAL sys_sem_open
GLOBAL sys_sem_close
GLOBAL sys_sem_wait
GLOBAL sys_sem_post
GLOBAL sys_sem_value

%macro SYSCALL 1
    push rbp
    mov rbp, rsp

    push rbx

    mov r10, rcx

    mov rbx, rdi
    mov rcx, rsi
    mov rsi, r10
    mov rdi, r8

    mov rax, %1
    int 0x80

    pop rbx

    mov rsp, rbp
    pop rbp
    ret
%endmacro

%macro SYSCALL6 1
    push rbp
    mov rbp, rsp

    push rbx

    mov r10, rcx

    mov rbx, rdi
    mov rcx, rsi
    mov rsi, r10
    mov rdi, r8
    ; R9 already contains arg5, no remapping needed

    mov rax, %1
    int 0x80

    pop rbx

    mov rsp, rbp
    pop rbp
    ret
%endmacro

sys_read:               SYSCALL 0
sys_write:              SYSCALL 1
sys_clearwindow:        SYSCALL 2
sys_date_time:          SYSCALL 3
sys_resize:             SYSCALL 4
sys_benchmark:          SYSCALL 5
sys_write_at_vram:      SYSCALL 6
sys_write_at_back:      SYSCALL 7
sys_present_fullframe:  SYSCALL 8
sys_get_screen_info:    SYSCALL 9
sys_print_registers:    SYSCALL 10
sys_getchar:            SYSCALL 11
sys_putPixel:           SYSCALL 12
sys_get_ms_since_boot:  SYSCALL 13
sys_sleep_ms:           SYSCALL 14
sys_kill_system:        SYSCALL 15
sys_audio:              SYSCALL 16
sys_putframe:           SYSCALL 17
sys_mm_alloc:           SYSCALL 18
sys_mm_free:            SYSCALL 19
sys_mm_state:           SYSCALL 20
sys_create_process:     SYSCALL6 21
sys_exit:               SYSCALL 22
sys_getpid:             SYSCALL 23
sys_ps:                 SYSCALL 24
sys_kill:               SYSCALL 25
sys_nice:               SYSCALL 26
sys_block:              SYSCALL 27
sys_unblock:            SYSCALL 28
sys_yield:              SYSCALL 29
sys_pipe_create:        SYSCALL 30
sys_pipe_open:          SYSCALL 31
sys_pipe_close:         SYSCALL 32
sys_set_fd:             SYSCALL 33
sys_sem_open:           SYSCALL 34
sys_sem_close:          SYSCALL 35
sys_sem_wait:           SYSCALL 36
sys_sem_post:           SYSCALL 37
sys_sem_value:          SYSCALL 38