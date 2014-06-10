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

#ifndef _XIO_ATOMIC_
#define _XIO_ATOMIC_

#include <inttypes.h>
#include "base.h"
#include "spinlock.h"

typedef struct atomic {
	int64_t val;
	spin_t lock;
} atomic_t;

static inline void atomic_init(atomic_t *at)
{
	spin_init(&at->lock);
	at->val = 0;
}

static inline void atomic_destroy(atomic_t *at)
{
	spin_destroy(&at->lock);
}

static inline i64 atomic_incs(atomic_t *at, i64 ref)
{
	int64_t old;
	spin_lock(&at->lock);
	old = at->val;
	at->val += ref;
	spin_unlock(&at->lock);
	return old;
}

static inline i64 atomic_decs(atomic_t *at, i64 ref)
{
	int64_t old;
	spin_lock(&at->lock);
	old = at->val;
	at->val -= ref;
	spin_unlock(&at->lock);
	return old;
}

static inline i64 atomic_inc(atomic_t *at)
{
	return atomic_incs(at, 1);
}

static inline i64 atomic_dec(atomic_t *at)
{
	return atomic_decs(at, 1);
}

static inline i64 atomic_fetch(atomic_t *at)
{
	int64_t old;
	spin_lock(&at->lock);
	old = at->val;
	spin_unlock(&at->lock);
	return old;
}

static inline i64 atomic_swap(atomic_t *at, int ref)
{
	int64_t old;
	spin_lock(&at->lock);
	old = at->val;
	at->val = ref;
	spin_unlock(&at->lock);
	return old;
}

#endif
