/*
 * freelist_mm.c — archivo stub de marcador de posición.
 * Reemplazar con una implementación real de freelist first-fit cuando sea necesario.
 */
#include "memory_manager.h"

/* Inicializa el administrador de memoria con el inicio y tamaño del heap indicados.
 * Por ahora no hace nada; los parámetros se suprimen para evitar advertencias del compilador. */
void mm_init(void *heap_start, uint64_t heap_size) {
    (void)heap_start;   /* aún sin usar — stub */
    (void)heap_size;    /* aún sin usar — stub */
}

/* Reserva 'size' bytes del heap y devuelve un puntero al bloque asignado. */
void *mm_alloc(uint64_t size) {
    (void)size;         /* aún sin usar — stub */
    return (void *)0;   /* sin implementación real: devuelve NULL */
}

/* Libera el bloque de memoria apuntado por 'ptr'.*/
void mm_free(void *ptr) {
    (void)ptr;          /* aún sin usar — stub */
}

/* Devuelve el estado actual del heap: total, usado y libre. */
void mm_state(uint64_t *total, uint64_t *used, uint64_t *free_mem) {
    *total    = 0;      /* tamaño total del heap        */
    *used     = 0;      /* bytes actualmente asignados  */
    *free_mem = 0;      /* bytes disponibles            */
}