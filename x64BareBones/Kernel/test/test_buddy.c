#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "../include/memory_manager.h"

#define HEAP_SIZE (4 * 1024 * 1024)   /* 4 MB */
static uint8_t fake_heap[HEAP_SIZE];

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int ranges_overlap(void *a, uint64_t sa, void *b, uint64_t sb) {
    uint8_t *a0 = (uint8_t *)a, *a1 = a0 + sa;
    uint8_t *b0 = (uint8_t *)b, *b1 = b0 + sb;
    return (a0 < b1) && (b0 < a1);
}

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

static void test_basic(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *a = mm_alloc(100);   /* → 128-byte block */
    void *b = mm_alloc(200);   /* → 256-byte block */
    void *c = mm_alloc(50);    /* →  64-byte block */

    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);
    assert(a != b && b != c && a != c);
    assert(!ranges_overlap(a, 128, b, 256));
    assert(!ranges_overlap(b, 256, c,  64));
    assert(!ranges_overlap(a, 128, c,  64));

    mm_free(b);
    void *d = mm_alloc(200);   /* must reuse b's 256-byte block */
    assert(d == b);

    mm_free(a);
    mm_free(c);
    mm_free(d);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);
    assert(free_mem == total);

    printf("test_basic              PASSED\n");
}

static void test_null_on_zero(void) {
    mm_init(fake_heap, HEAP_SIZE);
    void *p = mm_alloc(0);
    assert(p == NULL);
    printf("test_null_on_zero       PASSED\n");
}

static void test_out_of_memory(void) {
    mm_init(fake_heap, HEAP_SIZE);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);

    /* Exhaust the heap with max-size allocations */
    void *big = mm_alloc(total);
    assert(big != NULL);

    void *extra = mm_alloc(1);
    assert(extra == NULL);

    mm_free(big);
    printf("test_out_of_memory      PASSED\n");
}

static void test_full_coalescence(void) {
    mm_init(fake_heap, HEAP_SIZE);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);

    /* Allocate a single block spanning the whole heap */
    void *p = mm_alloc(total);
    assert(p != NULL);

    mm_state(&total, &used, &free_mem);
    assert(used == total);
    assert(free_mem == 0);

    mm_free(p);
    mm_state(&total, &used, &free_mem);
    assert(used == 0);
    assert(free_mem == total);

    printf("test_full_coalescence   PASSED\n");
}

static void test_alignment(void) {
    mm_init(fake_heap, HEAP_SIZE);

    /* Every returned pointer must be at least 16-byte aligned */
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

static void test_repeated_alloc_free(void) {
    mm_init(fake_heap, HEAP_SIZE);

    for (int round = 0; round < 10; round++) {
        void *p1 = mm_alloc(64);
        void *p2 = mm_alloc(128);
        void *p3 = mm_alloc(256);
        assert(p1 && p2 && p3);
        mm_free(p2);
        mm_free(p1);
        mm_free(p3);
    }

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_repeated_alloc_free PASSED\n");
}

static void test_invalid_free(void) {
    mm_init(fake_heap, HEAP_SIZE);
    /* Freeing NULL must not crash */
    mm_free(NULL);
    /* Freeing a pointer outside the heap must not crash */
    uint8_t dummy;
    mm_free(&dummy);
    printf("test_invalid_free       PASSED\n");
}

/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== Buddy System Tests ===\n");
    test_basic();
    test_null_on_zero();
    test_out_of_memory();
    test_full_coalescence();
    test_alignment();
    test_repeated_alloc_free();
    test_invalid_free();
    printf("\nAll tests PASSED\n");
    return 0;
}
