#include "memory_manager.h"

#define MIN_BLOCK 16u

// ---------------------------------------------------------------------
// F01 - DEFINICIÓN DE TIPOS
// ---------------------------------------------------------------------

typedef enum {
    NODE_FREE  = 0,
    NODE_USED  = 1,
    NODE_SPLIT = 2,
    NODE_FULL  = 3
} node_state_t;

typedef struct {
    void     *heap_base;
    uint64_t  total_size;
    uint32_t  min_block;
    uint32_t  levels;
    uint8_t  *tree;
    uint64_t  used_mem;
} buddy_t;

static buddy_t buddy;

// ---------------------------------------------------------------------
// F02 - UTILIDADES MATEMÁTICAS
// ---------------------------------------------------------------------

/* floor(log2(n)), n debe ser >= 1 */
static uint32_t my_log2(uint64_t n) {
    uint32_t r = 0;
    while (n > 1) { n >>= 1; r++; }
    return r;
}

/* mayor potencia de 2 menor o igual a n */
static uint64_t floor_pow2(uint64_t n) {
    uint64_t p = 1;
    while (p * 2 <= n) p *= 2;
    return p;
}

/* floor(log2(nodo)) = profundidad del nodo en el árbol binario indexado en 1 */
static uint32_t node_level(uint64_t n) {
    uint32_t level = 0;
    while (n > 1) { n >>= 1; level++; }
    return level;
}

// ---------------------------------------------------------------------
// F03 - PROPAGACIÓN DE ESTADO EN EL ÁRBOL
// ---------------------------------------------------------------------

/*
 * Propaga el estado hacia arriba desde el nodo hasta la raíz.
 * Se detiene en cuanto el estado del padre no cambia.
 */
static void update_parent(uint64_t node) {
    while (node > 1) {
        uint64_t parent = node / 2;
        uint8_t  left   = buddy.tree[2 * parent];
        uint8_t  right  = buddy.tree[2 * parent + 1];

        uint8_t new_state;
        if (left == (uint8_t)NODE_FREE && right == (uint8_t)NODE_FREE) {
            new_state = (uint8_t)NODE_FREE;
        } else if ((left == (uint8_t)NODE_USED || left == (uint8_t)NODE_FULL) &&
                   (right == (uint8_t)NODE_USED || right == (uint8_t)NODE_FULL)) {
            new_state = (uint8_t)NODE_FULL;
        } else {
            new_state = (uint8_t)NODE_SPLIT;
        }

        if (buddy.tree[parent] == new_state) break;
        buddy.tree[parent] = new_state;
        node = parent;
    }
}

// ---------------------------------------------------------------------
// F04 - BÚSQUEDA RECURSIVA DE NODO LIBRE
// ---------------------------------------------------------------------

/*
 * DFS recursivo: busca un nodo libre en target_level.
 * Divide nodos FREE al descender; deshace la división si ningún hijo está disponible.
 * Devuelve el índice del nodo, o 0 si falla.
 */
static uint64_t alloc_recursive(uint64_t node, uint32_t level,
                                uint32_t target_level) {
    uint8_t state = buddy.tree[node];

    if (state == (uint8_t)NODE_USED || state == (uint8_t)NODE_FULL)
        return 0;

    if (level == target_level)
        return (state == (uint8_t)NODE_FREE) ? node : 0;

    /* Necesitamos bajar más; dividimos nodos FREE al descender */
    int did_split = 0;
    if (state == (uint8_t)NODE_FREE) {
        buddy.tree[node]          = (uint8_t)NODE_SPLIT;
        buddy.tree[2 * node]      = (uint8_t)NODE_FREE;
        buddy.tree[2 * node + 1]  = (uint8_t)NODE_FREE;
        did_split = 1;
    }

    uint64_t result = alloc_recursive(2 * node,     level + 1, target_level);
    if (result == 0)
        result      = alloc_recursive(2 * node + 1, level + 1, target_level);

    /* Revertimos la división si no se pudo realizar ninguna asignación */
    if (result == 0 && did_split)
        buddy.tree[node] = (uint8_t)NODE_FREE;

    return result;
}

// ---------------------------------------------------------------------
// F05 - INICIALIZACIÓN DEL ADMINISTRADOR DE MEMORIA
// ---------------------------------------------------------------------

void mm_init(void *heap_start, uint64_t heap_size) {
    buddy.total_size = 0;

    if (!heap_start || heap_size < MIN_BLOCK * 4)
        return;

    uint64_t ratio = heap_size / MIN_BLOCK;
    if (ratio < 2) return;

    /* Dimensionamiento preliminar del árbol basado en el heap_size completo */
    uint32_t pre_levels = my_log2(ratio) + 1;
    uint64_t tree_size  = (uint64_t)1 << pre_levels;  /* indexado en 1: necesitamos 2^levels slots */

    if (tree_size >= heap_size) return;

    buddy.tree = (uint8_t *)heap_start;

    uint64_t manageable  = heap_size - tree_size;
    uint64_t total_size  = floor_pow2(manageable);

    if (total_size < MIN_BLOCK) return;

    buddy.heap_base  = (uint8_t *)heap_start + tree_size;
    buddy.total_size = total_size;
    buddy.min_block  = MIN_BLOCK;
    buddy.levels     = my_log2(total_size / MIN_BLOCK) + 1;
    buddy.used_mem   = 0;

    /* Inicializamos sólo los nodos que realmente usamos (1 .. 2^levels - 1) */
    uint64_t nodes = (uint64_t)1 << buddy.levels;
    for (uint64_t i = 0; i < nodes; i++)
        buddy.tree[i] = (uint8_t)NODE_FREE;
}

// ---------------------------------------------------------------------
// F06 - ASIGNACIÓN DE MEMORIA
// ---------------------------------------------------------------------

void *mm_alloc(uint64_t size) {
    if (size == 0 || buddy.total_size == 0) return (void *)0;

    /* Redondeamos al siguiente bloque que sea potencia de 2, mínimo min_block */
    uint64_t block_size = buddy.min_block;
    while (block_size < size) {
        if (block_size >= buddy.total_size) return (void *)0;
        block_size *= 2;
    }
    if (block_size > buddy.total_size) return (void *)0;

    /* Nivel cuyos bloques tienen exactamente block_size bytes */
    uint32_t target_level = my_log2(buddy.total_size / block_size);

    uint64_t node = alloc_recursive(1, 0, target_level);
    if (node == 0) return (void *)0;

    buddy.tree[node] = (uint8_t)NODE_USED;
    buddy.used_mem  += block_size;
    update_parent(node);

    uint32_t level = node_level(node);
    uint64_t pos   = node - ((uint64_t)1 << level);
    return (uint8_t *)buddy.heap_base + pos * (buddy.total_size >> level);
}

// ---------------------------------------------------------------------
// F07 - LIBERACIÓN DE MEMORIA
// ---------------------------------------------------------------------

void mm_free(void *ptr) {
    if (!ptr || buddy.total_size == 0) return;
    if ((uint8_t *)ptr < (uint8_t *)buddy.heap_base) return;

    uint64_t offset = (uint8_t *)ptr - (uint8_t *)buddy.heap_base;
    if (offset >= buddy.total_size) return;

    /* Recorremos desde el nivel hoja hacia arriba para encontrar el nodo USED de ptr */
    uint64_t found = 0;
    for (int32_t lvl = (int32_t)(buddy.levels - 1); lvl >= 0; lvl--) {
        uint64_t bsz  = buddy.total_size >> lvl;
        if (offset % bsz != 0) continue;           /* ptr debe estar alineado a bsz */
        uint64_t node = ((uint64_t)1 << lvl) + offset / bsz;
        if (buddy.tree[node] == (uint8_t)NODE_USED) {
            found = node;
            break;
        }
    }
    if (found == 0) return;

    uint32_t level      = node_level(found);
    uint64_t block_size = buddy.total_size >> level;

    buddy.tree[found]  = (uint8_t)NODE_FREE;
    buddy.used_mem    -= block_size;

    /* Fusionamos con el buddy mientras sea posible */
    uint64_t node = found;
    while (node > 1) {
        uint64_t buddy_node = (node % 2 == 0) ? node + 1 : node - 1;
        if (buddy.tree[buddy_node] != (uint8_t)NODE_FREE) break;
        buddy.tree[node / 2] = (uint8_t)NODE_FREE;
        node = node / 2;
    }

    if (node > 1)
        update_parent(node);
}

// ---------------------------------------------------------------------
// F08 - CONSULTA DE ESTADO DE MEMORIA
// ---------------------------------------------------------------------

void mm_state(uint64_t *total, uint64_t *used, uint64_t *free_mem) {
    *total    = buddy.total_size;
    *used     = buddy.used_mem;
    *free_mem = buddy.total_size - buddy.used_mem;
}