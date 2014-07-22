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

#ifndef _H_PROXYIO_INT_ARRAY_
#define _H_PROXYIO_INT_ARRAY_

#include <utils/base.h>
#include <utils/alloc.h>

#define UNORDER_INT_ARRAY_DEFAULT_CAP 10

struct unorder_int_array {
	int size;
	int cap;
	int *at;
};

static inline void unorder_int_array_init (struct unorder_int_array *arr)
{
	arr->size = 0;
	arr->cap = UNORDER_INT_ARRAY_DEFAULT_CAP;
	if (!(arr->at = mem_zalloc (sizeof (int) * arr->cap)))
		BUG_ON (1);
}

static inline void unorder_int_array_destroy (struct unorder_int_array *arr)
{
	int i;
	mem_free (arr->at, sizeof (int) * arr->cap);
	ZERO (arr);
}

int unorder_int_array_push_back (struct unorder_int_array *arr, int value);

static inline int unorder_int_array_at (struct unorder_int_array *arr, int idx)
{
	return arr->at[idx];
}

static inline void unorder_int_array_erase (struct unorder_int_array *arr, int idx)
{
	arr->at[idx] = arr->at[--arr->size];
}


#endif
