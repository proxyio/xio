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

#include "base.h"
#include "spinlock.h"

#if !defined HAVE_DEBUG && defined HAVE_PTHREAD_SPIN_LOCK

int spin_init(spin_t* spin) {
    int rc;
    pthread_spinlock_t* lock = (pthread_spinlock_t*) spin;

    if ((rc = pthread_spin_init(lock, 0)) != 0) {
        ERRNO_RETURN(rc);
    }

    return 0;
}


int spin_lock(spin_t* spin) {
    int rc;
    pthread_spinlock_t* lock = (pthread_spinlock_t*) spin;

    if ((rc = pthread_spin_lock(lock)) != 0) {
        ERRNO_RETURN(rc);
    }

    return 0;
}

int spin_unlock(spin_t* spin) {
    int rc;
    pthread_spinlock_t* lock = (pthread_spinlock_t*) spin;

    if ((rc = pthread_spin_unlock(lock)) != 0) {
        ERRNO_RETURN(rc);
    }

    return 0;
}


int spin_destroy(spin_t* spin) {
    int rc;
    pthread_spinlock_t* lock = (pthread_spinlock_t*) spin;

    if ((rc = pthread_spin_destroy(lock)) != 0) {
        ERRNO_RETURN(rc);
    }

    return 0;
}



#endif
