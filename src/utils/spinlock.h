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

#ifndef _H_PROXYIO_SPINLOCK_
#define _H_PROXYIO_SPINLOCK_

#include <config.h>
#include <pthread.h>

#if !defined HAVE_DEBUG && defined HAVE_PTHREAD_SPIN_LOCK

typedef struct spin {
    pthread_spinlock_t _spin;
} spin_t;

int spin_init(spin_t* lock);
int spin_lock(spin_t* lock);
int spin_unlock(spin_t* lock);

static inline void spin_relock(spin_t* lock) {
    spin_unlock(lock);
    spin_lock(lock);
}

int spin_destroy(spin_t* lock);

#else

#include "mutex.h"

typedef mutex_t spin_t;

static inline int spin_init(spin_t* lock) {
    return mutex_init(lock);
}

static inline int spin_lock(spin_t* lock) {
    return mutex_lock(lock);
}

static inline int spin_unlock(spin_t* lock) {
    return mutex_unlock(lock);
}

static inline void spin_relock(spin_t* lock) {
    spin_unlock(lock);
    spin_lock(lock);
}

static inline int spin_destroy(spin_t* lock) {
    return mutex_destroy(lock);
}


#endif


#endif
