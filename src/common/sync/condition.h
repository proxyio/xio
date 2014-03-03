#ifndef _CONDITION_H_
#define _CONDITION_H_

#include <pthread.h>
#include "mutex.h"

typedef struct condition {
    pthread_cond_t _cond;
} condition_t;

int condition_init(condition_t *cond);
int condition_destroy(condition_t *cond);
int condition_wait(condition_t *cond, mutex_t *mutex);
int condition_signal(condition_t *cond);
int condition_broadcast(condition_t *cond);

#endif
