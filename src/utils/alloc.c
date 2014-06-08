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

#include <malloc.h>
#include <string.h>
#include "alloc.h"

static mem_stat_t mem_stats = {};

void *mem_alloc (uint32_t size)
{
	void  *p = malloc (size);

	if (p) {
		mem_stats.alloc++;
		mem_stats.alloc_size += size;
	}
	return p;
}

void *mem_zalloc (uint32_t size)
{
	void  *p = mem_alloc (size);

	if (p)
		memset (p, 0, size);
	return p;
}

void *mem_align (uint32_t alignment, uint32_t size)
{
	void  *p;

	if ( (p = memalign (alignment, size) ) ) {
		mem_stats.alloc++;
		mem_stats.alloc_size += size;
	}
	return p;
}

void mem_free (void *ptr, uint32_t size)
{
	if (ptr) {
		mem_stats.alloc--;
		mem_stats.alloc_size -= size;
	}
	free (ptr);
}

void *mem_realloc (void *ptr, uint32_t size)
{
	void *newptr = NULL;

	if (!ptr)
		return mem_alloc (size);
	newptr = realloc (ptr, size);
	return newptr;
}


const mem_stat_t *mem_stat()
{
	return &mem_stats;
}
