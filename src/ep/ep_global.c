#include <stdio.h>
#include "ep_struct.h"

struct xep_global xepgb = {};

void xep_global_init() {
    int i;
    xepgb.exiting = true;
    mutex_init(&xepgb.lock);

    for (i = 0; i < XIO_MAX_EPS; i++) {
	xepgb.unused[i] = i;
	xep_init(&xepgb.eps[i]);
    }
    xepgb.neps = 0;
}

void xep_global_exit() {
    int i;
    xepgb.exiting = true;
    mutex_destroy(&xepgb.lock);

    for (i = 0; i < XIO_MAX_EPS; i++)
	xep_exit(&xepgb.eps[i]);
}

int xep_alloc() {
    int eid;
    mutex_lock(&xepgb.lock);
    BUG_ON(xepgb.neps >= XIO_MAX_EPS);
    eid = xepgb.unused[xepgb.neps++];
    mutex_unlock(&xepgb.lock);
    return eid;
}

void xep_free(int eid) {
    mutex_lock(&xepgb.lock);
    xepgb.unused[--xepgb.neps] = eid;
    mutex_unlock(&xepgb.lock);
}

struct xep *xep_get(int eid) {
    return &xepgb.eps[eid];
}
