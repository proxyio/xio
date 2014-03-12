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

#define atomic_add(at, nval) ({ int64_t __old;	\
	    spin_lock(&(at)->lock);		\
	    __old = (at)->val;			\
	    (at)->val += nval;			\
	    spin_unlock(&(at)->lock);		\
	    __old;				\
	})

#define atomic_dec(at, nval) ({ int64_t __old;	\
	    spin_lock(&(at)->lock);		\
	    __old = (at)->val;			\
	    (at)->val -= nval;			\
	    spin_unlock(&(at)->lock);		\
	    __old;				\
	})

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

#define atomic_dec_and_lock(at, ops, lock) do {	\
	int64_t __old;				\
	spin_lock(&(at)->lock);			\
	__old = (at)->val--;			\
	spin_unlock(&(at)->lock);		\
	if (__old == 1)				\
	    ops->lock(&lock);			\
    } while (0)
    



#endif
