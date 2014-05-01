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


typedef int (*xsockopf) (struct xsock *sx, void *val, int *vallen);

static int get_noblock(struct xsock *sx, void *val, int *vallen) {
    mutex_lock(&sx->lock);
    *(int *)val = sx->fasync ? true : false;
    mutex_unlock(&sx->lock);
    return 0;
}

static int get_sndwin(struct xsock *sx, void *val, int *vallen) {
    mutex_lock(&sx->lock);
    *(int *)val = sx->snd_wnd;
    mutex_unlock(&sx->lock);
    return 0;
}

static int get_rcvwin(struct xsock *sx, void *val, int *vallen) {
    mutex_lock(&sx->lock);
    *(int *)val = sx->rcv_wnd;
    mutex_unlock(&sx->lock);
    return 0;
}

static int get_sndbuf(struct xsock *sx, void *val, int *vallen) {
    mutex_lock(&sx->lock);
    *(int *)val = sx->snd;
    mutex_unlock(&sx->lock);
    return 0;
}

static int get_rcvbuf(struct xsock *sx, void *val, int *vallen) {
    mutex_lock(&sx->lock);
    *(int *)val = sx->rcv;
    mutex_unlock(&sx->lock);
    return 0;
}


static int get_linger(struct xsock *sx, void *val, int *vallen) {
    return -1;
}

static int get_sndtimeo(struct xsock *sx, void *val, int *vallen) {
    return -1;
}

static int get_rcvtimeo(struct xsock *sx, void *val, int *vallen) {
    return -1;
}

static int get_reconnect(struct xsock *sx, void *val, int *vallen) {
    return -1;
}

static int get_reconn_ivl(struct xsock *sx, void *val, int *vallen) {
    return -1;
}

static int get_reconn_ivlmax(struct xsock *sx, void *val, int *vallen) {
    return -1;
}

static int get_socktype(struct xsock *sx, void *val, int *vallen) {
    *(int *)val = sx->type;
    return -1;
}

static int get_sockproto(struct xsock *sx, void *val, int *vallen) {
    *(int *)val = sx->pf;
    return -1;
}


const xsockopf getopt_vf[] = {
    get_noblock,
    get_sndwin,
    get_rcvwin,
    get_sndbuf,
    get_rcvbuf,
    get_linger,
    get_sndtimeo,
    get_rcvtimeo,
    get_reconnect,
    get_reconn_ivl,
    get_reconn_ivlmax,
    get_socktype,
    get_sockproto,
};


int xgetsockopt(int xd, int level, int opt, void *val, int *vallen) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    BUG_ON(NELEM(getopt_vf, xsockopf) != 13);
    if ((level != XL_SOCKET && !sx->l4proto->setsockopt) ||
	((level == XL_SOCKET && !getopt_vf[opt]) ||
	 (opt >= NELEM(getopt_vf, xsockopf)))) {
	errno = EINVAL;
	return -1;
    }
    switch (level) {
    case XL_SOCKET:
	getopt_vf[opt](sx, val, vallen);
	break;
    default:
	rc = sx->l4proto->getsockopt(xd, level, opt, val, vallen);
    }
    return rc;
}
