#include <stdio.h>
#include "pipe_struct.h"

struct xpipe_global xpgb = {};

void xpipe_module_init() {
    int i;
    xpgb.exiting = true;
    mutex_init(&xpgb.lock);

    for (i = 0; i < XIO_MAX_PIPES; i++) {
	xpgb.unused[i] = i;
	xpipe_init(&xpgb.pipes[i]);
    }
    xpgb.npipes = 0;
}

void xpipe_module_exit() {
    int i;
    xpgb.exiting = true;
    mutex_destroy(&xpgb.lock);

    for (i = 0; i < XIO_MAX_PIPES; i++)
	xpipe_exit(&xpgb.pipes[i]);
}

int pfd_alloc() {
    int pfd;
    mutex_lock(&xpgb.lock);
    BUG_ON(xpgb.npipes >= XIO_MAX_PIPES);
    pfd = xpgb.unused[xpgb.npipes++];
    mutex_unlock(&xpgb.lock);
    return pfd;
}

void pfd_free(int pfd) {
    mutex_lock(&xpgb.lock);
    xpgb.unused[--xpgb.npipes] = pfd;
    mutex_unlock(&xpgb.lock);
}

struct pipe_struct *pfd_get(int pfd) {
    return &xpgb.pipes[pfd];
}
