#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include "xgb.h"


int xgetsockopt(int xd, int level, int opt, void *on, int *size) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (!on) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&sx->lock);
	*(int *)on = sx->fasync ? true : false;
	mutex_unlock(&sx->lock);
	break;
    case XSNDBUF:
	mutex_lock(&sx->lock);
	*(int *)on = sx->snd_wnd;
	mutex_unlock(&sx->lock);
	break;
    case XRCVBUF:
	mutex_lock(&sx->lock);
	*(int *)on = sx->rcv_wnd;
	mutex_unlock(&sx->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}
