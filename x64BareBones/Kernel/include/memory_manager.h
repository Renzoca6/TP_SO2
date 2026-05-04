#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stdint.h>

void  mm_init(void *heap_start, uint64_t heap_size);
void *mm_alloc(uint64_t size);
void  mm_free(void *ptr);
void  mm_state(uint64_t *total, uint64_t *used, uint64_t *free_mem);

#endif
