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

int acceptq_push(struct sockbase *self, struct sockbase *new) {
    int rc = 0;

    while (self->owner >= 0)
	self = xget(self->owner);

    mutex_lock(&self->lock);
    if (list_empty(&self->acceptq.head) && self->acceptq.waiters > 0) {
	condition_broadcast(&self->acceptq.cond);
    }
    list_add_tail(&new->acceptq.link, &self->acceptq.head);
    __xeventnotify(self);
    mutex_unlock(&self->lock);
    return rc;
}

struct sockbase *acceptq_pop(struct sockbase *self) {
    struct sockbase *new = 0;

    mutex_lock(&self->lock);
    while (list_empty(&self->acceptq.head) && !self->fasync) {
	self->acceptq.waiters++;
	condition_wait(&self->acceptq.cond, &self->lock);
	self->acceptq.waiters--;
    }
    if (!list_empty(&self->acceptq.head)) {
	new = list_first(&self->acceptq.head, struct sockbase, acceptq.link);
	list_del_init(&new->acceptq.link);
    }
    __xeventnotify(self);
    mutex_unlock(&self->lock);
    return new;
}

int xaccept(int fd) {
    struct sockbase *self = xget(fd);
    struct sockbase *new_self = 0;

    if (!self->fok) {
	errno = EPIPE;
	return -1;
    }
    if (self->type != XLISTENER) {
	errno = EPROTO;
	return -1;
    }
    if ((new_self = acceptq_pop(self)))
	return new_self->fd;
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
    char sockaddr[TP_SOCKADDRLEN] = {};

    if (sockaddr_addr(addr, sockaddr, sizeof(sockaddr)) != 0)
	return -1;
    return _xlisten(pf, sockaddr);
}
