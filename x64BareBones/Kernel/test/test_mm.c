#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#include "memory_manager.h"

// ---------------------------------------------------------------------
// T01 - CONFIGURACIÓN Y AUXILIARES
// ---------------------------------------------------------------------

#define MIN_BLOCK  16u        /* tamaño mínimo de bloque del buddy system */
#define MAX_ALLOCS 512        /* máxima cantidad de bloques vivos simultáneos */

typedef struct {
    void    *ptr;
    uint64_t size;
} alloc_t;

/* variables globales para el resumen al recibir SIGINT */
static volatile sig_atomic_t stop = 0;

static uint64_t iterations   = 0;
static uint64_t allocs_done  = 0;
static uint64_t frees_done   = 0;
static uint64_t alloc_failed = 0;

/* handler de Ctrl+C: activa la bandera de salida */
static void on_sigint(int sig) {
    (void)sig;
    stop = 1;
}

/* redondea el tamaño pedido a la siguiente potencia de 2 (mínimo MIN_BLOCK) */
static uint64_t round_up_size(uint64_t size) {
    uint64_t bs = MIN_BLOCK;
    while (bs < size) bs *= 2;
    return bs;
}

/* verifica si dos rangos de memoria se solapan */
static int ranges_overlap(void *a, uint64_t sa, void *b, uint64_t sb) {
    uint8_t *a0 = (uint8_t *)a, *a1 = a0 + sa;
    uint8_t *b0 = (uint8_t *)b, *b1 = b0 + sb;
    return (a0 < b1) && (b0 < a1);
}

// ---------------------------------------------------------------------
// T02 - FUZZ TEST PRINCIPAL
// ---------------------------------------------------------------------

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <max_memory_bytes>\n", argv[0]);
        return 1;
    }

    uint64_t heap_size = (uint64_t)atoll(argv[1]);
    if (heap_size < 64) {
        fprintf(stderr, "Heap size too small (min 64)\n");
        return 1;
    }

    uint8_t *heap = (uint8_t *)malloc(heap_size);
    if (!heap) {
        fprintf(stderr, "Could not allocate heap\n");
        return 1;
    }

    signal(SIGINT, on_sigint);

    mm_init(heap, heap_size);
    srand((unsigned)time(NULL));

    alloc_t allocs[MAX_ALLOCS];
    int count = 0;

    while (!stop) {
        iterations++;

        uint64_t total, used, free_mem;
        mm_state(&total, &used, &free_mem);

        /*
         * Con probabilidad 2/3 intenta alloc, 1/3 intenta free.
         * Si no hay bloques vivos, fuerza alloc.
         */
        if (count == 0 || (count < MAX_ALLOCS && rand() % 3 != 0)) {

            /* si el heap está lleno, libera uno para seguir */
            if (free_mem == 0) {
                int idx = rand() % count;
                mm_free(allocs[idx].ptr);
                allocs[idx] = allocs[count - 1];
                count--;
                frees_done++;
                continue;
            }

            /* tamaño aleatorio acotado a 16 KB */
            uint64_t max_req = free_mem < 16384 ? free_mem : 16384;
            uint64_t req = (uint64_t)(rand() % max_req) + 1;

            void *ptr = mm_alloc(req);
            if (!ptr) {
                /* falla esperable por fragmentación externa, no es error */
                alloc_failed++;
                continue;
            }

            uint64_t blk = round_up_size(req);

            /* verifica que el nuevo bloque no solape ningún bloque vivo */
            for (int i = 0; i < count; i++) {
                if (ranges_overlap(ptr, blk, allocs[i].ptr, allocs[i].size)) {
                    fprintf(stderr,
                            "ERROR iter=%lu: overlap  new=[%p,+%lu)  old=[%p,+%lu)\n",
                            (unsigned long)iterations,
                            ptr, (unsigned long)blk,
                            allocs[i].ptr, (unsigned long)allocs[i].size);
                    free(heap);
                    return 1;
                }
            }

            allocs[count].ptr  = ptr;
            allocs[count].size = blk;
            count++;
            allocs_done++;
        } else {
            /* libera un bloque al azar */
            int idx = rand() % count;
            mm_free(allocs[idx].ptr);
            allocs[idx] = allocs[count - 1];
            count--;
            frees_done++;
        }
    }

    printf("\n--- test_mm summary ---\n");
    printf("iterations:   %lu\n", (unsigned long)iterations);
    printf("allocs:       %lu\n", (unsigned long)allocs_done);
    printf("frees:        %lu\n", (unsigned long)frees_done);
    printf("alloc_failed: %lu (debido a fragmentacion externa)\n",
           (unsigned long)alloc_failed);
    printf("live_blocks:  %d\n", count);
    printf("overlaps:     0\n");
    printf("status:       OK\n");

    free(heap);
    return 0;
}
