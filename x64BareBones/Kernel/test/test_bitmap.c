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

static int ranges_overlap(void *a, size_t sa, void *b, size_t sb) {
    uint8_t *a0 = (uint8_t *)a, *a1 = a0 + sa;
    uint8_t *b0 = (uint8_t *)b, *b1 = b0 + sb;
    return (a0 < b1) && (b0 < a1);
}

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

static void test_basic(void) {
    mm_init(fake_heap, HEAP_SIZE);

    /*
     * Block size = 16.  Header occupies 1 block.
     * alloc(100) → ceil(100/16) =  7 user blocks →  8 total blocks
     * alloc(200) → ceil(200/16) = 13 user blocks → 14 total blocks
     * alloc(50)  → ceil( 50/16) =  4 user blocks →  5 total blocks
     */
    void *a = mm_alloc(100);
    void *b = mm_alloc(200);
    void *c = mm_alloc(50);

    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);
    assert(a != b && b != c && a != c);
    assert(!ranges_overlap(a, 7 * 16, b, 13 * 16));
    assert(!ranges_overlap(b, 13 * 16, c,  4 * 16));
    assert(!ranges_overlap(a, 7 * 16, c,  4 * 16));

    /* Free b, then alloc same size → first-fit should reuse the same spot */
    mm_free(b);
    void *d = mm_alloc(200);
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

    /* Allocate everything possible (all blocks minus header) */
    size_t max_user = (size_t)total - 16;
    void *big = mm_alloc(max_user);
    assert(big != NULL);

    /* No room left */
    void *extra = mm_alloc(1);
    assert(extra == NULL);

    mm_free(big);
    printf("test_out_of_memory      PASSED\n");
}

static void test_single_block_fill(void) {
    mm_init(fake_heap, HEAP_SIZE);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);

    /* Alloc as much as possible */
    size_t max_user = (size_t)total - 16;
    void *p = mm_alloc(max_user);
    assert(p != NULL);

    mm_state(&total, &used, &free_mem);
    assert(used == total);
    assert(free_mem == 0);

    mm_free(p);
    mm_state(&total, &used, &free_mem);
    assert(used == 0);
    assert(free_mem == total);

    printf("test_single_block_fill  PASSED\n");
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

static void test_first_fit_reuse(void) {
    mm_init(fake_heap, HEAP_SIZE);

    /* Allocate several blocks, free the second, and check first-fit
     * picks that gap for a smaller allocation */
    void *a = mm_alloc(200);  /* 14 blocks */
    void *b = mm_alloc(100);  /*  8 blocks */

    mm_free(a);

    /* Now the first free gap should be exactly where 'a' was */
    void *c = mm_alloc(50);   /* only needs 5 blocks — fits in a's gap */
    assert(c != NULL);
    assert(c == a);           /* first-fit picks the earliest gap */

    mm_free(b);
    mm_free(c);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_first_fit_reuse    PASSED\n");
}

static void test_multiple_frees(void) {
    mm_init(fake_heap, HEAP_SIZE);

    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = mm_alloc(32);
        assert(ptrs[i] != NULL);
    }

    /* Free every other block */
    for (int i = 0; i < 10; i += 2)
        mm_free(ptrs[i]);

    /* Re-allocate through the gaps (first-fit) */
    for (int i = 0; i < 5; i++) {
        void *p = mm_alloc(16);
        assert(p != NULL);
        mm_free(p);
    }

    /* Clean up remaining */
    for (int i = 1; i < 10; i += 2)
        mm_free(ptrs[i]);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(used == 0);

    printf("test_multiple_frees     PASSED\n");
}

static void test_small_pool(void) {
    /*
     * 64-byte pool → max_blocks = 4, bitmap = 1 word (4 B),
     * rounded to 16 B.  available = 48 B → total_blocks = 3.
     * Every allocation costs at least 2 blocks (1 header + 1 user),
     * so only one allocation fits at a time.
     */
    uint8_t tiny[64];
    mm_init(tiny, 64);

    uint64_t total, used, free_mem;
    mm_state(&total, &used, &free_mem);
    assert(total == 48);

    void *p = mm_alloc(16);
    assert(p != NULL);

    /* Second allocation cannot fit (only 1 free block left) */
    void *q = mm_alloc(16);
    assert(q == NULL);

    mm_free(p);

    /* After free, the 2-block gap is reusable */
    q = mm_alloc(16);
    assert(q != NULL);
    assert(q == p);

    mm_free(q);

    printf("test_small_pool         PASSED\n");
}

/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== Bitmapped Memory Manager Tests ===\n");
    test_basic();
    test_null_on_zero();
    test_out_of_memory();
    test_single_block_fill();
    test_alignment();
    test_repeated_alloc_free();
    test_invalid_free();
    test_first_fit_reuse();
    test_multiple_frees();
    test_small_pool();
    printf("\nAll tests PASSED\n");
    return 0;
}
