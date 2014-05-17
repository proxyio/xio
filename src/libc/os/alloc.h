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

#ifndef _XIO_ZMALLOC_
#define _XIO_ZMALLOC_

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
