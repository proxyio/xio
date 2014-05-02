/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include "xgb.h"

typedef int (*xsockopf) (struct xsock *sx, void *val, int vallen);

static int set_noblock(struct xsock *sx, void *val, int vallen) {
    mutex_lock(&sx->lock);
    sx->fasync = *(int *)val ? true : false;
    mutex_unlock(&sx->lock);
    return 0;
}

static int set_sndwin(struct xsock *sx, void *val, int vallen) {
    mutex_lock(&sx->lock);
    sx->snd_wnd = (*(int *)val);
    mutex_unlock(&sx->lock);
    return 0;
}

static int set_rcvwin(struct xsock *sx, void *val, int vallen) {
    mutex_lock(&sx->lock);
    sx->rcv_wnd = (*(int *)val);
    mutex_unlock(&sx->lock);
    return 0;
}

static int set_linger(struct xsock *sx, void *val, int vallen) {
    return -1;
}

static int set_sndtimeo(struct xsock *sx, void *val, int vallen) {
    return -1;
}

static int set_rcvtimeo(struct xsock *sx, void *val, int vallen) {
    return -1;
}

static int set_reconnect(struct xsock *sx, void *val, int vallen) {
    return -1;
}

static int set_reconn_ivl(struct xsock *sx, void *val, int vallen) {
    return -1;
}

static int set_reconn_ivlmax(struct xsock *sx, void *val, int vallen) {
    return -1;
}

static int set_tracedebug(struct xsock *sx, void *val, int vallen) {
    mutex_lock(&sx->lock);
    sx->ftracedebug = *(int *)val ? true : false;
    mutex_unlock(&sx->lock);
    return 0;
}

const xsockopf setopt_vf[] = {
    set_noblock,
    set_sndwin,
    set_rcvwin,
    0,
    0,
    set_linger,
    set_sndtimeo,
    set_rcvtimeo,
    set_reconnect,
    set_reconn_ivl,
    set_reconn_ivlmax,
    0,
    0,
    set_tracedebug,
};

int xsetsockopt(int xd, int level, int opt, void *val, int vallen) {
    int rc;
    struct xsock *sx = xget(xd);

    BUG_ON(NELEM(setopt_vf, xsockopf) != 13);
    if ((level != XL_SOCKET && !sx->l4proto->setsockopt) ||
	((level == XL_SOCKET && !setopt_vf[opt]) ||
	 (opt >= NELEM(setopt_vf, xsockopf)))) {
	errno = EINVAL;
	return -1;
    }
    switch (level) {
    case XL_SOCKET:
	setopt_vf[opt](sx, val, vallen);
	break;
    default:
	rc = sx->l4proto->setsockopt(xd, level, opt, val, vallen);
    }
    return rc;
}

