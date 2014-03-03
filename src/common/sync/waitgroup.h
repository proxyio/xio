#ifndef _WAITGROUP_H_
#define _WAITGROUP_H_

#include "mutex.h"
#include "condition.h"

typedef struct waitgroup {
    int ref;
    condition_t cond;
    mutex_t mutex;
} waitgroup_t;

int waitgroup_init(waitgroup_t *wg);
int waitgroup_destroy(waitgroup_t *wg);
int waitgroup_add(waitgroup_t *wg);
int waitgroup_adds(waitgroup_t *wg, int refs);
int waitgroup_done(waitgroup_t *wg);
int waitgroup_dones(waitgroup_t *wg, int refs);
int waitgroup_ref(waitgroup_t *wg);
int waitgroup_wait(waitgroup_t *wg);

#endif
