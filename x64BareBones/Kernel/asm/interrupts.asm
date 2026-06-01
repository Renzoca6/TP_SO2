default rel

GLOBAL picMasterMask
GLOBAL picSlaveMask
GLOBAL haltcpu
GLOBAL _hlt

GLOBAL _irq00Handler
GLOBAL _irq01Handler
GLOBAL _irq02Handler
GLOBAL _irq03Handler
GLOBAL _irq04Handler
GLOBAL _irq05Handler
GLOBAL _irq06Handler
GLOBAL _exception0Handler
GLOBAL _exception6Handler
GLOBAL _exception8Handler
GLOBAL _exception13Handler
GLOBAL _exception14Handler


EXTERN syscall_handler
EXTERN irqDispatcher
EXTERN exceptionDispatcher
EXTERN schedule


SECTION .text



%macro pushState 0
	push rax
	push rbx
	push rcx
	push rdx
	push rbp
	push rdi
	push rsi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
%endmacro

%macro popState 0
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rsi
	pop rdi
	pop rbp
	pop rdx
	pop rcx
	pop rbx
	pop rax
%endmacro

%macro irqHandlerMaster 1
	pushState

	mov rdi, %1 ; pasaje de parametro
	mov rsi, rsp ; pasaje de puntero a registros en el stack (use LEA to get address)
	call irqDispatcher

	; signal pic EOI (End of Interrupt)
	mov al, 20h
	out 20h, al

	popState
	iretq
%endmacro



; exceptionHandler macro
; args:
;   1 = id exception
;   2 = skip bytes 

%macro exceptionHandler 1
	pushState

	mov rdi, %1           ; primer argumento: exception ID
	mov rsi, rsp          ; segundo argumento: puntero al stack (frame de registros)

	call exceptionDispatcher

	popState
	iretq
%endmacro


; exceptionHandlerErr: para excepciones con ERROR CODE (#DF, #GP, #PF).
; El CPU pushea el error code antes del frame; lo descartamos para que el
; iretq quede igual que en las excepciones sin error code.
; args: 1 = id exception
%macro exceptionHandlerErr 1
	add rsp, 8            ; descartar el error code
	pushState

	mov rdi, %1
	mov rsi, rsp
	call exceptionDispatcher

	popState
	iretq
%endmacro


_hlt:
	sti
	hlt
	ret

; Timer 8254 (tick del timer)
; Handler personalizado: después del dispatch normal de IRQ corre el scheduler
; y posiblemente cambia al stack de otro proceso.
_irq00Handler:
	pushState

	mov rdi, 0
	mov rsi, rsp
	call irqDispatcher

	; Correr scheduler — pasar RSP actual (después de pushState)
	mov rdi, rsp
	call schedule
	test rax, rax
	jz .no_switch
	mov rsp, rax          ; cambiar al stack del siguiente proceso
.no_switch:

	; signal EOI al PIC
	mov al, 20h
	out 20h, al

	popState
	iretq

;Keyboard
_irq01Handler:
	irqHandlerMaster 1

;Cascade pic never called
_irq02Handler:
	irqHandlerMaster 2

;Serial Port 2 and 4
_irq03Handler:
	irqHandlerMaster 3

;Serial Port 1 and 3
_irq04Handler:
	irqHandlerMaster 4

;USB
_irq05Handler:
	irqHandlerMaster 5


;Zero Division Exception
_exception0Handler:
	exceptionHandler 0

;OP Exception (Invalid Opcode)
_exception6Handler:
	exceptionHandler 6

;Double Fault (#8) — pushea error code (siempre 0)
_exception8Handler:
	exceptionHandlerErr 8

;General Protection Fault (#13) — pushea error code
_exception13Handler:
	exceptionHandlerErr 13

;Page Fault (#14) — pushea error code
_exception14Handler:
	exceptionHandlerErr 14



; syscall 
_irq06Handler:
    pushState  ;preservo los registros
   
    mov rdi, [rsp + 14*8]   ; numero de syscall(recibido por userland)
    mov rsi, rsp         ; 2nd arg: puntero a registros 
    call syscall_handler

    popState  ;les hago pop
    iretq
	 
haltcpu:
	cli
	hlt
	ret



SECTION .bss
	aux resq 1

SECTION .data
	registers:
	dq 0
	dq 0
	dq 0
	dq 0
	dq 0
	dq 0
