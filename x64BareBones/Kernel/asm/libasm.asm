GLOBAL cpuVendor
GLOBAL semLock
GLOBAL semUnlock

section .text
	
cpuVendor:
	push rbp
	mov rbp, rsp

	push rbx

	mov rax, 0
	cpuid


	mov [rdi], ebx
	mov [rdi + 4], edx
	mov [rdi + 8], ecx

	mov byte [rdi+13], 0

	mov rax, rdi

	pop rbx

	mov rsp, rbp
	pop rbp
	ret

; ---------------------------------------------------------------------
; Spinlock atómico. xchg con memoria tiene prefijo LOCK implícito en x86.
; int semLock(uint8_t *lock)  — gira hasta adquirir; retorna el valor anterior.
semLock:
    mov  al, 1
    xchg al, BYTE [rdi]     ; swap atómico: al ← *lock, *lock ← 1
    cmp  al, 0
    jne  semLock             ; si ya estaba tomado, reintentar
    ret

; void semUnlock(uint8_t *lock)
semUnlock:
    mov BYTE [rdi], 0
    ret
