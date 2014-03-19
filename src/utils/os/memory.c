#include <malloc.h>
#include <string.h>
#include "memory.h"

static mem_stat_t mem_stats = {};

void *mem_alloc(uint32_t size) {
    void  *p = malloc(size);

    if (p) {
	mem_stats.alloc++;
	mem_stats.alloc_size += size;
    }
    return p;
}

void *mem_zalloc(uint32_t size) {
    void  *p = mem_alloc(size);

    if (p)
        memset(p, 0, size);
    return p;
}

void *mem_align(uint32_t alignment, uint32_t size) {
    void  *p;

    if ((p = memalign(alignment, size))) {
	mem_stats.alloc++;
	mem_stats.alloc_size += size;
    }
    return p;
}

void mem_free(void *ptr, uint32_t size) {
    if (ptr) {
	mem_stats.alloc--;
	mem_stats.alloc_size -= size;
    }
    free(ptr);
}

void *mem_realloc(void *ptr, uint32_t size) {
    void *newptr = NULL;

    if (!ptr)
	return mem_alloc(size);
    newptr = realloc(ptr, size);
    return newptr;
}


const mem_stat_t *mem_stat() {
    return &mem_stats;
}
