#ifndef _HPIO_MUTEX_
#define _HPIO_MUTEX_

#include <pthread.h>

typedef struct mutex {
    pthread_mutex_t _mutex;
} mutex_t;

#define MUTEX_INITIALIZER { PTHREAD_MUTEX_INITIALIZER }

int mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_trylock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);
int mutex_destroy(mutex_t *mutex);

#endif
