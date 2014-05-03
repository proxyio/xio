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


typedef int (*sock_getopt) (struct xsock *xsk, void *val, int *vallen);

static int get_noblock(struct xsock *xsk, void *val, int *vallen) {
    mutex_lock(&xsk->lock);
    *(int *)val = xsk->fasync ? true : false;
    mutex_unlock(&xsk->lock);
    return 0;
}

static int get_sndwin(struct xsock *xsk, void *val, int *vallen) {
    mutex_lock(&xsk->lock);
    *(int *)val = xsk->snd_wnd;
    mutex_unlock(&xsk->lock);
    return 0;
}

static int get_rcvwin(struct xsock *xsk, void *val, int *vallen) {
    mutex_lock(&xsk->lock);
    *(int *)val = xsk->rcv_wnd;
    mutex_unlock(&xsk->lock);
    return 0;
}

static int get_sndbuf(struct xsock *xsk, void *val, int *vallen) {
    mutex_lock(&xsk->lock);
    *(int *)val = xsk->snd;
    mutex_unlock(&xsk->lock);
    return 0;
}

static int get_rcvbuf(struct xsock *xsk, void *val, int *vallen) {
    mutex_lock(&xsk->lock);
    *(int *)val = xsk->rcv;
    mutex_unlock(&xsk->lock);
    return 0;
}


static int get_linger(struct xsock *xsk, void *val, int *vallen) {
    return -1;
}

static int get_sndtimeo(struct xsock *xsk, void *val, int *vallen) {
    return -1;
}

static int get_rcvtimeo(struct xsock *xsk, void *val, int *vallen) {
    return -1;
}

static int get_reconnect(struct xsock *xsk, void *val, int *vallen) {
    return -1;
}

static int get_reconn_ivl(struct xsock *xsk, void *val, int *vallen) {
    return -1;
}

static int get_reconn_ivlmax(struct xsock *xsk, void *val, int *vallen) {
    return -1;
}

static int get_socktype(struct xsock *xsk, void *val, int *vallen) {
    *(int *)val = xsk->type;
    return 0;
}

static int get_sockproto(struct xsock *xsk, void *val, int *vallen) {
    *(int *)val = xsk->pf;
    return 0;
}

static int get_tracedebug(struct xsock *xsk, void *val, int *vallen) {
    *(int *)val = (int)xsk->ftracedebug;
    return 0;
}

const sock_getopt getopt_vfptr[] = {
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
    get_tracedebug,
};


int xgetopt(int fd, int level, int opt, void *val, int *vallen) {
    int rc = 0;
    struct xsock *xsk = xget(fd);

    if ((level != XL_SOCKET && !xsk->l4proto->setsockopt) ||
	((level == XL_SOCKET && !getopt_vfptr[opt]) ||
	 (opt >= NELEM(getopt_vfptr, sock_getopt)))) {
	errno = EINVAL;
	return -1;
    }
    switch (level) {
    case XL_SOCKET:
	getopt_vfptr[opt](xsk, val, vallen);
	break;
    default:
	rc = xsk->l4proto->getsockopt(fd, level, opt, val, vallen);
    }
    return rc;
}
