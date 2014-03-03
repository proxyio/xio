#ifndef _ZMALLOC_H_
#define _ZMALLOC_H_

#include <inttypes.h>

typedef struct {
    int64_t alloc;
    int64_t alloc_size;
    int64_t memalign;
    int64_t memalign_size;
    int64_t free;
    int64_t free_size;
} mem_stat_t;



void *mem_alloc(uint32_t size);
void *mem_zalloc(uint32_t size);
void *mem_realloc(void *ptr, uint32_t size);
void mem_free(void *ptr, uint32_t size);
void *mem_align(uint32_t alignment, uint32_t size);
const mem_stat_t *mem_stat();

#endif
