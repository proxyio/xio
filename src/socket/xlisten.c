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
#include <transport/sockaddr.h>
#include "xgb.h"

int reqsocks_push(struct xsock *sx, struct xsock *req_sx) {
    int rc = 0;

    while (sx->parent >= 0)
	sx = xget(sx->parent);

    mutex_lock(&sx->lock);
    if (list_empty(&sx->acceptq) && sx->acceptq_waiters > 0) {
	condition_broadcast(&sx->acceptq_cond);
    }
    list_add_tail(&req_sx->acceptq_link, &sx->acceptq);
    __xpoll_notify(sx);
    mutex_unlock(&sx->lock);
    return rc;
}

struct xsock *reqsocks_pop(struct xsock *sx) {
    struct xsock *req_sx = 0;

    mutex_lock(&sx->lock);
    while (list_empty(&sx->acceptq) && !sx->fasync) {
	sx->acceptq_waiters++;
	condition_wait(&sx->acceptq_cond, &sx->lock);
	sx->acceptq_waiters--;
    }
    if (!list_empty(&sx->acceptq)) {
	req_sx = list_first(&sx->acceptq, struct xsock, acceptq_link);
	list_del_init(&req_sx->acceptq_link);
    }
    mutex_unlock(&sx->lock);
    return req_sx;
}

int xaccept(int xd) {
    struct xsock *sx = xget(xd);
    struct xsock *new_sx = 0;

    if (!sx->fok) {
	errno = EPIPE;
	return -1;
    }
    if (sx->type != XLISTENER) {
	errno = EPROTO;
	return -1;
    }
    if ((new_sx = reqsocks_pop(sx)))
	return new_sx->xd;
    errno = EAGAIN;
    return -1;
}

int _xlisten(int pf, const char *addr) {
    int xd = xsocket(pf, XLISTENER);

    if (pf <= 0) {
	errno = EPROTO;
	return -1;
    }
    if (xd < 0) {
	errno = EMFILE;
	return -1;
    }
    if (xbind(xd, addr) != 0)
	return -1;
    return xd;
}


int xlisten(const char *addr) {
    int pf = sockaddr_pf(addr);
    char sx_addr[TP_SOCKADDRLEN] = {};

    if (sockaddr_addr(addr, sx_addr, sizeof(sx_addr)) != 0)
	return -1;
    return _xlisten(pf, sx_addr);
}
