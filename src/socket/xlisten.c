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

int acceptq_push(struct xsock *xsk, struct xsock *req_xsk) {
    int rc = 0;

    while (xsk->owner >= 0)
	xsk = xget(xsk->owner);

    mutex_lock(&xsk->lock);
    if (list_empty(&xsk->acceptq.head) && xsk->acceptq.waiters > 0) {
	condition_broadcast(&xsk->acceptq.cond);
    }
    list_add_tail(&req_xsk->acceptq.link, &xsk->acceptq.head);
    __xpoll_notify(xsk);
    mutex_unlock(&xsk->lock);
    return rc;
}

struct xsock *acceptq_pop(struct xsock *xsk) {
    struct xsock *req_xsk = 0;

    mutex_lock(&xsk->lock);
    while (list_empty(&xsk->acceptq.head) && !xsk->fasync) {
	xsk->acceptq.waiters++;
	condition_wait(&xsk->acceptq.cond, &xsk->lock);
	xsk->acceptq.waiters--;
    }
    if (!list_empty(&xsk->acceptq.head)) {
	req_xsk = list_first(&xsk->acceptq.head, struct xsock, acceptq.link);
	list_del_init(&req_xsk->acceptq.link);
    }
    __xpoll_notify(xsk);
    mutex_unlock(&xsk->lock);
    return req_xsk;
}

int xaccept(int fd) {
    struct xsock *xsk = xget(fd);
    struct xsock *new_xsk = 0;

    if (!xsk->fok) {
	errno = EPIPE;
	return -1;
    }
    if (xsk->type != XLISTENER) {
	errno = EPROTO;
	return -1;
    }
    if ((new_xsk = acceptq_pop(xsk)))
	return new_xsk->fd;
    errno = EAGAIN;
    return -1;
}

int _xlisten(int pf, const char *addr) {
    int fd = xsocket(pf, XLISTENER);

    if (pf <= 0) {
	errno = EPROTO;
	return -1;
    }
    if (fd < 0) {
	errno = EMFILE;
	return -1;
    }
    if (xbind(fd, addr) != 0)
	return -1;
    return fd;
}


int xlisten(const char *addr) {
    int pf = sockaddr_pf(addr);
    char xsk_addr[TP_SOCKADDRLEN] = {};

    if (sockaddr_addr(addr, xsk_addr, sizeof(xsk_addr)) != 0)
	return -1;
    return _xlisten(pf, xsk_addr);
}
