/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#ifndef _XIO_SLAB_
#define _XIO_SLAB_

#include "alloc.h"

struct __mitem {
    void *next;
};

typedef struct mem_cache {
    int mem_cache_size;
    int free_size;
    void *free_cache_list;
} mem_cache_t;

static inline void mem_cache_init(mem_cache_t *mc, uint32_t sz)
{
    mc->free_size = 0;
    mc->mem_cache_size = sz >
                         sizeof(struct __mitem) ? sz : sizeof(struct __mitem);
}

static inline void mem_cache_destroy(mem_cache_t *mc)
{
    void *free_ptr;

    while (mc->free_cache_list) {
        free_ptr = mc->free_cache_list;
        mc->free_cache_list = ((struct __mitem *)free_ptr)->next;
        mem_free(free_ptr, mc->mem_cache_size);
    }
}

static inline void *mem_cache_alloc(mem_cache_t *mc)
{
    void *free_ptr = NULL;

    if (mc->free_cache_list) {
        mc->free_size--;
        free_ptr = mc->free_cache_list;
        mc->free_cache_list = ((struct __mitem *)free_ptr)->next;
    } else
        free_ptr = mem_zalloc(mc->mem_cache_size);
    return free_ptr;
}

static inline void mem_cache_free(mem_cache_t *mc, void *free_ptr)
{
    mc->free_size++;
    ((struct __mitem *)free_ptr)->next = mc->free_cache_list;
    mc->free_cache_list = free_ptr;
}




#endif
