#ifndef _HPIO_SLAB_
#define _HPIO_SLAB_

#include "alloc.h"

struct __mitem {
    void *next;
};

typedef struct mem_cache {
    int mem_cache_size;
    int free_size;
    void *free_cache_list;
} mem_cache_t;

static inline void mem_cache_init(mem_cache_t *mc, uint32_t sz) {
    mc->free_size = 0;
    mc->mem_cache_size = sz >
	sizeof(struct __mitem) ? sz : sizeof(struct __mitem);
}

static inline void mem_cache_destroy(mem_cache_t *mc) {
    void *free_ptr;

    while (mc->free_cache_list) {
	free_ptr = mc->free_cache_list;
	mc->free_cache_list = ((struct __mitem *)free_ptr)->next;
	mem_free(free_ptr, mc->mem_cache_size);
    }
}

static inline void *mem_cache_alloc(mem_cache_t *mc) {
    void *free_ptr = NULL;

    if (mc->free_cache_list) {
	mc->free_size--;
	free_ptr = mc->free_cache_list;
	mc->free_cache_list = ((struct __mitem *)free_ptr)->next;
    } else
	free_ptr = mem_zalloc(mc->mem_cache_size);
    return free_ptr;
}

static inline void mem_cache_free(mem_cache_t *mc, void *free_ptr) {
    mc->free_size++;
    ((struct __mitem *)free_ptr)->next = mc->free_cache_list;
    mc->free_cache_list = free_ptr;
}




#endif
