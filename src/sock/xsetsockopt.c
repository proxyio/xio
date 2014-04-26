#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include "xgb.h"


int xsetsockopt(int xd, int level, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (!on || size <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&sx->lock);
	sx->fasync = *(int *)on ? true : false;
	mutex_unlock(&sx->lock);
	break;
    case XSNDBUF:
	mutex_lock(&sx->lock);
	sx->snd_wnd = (*(int *)on);
	mutex_unlock(&sx->lock);
	break;
    case XRCVBUF:
	mutex_lock(&sx->lock);
	sx->rcv_wnd = (*(int *)on);
	mutex_unlock(&sx->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}

