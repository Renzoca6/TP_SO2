#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "memory_manager.h"

// ---------------------------------------------------------------------
// T01 - CONFIGURACIÓN Y AUXILIARES
// ---------------------------------------------------------------------

#define HEAP_SIZE (4 * 1024 * 1024)   /* 4 MB */
static uint8_t fake_heap[HEAP_SIZE];

/* verifica si dos rangos de memoria se solapan */
static int ranges_overlap(void *a, uint64_t sa, void *b, uint64_t sb) {
    uint8_t *a0 = (uint8_t *)a, *a1 = a0 + sa;
    uint8_t *b0 = (uint8_t *)b, *b1 = b0 + sb;
    return (a0 < b1) && (b0 < a1);
}

// ---------------------------------------------------------------------
// T02 - REUTILIZACIÓN DE BLOQUES
// ---------------------------------------------------------------------

/*
 * Verifica que al liberar un bloque y pedir otro del mismo tamaño
 * se reutilice exactamente la misma dirección base.
 * Propio del buddy system: los bloques se devuelven al mismo nivel
 * del árbol y la búsqueda DFS los reencuentra en la misma posición.
 */
static void test_buddy_reuse(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *a = mm_alloc(4096);
    assert(a != NULL);
    mm_free(a);

    void *b = mm_alloc(4096);
    assert(b != NULL);
    assert(b == a);

    mm_free(b);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);
    assert(free_mem == total);

    printf("test_buddy_reuse        PASSED\n");
}

// ---------------------------------------------------------------------
// T03 - COALESCENCIA DE BUDDIES
// ---------------------------------------------------------------------

/*
 * Dos bloques adyacentes (buddies) de 4096 se fusionan al liberarlos.
 * El alloc de 8192 debe usar el bloque fusionado desde la misma base.
 */
static void test_buddy_coalescence(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *x = mm_alloc(4096);
    void *y = mm_alloc(4096);
    assert(x != NULL);
    assert(y != NULL);
    assert(x != y);
    assert(y == (uint8_t *)x + 4096);

    mm_free(x);
    mm_free(y);

    void *z = mm_alloc(8192);
    assert(z != NULL);
    assert(z == x);

    mm_free(z);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_buddy_coalescence   PASSED\n");
}

// ---------------------------------------------------------------------
// T04 - COALESCENCIA PARCIAL
// ---------------------------------------------------------------------

/*
 * Tres bloques de 2048: los dos primeros son buddies y se fusionan
 * al liberarlos. El tercero, que no es buddy de los anteriores,
 * debe permanecer intacto y sin solaparse con el bloque fusionado.
 */
static void test_partial_coalescence(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *a = mm_alloc(2048);
    void *b = mm_alloc(2048);
    void *c = mm_alloc(2048);
    assert(a && b && c);
    assert(a != b && b != c && a != c);
    assert(b == (uint8_t *)a + 2048);

    mm_free(a);
    mm_free(b);

    void *d = mm_alloc(4096);
    assert(d != NULL);
    assert(d == a);

    assert(!ranges_overlap(d, 4096, c, 2048));

    mm_free(c);
    mm_free(d);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_partial_coalescence PASSED\n");
}

// ---------------------------------------------------------------------
// ENTRY POINT
// ---------------------------------------------------------------------

int main(void) {
    printf("=== Buddy-Specific Tests ===\n");
    test_buddy_reuse();
    test_buddy_coalescence();
    test_partial_coalescence();
    printf("\nAll buddy tests PASSED\n");
    return 0;
}
