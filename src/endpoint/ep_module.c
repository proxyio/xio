#include <stdio.h>
#include "ep_struct.h"

struct xep_global epgb = {};

void xep_module_init() {
    int i;
    epgb.exiting = true;
    mutex_init(&epgb.lock);

    for (i = 0; i < XIO_MAX_ENDPOINTS; i++) {
	epgb.unused[i] = i;
    }
    epgb.nendpoints = 0;
}

void xep_module_exit() {
    epgb.exiting = true;
    mutex_destroy(&epgb.lock);
}

int efd_alloc() {
    int efd;
    mutex_lock(&epgb.lock);
    BUG_ON(epgb.nendpoints >= XIO_MAX_ENDPOINTS);
    efd = epgb.unused[epgb.nendpoints++];
    mutex_unlock(&epgb.lock);
    return efd;
}

void efd_free(int efd) {
    mutex_lock(&epgb.lock);
    epgb.unused[--epgb.nendpoints] = efd;
    mutex_unlock(&epgb.lock);
}

struct endpoint *efd_get(int efd) {
    return &epgb.endpoints[efd];
}
