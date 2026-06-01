#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdint.h>

/* ===================== */
/*   Excepciones de CPU   */
/* ===================== */

typedef enum {
    EXC_DIVIDE_ERROR    = 0x00,
    EXC_INVALID_OPCODE  = 0x06,
    EXC_DOUBLE_FAULT    = 0x08,
    EXC_GENERAL_PROTECT = 0x0D,
    EXC_PAGE_FAULT      = 0x0E
} exception_id_t;

/**
 * Despacha la excepción recibida desde el handler ASM.
 */
void exceptionDispatcher(int exception_id, uint64_t *registers);

#endif /* EXCEPTIONS_H */
