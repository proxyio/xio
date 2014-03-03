#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <pthread.h>

typedef struct spin {
    pthread_spinlock_t _spin;
} spin_t;

int spin_init(spin_t *spin);
int spin_lock(spin_t *spin);
int spin_unlock(spin_t *spin);
int spin_destroy(spin_t *spin);

#endif
