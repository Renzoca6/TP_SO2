#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

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
// T02 - ASIGNACIÓN Y LIBERACIÓN BÁSICA
// ---------------------------------------------------------------------

/* varios allocs devuelven punteros no nulos, distintos y sin solapamiento */
static void test_alloc_non_null(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *a = mm_alloc(100);
    void *b = mm_alloc(200);
    void *c = mm_alloc(50);

    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);
    assert(a != b && b != c && a != c);
    assert(!ranges_overlap(a, 100, b, 200));
    assert(!ranges_overlap(b, 200, c, 50));
    assert(!ranges_overlap(a, 100, c, 50));

    mm_free(a);
    mm_free(b);
    mm_free(c);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);
    assert(free_mem == total);

    printf("test_alloc_non_null     PASSED\n");
}

/* alloc(0) debe devolver NULL */
static void test_alloc_zero(void) {
    mm_init(fake_heap, HEAP_SIZE);
    assert(mm_alloc(0) == NULL);
    printf("test_alloc_zero         PASSED\n");
}

// ---------------------------------------------------------------------
// T03 - LÍMITES DE MEMORIA
// ---------------------------------------------------------------------

/* agota el heap con un solo bloque grande y verifica que el siguiente falle */
static void test_out_of_memory(void) {
    mm_init(fake_heap, HEAP_SIZE);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);

    void *big = mm_alloc(total);
    assert(big != NULL);
    assert(mm_alloc(1) == NULL);

    mm_free(big);
    printf("test_out_of_memory      PASSED\n");
}

/* ocupa todo el heap, verifica used==total y free==0, luego libera y comprueba */
static void test_full_alloc_free(void) {
    mm_init(fake_heap, HEAP_SIZE);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);

    void *p = mm_alloc(total);
    assert(p != NULL);

    mm_state(&total, &used, &free_mem);
    assert(used == total);
    assert(free_mem == 0);

    mm_free(p);
    mm_state(&total, &used, &free_mem);
    assert(used == 0);
    assert(free_mem == total);

    printf("test_full_alloc_free    PASSED\n");
}

// ---------------------------------------------------------------------
// T04 - ALINEACIÓN
// ---------------------------------------------------------------------

/* todo puntero devuelto debe ser múltiplo de 16 */
static void test_alignment(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *ptrs[32];
    for (int i = 0; i < 32; i++) {
        ptrs[i] = mm_alloc(1);
        assert(ptrs[i] != NULL);
        assert(((uintptr_t)ptrs[i] % 16) == 0);
    }
    for (int i = 0; i < 32; i++)
        mm_free(ptrs[i]);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_alignment          PASSED\n");
}

// ---------------------------------------------------------------------
// T05 - ESTRÉS Y ESTABILIDAD
// ---------------------------------------------------------------------

/* ciclos repetidos de alloc/free para detectar fugas o corrupción */
static void test_stress_alloc_free(void) {
    mm_init(fake_heap, HEAP_SIZE);

    for (int round = 0; round < 10; round++) {
        void *a = mm_alloc(64);
        void *b = mm_alloc(128);
        void *c = mm_alloc(256);
        assert(a && b && c);
        mm_free(a);
        mm_free(b);
        mm_free(c);
    }

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_stress_alloc_free  PASSED\n");
}

/* asigna hasta 256 bloques de 16 bytes, verificando que no se solapen */
static void test_stress_small(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *ptrs[256];
    int count = 0;
    for (int i = 0; i < 256; i++) {
        ptrs[i] = mm_alloc(16);
        if (ptrs[i] == NULL) break;
        for (int j = 0; j < i; j++)
            assert(!ranges_overlap(ptrs[j], 16, ptrs[i], 16));
        count++;
    }

    for (int i = 0; i < count; i++)
        mm_free(ptrs[i]);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_stress_small       PASSED (%d blocks)\n", count);
}

// ---------------------------------------------------------------------
// T06 - PUNTEROS INVÁLIDOS Y DOBLE FREE
// ---------------------------------------------------------------------

/* free(NULL) no debe crashear */
static void test_null_free(void) {
    mm_init(fake_heap, HEAP_SIZE);
    mm_free(NULL);
    printf("test_null_free          PASSED\n");
}

/* free de un puntero fuera del heap no debe crashear */
static void test_invalid_free(void) {
    mm_init(fake_heap, HEAP_SIZE);
    uint8_t dummy;
    mm_free(&dummy);
    printf("test_invalid_free       PASSED\n");
}

/* liberar dos veces el mismo bloque no debe corromper el estado */
static void test_double_free(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *p = mm_alloc(64);
    assert(p != NULL);
    mm_free(p);
    mm_free(p);

    void *q = mm_alloc(64);
    assert(q != NULL);
    mm_free(q);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_double_free        PASSED\n");
}

// ---------------------------------------------------------------------
// T07 - AISLAMIENTO DE BLOQUES
// ---------------------------------------------------------------------

/* un bloque chico sobrevive al free y realloc de uno grande */
static void test_split_isolation(void) {
    mm_init(fake_heap, HEAP_SIZE);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);

    void *small = mm_alloc(16);
    assert(small != NULL);

    void *large = mm_alloc(total / 2);
    assert(large != NULL);

    mm_free(large);

    void *large2 = mm_alloc(total / 2);
    assert(large2 != NULL);
    assert(!ranges_overlap(small, 16, large2, total / 2));

    mm_free(small);
    mm_free(large2);

    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_split_isolation    PASSED\n");
}

/* asigna varios tamaños distintos y verifica que no se solapen */
static void test_multiple_sizes(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *p[6];
    uint64_t sizes[] = {16, 32, 64, 128, 256, 512};

    for (int i = 0; i < 6; i++) {
        p[i] = mm_alloc(sizes[i]);
        assert(p[i] != NULL);
    }

    for (int i = 0; i < 6; i++)
        for (int j = i + 1; j < 6; j++)
            assert(!ranges_overlap(p[i], sizes[i], p[j], sizes[j]));

    for (int i = 0; i < 6; i++)
        mm_free(p[i]);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_multiple_sizes     PASSED\n");
}

// ---------------------------------------------------------------------
// T08 - ORDEN DE LIBERACIÓN
// ---------------------------------------------------------------------

/* liberar en orden inverso no debe impedir la recuperación total */
static void test_out_of_order_free(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *a = mm_alloc(1024);
    void *b = mm_alloc(1024);
    void *c = mm_alloc(1024);
    void *d = mm_alloc(1024);
    assert(a && b && c && d);

    mm_free(d);
    mm_free(c);
    mm_free(b);
    mm_free(a);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);
    assert(free_mem == total);

    printf("test_out_of_order_free  PASSED\n");
}

// ---------------------------------------------------------------------
// ENTRY POINT
// ---------------------------------------------------------------------

int main(void) {
    printf("=== Generic Memory Manager Tests ===\n");
    test_alloc_non_null();
    test_alloc_zero();
    test_out_of_memory();
    test_full_alloc_free();
    test_alignment();
    test_stress_alloc_free();
    test_null_free();
    test_invalid_free();
    test_double_free();
    test_split_isolation();
    test_multiple_sizes();
    test_stress_small();
    test_out_of_order_free();
    printf("\nAll generic tests PASSED\n");
    return 0;
}
