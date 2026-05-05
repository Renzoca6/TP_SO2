#include "memory_manager.h"
#include <stddef.h>
#include <stdbool.h>

#define BLOCK_SIZE      16u
#define BITS_PER_WORD   32u

#define BIT_IDX(b)      ((b) / BITS_PER_WORD)
#define BIT_OFF(b)      ((b) % BITS_PER_WORD)
#define BIT_MASK(b)     (1u << BIT_OFF(b))

typedef struct {
    uint32_t nblocks;
} blk_header_t;

static void     *g_pool         = (void *)0;
static uint32_t *g_bitmap       = (uint32_t *)0;
static uint8_t  *g_blocks_base  = (uint8_t *)0;
static size_t    g_total_blocks = 0;
static size_t    g_used_blocks  = 0;

/* ------------------------------------------------------------
 * Bitmap primitives (inline, operate on uint32_t words)
 * ------------------------------------------------------------ */

static inline void bitmap_set(uint32_t *bmp, size_t bit) {
    bmp[BIT_IDX(bit)] |= BIT_MASK(bit);
}

static inline void bitmap_clear(uint32_t *bmp, size_t bit) {
    bmp[BIT_IDX(bit)] &= ~BIT_MASK(bit);
}

static inline bool bitmap_test(const uint32_t *bmp, size_t bit) {
    return (bmp[BIT_IDX(bit)] & BIT_MASK(bit)) != 0;
}

/* ------------------------------------------------------------
 * First-fit: scan bitmap left-to-right for 'needed' consecutive
 * free bits (0 = free). Returns start index or -1.
 * ------------------------------------------------------------ */

static int32_t first_fit(size_t needed) {
    if (needed == 0 || needed > g_total_blocks)
        return -1;

    size_t limit = g_total_blocks - needed;
    for (size_t i = 0; i <= limit; i++) {
        if (bitmap_test(g_bitmap, i))
            continue;

        size_t j;
        for (j = i + 1; j < i + needed; j++) {
            if (bitmap_test(g_bitmap, j))
                break;
        }

        if (j == i + needed)
            return (int32_t)i;
    }

    return -1;
}

/* ------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------ */

void mm_init(void *heap_start, uint64_t heap_size) {
    g_total_blocks = 0;
    g_used_blocks  = 0;
    g_pool         = heap_start;

    if (!heap_start || heap_size < BLOCK_SIZE * 2)
        return;

    size_t hsize = (size_t)heap_size;

    /* Maximum blocks that could theoretically fit */
    size_t max_blocks = hsize / BLOCK_SIZE;

    /* Bitmap dimension: words needed for every conceivable block  */
    size_t bitmap_words = (max_blocks + BITS_PER_WORD - 1) / BITS_PER_WORD;
    size_t bitmap_bytes = bitmap_words * sizeof(uint32_t);

    /* Round bitmap_bytes up to BLOCK_SIZE so g_blocks_base stays aligned */
    if (bitmap_bytes % BLOCK_SIZE != 0)
        bitmap_bytes = (bitmap_bytes + BLOCK_SIZE - 1) & ~(size_t)(BLOCK_SIZE - 1);

    if (bitmap_bytes >= hsize)
        return;

    size_t available   = hsize - bitmap_bytes;
    g_total_blocks     = available / BLOCK_SIZE;

    if (g_total_blocks < 2)
        return;

    g_bitmap      = (uint32_t *)heap_start;
    g_blocks_base = (uint8_t *)heap_start + bitmap_bytes;

    /* Clear the entire bitmap */
    for (size_t i = 0; i < bitmap_words; i++)
        g_bitmap[i] = 0;
}

void *mm_alloc(uint64_t size) {
    if (!g_pool || size == 0 || g_total_blocks == 0)
        return (void *)0;

    /* Blocks needed for the user payload */
    size_t user_blocks = ((size_t)size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    /* +1 for the header block stored right before the returned pointer */
    size_t needed = user_blocks + 1;

    int32_t start = first_fit(needed);
    if (start < 0)
        return (void *)0;

    /* Mark the run as used */
    for (size_t i = 0; i < needed; i++)
        bitmap_set(g_bitmap, (size_t)start + i);

    g_used_blocks += needed;

    /* Write header in the very first block */
    uint8_t       *header_addr = g_blocks_base + (size_t)start * BLOCK_SIZE;
    blk_header_t  *hdr         = (blk_header_t *)header_addr;
    hdr->nblocks = (uint32_t)user_blocks;

    /* User pointer starts *after* the header block */
    return (void *)(header_addr + BLOCK_SIZE);
}

void mm_free(void *ptr) {
    if (!ptr || !g_pool || g_total_blocks == 0)
        return;

    uint8_t *up = (uint8_t *)ptr;

    /* Bounds check */
    if (up < g_blocks_base)
        return;

    size_t offset = (size_t)(up - g_blocks_base);

    if (offset >= g_total_blocks * BLOCK_SIZE)
        return;

    /* Must be block-aligned */
    if (offset % BLOCK_SIZE != 0)
        return;

    size_t block_idx = offset / BLOCK_SIZE;
    if (block_idx == 0)
        return;

    /* Header sits one block before the user pointer */
    blk_header_t *hdr = (blk_header_t *)(up - BLOCK_SIZE);
    uint32_t user_blocks = hdr->nblocks;

    size_t total     = (size_t)user_blocks + 1;
    size_t start_bit = block_idx - 1;

    if (start_bit + total > g_total_blocks)
        return;

    /* Clear the corresponding bits */
    for (size_t i = 0; i < total; i++)
        bitmap_clear(g_bitmap, start_bit + i);

    g_used_blocks -= total;
}

void mm_state(uint64_t *total, uint64_t *used, uint64_t *free_mem) {
    if (!g_pool) {
        *total    = 0;
        *used     = 0;
        *free_mem = 0;
        return;
    }

    *total    = (uint64_t)(g_total_blocks * BLOCK_SIZE);
    *used     = (uint64_t)(g_used_blocks * BLOCK_SIZE);
    *free_mem = *total - *used;
}
