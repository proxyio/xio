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

int acceptq_add(struct sockbase *sb, struct sockbase *new) {
    int rc = 0;

    while (sb->owner >= 0)
	sb = xgb.sockbases[sb->owner];

    mutex_lock(&sb->lock);
    if (list_empty(&sb->acceptq.head) && sb->acceptq.waiters > 0) {
	condition_broadcast(&sb->acceptq.cond);
    }
    list_add_tail(&new->acceptq.link, &sb->acceptq.head);
    __emit_pollevents(sb);
    mutex_unlock(&sb->lock);
    return rc;
}


int __acceptq_rm_nohup(struct sockbase *sb, struct sockbase **new) {
    if (!list_empty(&sb->acceptq.head)) {
	*new = list_first(&sb->acceptq.head, struct sockbase, acceptq.link);
	list_del_init(&(*new)->acceptq.link);
	return 0;
    }
    return -1;
}

int acceptq_rm_nohup(struct sockbase *sb, struct sockbase **new) {
    int rc;
    mutex_lock(&sb->lock);
    rc = __acceptq_rm_nohup(sb, new);
    mutex_unlock(&sb->lock);
    return rc;
}


int acceptq_rm(struct sockbase *sb, struct sockbase **new) {
    int rc = -1;

    mutex_lock(&sb->lock);
    while (list_empty(&sb->acceptq.head) && !sb->fasync) {
	sb->acceptq.waiters++;
	condition_wait(&sb->acceptq.cond, &sb->lock);
	sb->acceptq.waiters--;
    }
    rc = __acceptq_rm_nohup(sb, new);
    __emit_pollevents(sb);
    mutex_unlock(&sb->lock);
    return rc;
}

int xaccept(int fd) {
    struct sockbase *new = 0;
    struct sockbase *sb = xget(fd);

    if (!sb) {
	errno = EBADF;
	return -1;
    }
    if (acceptq_rm(sb, &new) < 0) {
	xput(fd);
	errno = EAGAIN;
	return -1;
    }
    xput(fd);
    return new->fd;
}

int _xlisten(int pf, const char *addr) {
    int fd;

    if ((fd = xsocket(pf, XLISTENER)) < 0)
	return -1;
    if (xbind(fd, addr) != 0) {
	xclose(fd);
	return -1;
    }
    return fd;
}


int xlisten(const char *addr) {
    int pf = sockaddr_pf(addr);
    char sockaddr[TP_SOCKADDRLEN] = {};

    if (pf < 0 || sockaddr_addr(addr, sockaddr, sizeof(sockaddr)) != 0) {
	errno = EPROTO;
	return -1;
    }
    return _xlisten(pf, sockaddr);
}
