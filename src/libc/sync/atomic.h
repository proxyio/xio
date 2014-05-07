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

#ifndef _HPIO_ATOMIC_
#define _HPIO_ATOMIC_

#include <inttypes.h>
#include "sync/spin.h"

typedef struct atomic {
    int64_t val;
    spin_t lock;
} atomic_t;

#define atomic_init(at) do {			\
	spin_init(&(at)->lock);			\
	(at)->val = 0;				\
    } while (0)

#define atomic_destroy(at) do {			\
	spin_destroy(&(at)->lock);		\
    } while (0)

#define atomic_incs(at, nval) ({ int64_t __old;	\
	    spin_lock(&(at)->lock);		\
	    __old = (at)->val;			\
	    (at)->val += nval;			\
	    spin_unlock(&(at)->lock);		\
	    __old;				\
	})

#define atomic_decs(at, nval) ({ int64_t __old;	\
	    spin_lock(&(at)->lock);		\
	    __old = (at)->val;			\
	    (at)->val -= nval;			\
	    spin_unlock(&(at)->lock);		\
	    __old;				\
	})

#define atomic_inc(at) atomic_incs(at, 1)
#define atomic_dec(at) atomic_decs(at, 1)

#define atomic_read(at) ({ int64_t __old;	\
	    spin_lock(&(at)->lock);		\
	    __old = (at)->val;			\
	    spin_unlock(&(at)->lock);		\
	    __old;})

#define atomic_set(at, nval) ({ int64_t __old;	\
	    spin_lock(&(at)->lock);		\
	    __old = (at)->val;			\
	    (at)->val = nval;			\
	    spin_unlock(&(at)->lock);		\
	    __old;})

#define atomic_dec_and_lock(at, locktype, ext_lock) if (({	\
		int64_t __old;					\
		spin_lock(&(at)->lock);				\
		__old = (at)->val--;				\
		spin_unlock(&(at)->lock);			\
		if (__old == 1)					\
		    locktype##_lock(&ext_lock);			\
		__old;						\
	    }) == 1)
    



#endif
