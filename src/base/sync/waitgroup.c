#include <stdio.h>
#include "waitgroup.h"

int waitgroup_init(waitgroup_t *wg) {
    wg->ref = 0;
    if (condition_init(&wg->cond) < 0)
	return -1;
    return mutex_init(&wg->mutex);
}

int waitgroup_destroy(waitgroup_t *wg) {
    condition_destroy(&wg->cond);
    mutex_destroy(&wg->mutex);
    return 0;
}

static inline int __waitgroup_incr(waitgroup_t *wg, int refs) {
    mutex_lock(&wg->mutex);
    wg->ref += refs;
    mutex_unlock(&wg->mutex);
    return 0;
}

static inline int __waitgroup_decr(waitgroup_t *wg, int refs) {
    mutex_lock(&wg->mutex);
    if ((wg->ref -= refs) == 0)
	condition_broadcast(&wg->cond);
    mutex_unlock(&wg->mutex);
    return 0;
}

static inline int __waitgroup_get(waitgroup_t *wg) {
    int _ref = 0;
    mutex_lock(&wg->mutex);
    _ref = wg->ref;
    mutex_unlock(&wg->mutex);
    return _ref;
}

int waitgroup_add(waitgroup_t *wg) {
    return __waitgroup_incr(wg, 1);
}

int waitgroup_done(waitgroup_t *wg) {
    return __waitgroup_decr(wg, 1);
}

int waitgroup_adds(waitgroup_t *wg, int refs) {
    return __waitgroup_incr(wg, refs);
}

int waitgroup_dones(waitgroup_t *wg, int refs) {
    return __waitgroup_decr(wg, refs);
}

int waitgroup_wait(waitgroup_t *wg) {
    mutex_lock(&wg->mutex);
    while (wg->ref > 0)
	condition_wait(&wg->cond, &wg->mutex);
    mutex_unlock(&wg->mutex);
    return 0;
}








