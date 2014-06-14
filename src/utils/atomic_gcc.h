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

#ifndef _H_PROXYIO_ATOMIC_GCC_
#define _H_PROXYIO_ATOMIC_GCC_

#include <inttypes.h>
#include "base.h"
#include "spinlock.h"

typedef struct {
	i64 ref;
} atomic_t;

static inline void atomic_init(atomic_t *at)
{
	at->ref = 0;
}

static inline void atomic_destroy(atomic_t *at)
{
}

static inline i64 atomic_incrs(atomic_t *at, i64 ref)
{
	return __sync_fetch_and_add (&at->ref, ref);
}

static inline i64 atomic_decrs(atomic_t *at, i64 ref)
{
	return __sync_fetch_and_sub (&at->ref, ref);
}

static inline i64 atomic_incr(atomic_t *at)
{
	return atomic_incrs(at, 1);
}

static inline i64 atomic_decr(atomic_t *at)
{
	return atomic_decrs(at, 1);
}

static inline i64 atomic_fetch(atomic_t *at)
{
	return at->ref;
}

static inline i64 atomic_swap(atomic_t *at, int ref)
{
	BUG_ON(1);
}

#endif
