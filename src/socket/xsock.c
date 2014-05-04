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

/* Default input/output buffer size */
static int DEF_SNDBUF = 10485760;
static int DEF_RCVBUF = 10485760;

int fd_alloc() {
    int fd;
    mutex_lock(&xgb.lock);
    BUG_ON(xgb.nsocks >= XIO_MAX_SOCKS);
    fd = xgb.unused[xgb.nsocks++];
    mutex_unlock(&xgb.lock);
    return fd;
}

void fd_free(int fd) {
    mutex_lock(&xgb.lock);
    xgb.unused[--xgb.nsocks] = fd;
    mutex_unlock(&xgb.lock);
}

struct xsock *xget(int fd) {
    return &xgb.socks[fd];
}

static void xshutdown_task_f(struct xtask *ts) {
    struct xsock *self = cont_of(ts, struct xsock, shutdown);

    DEBUG_OFF("xsock %d shutdown %s", self->fd, pf_str[self->pf]);
    self->sockspec_vfptr->close(self->fd);
}

static void xsock_init(int fd) {
    struct xsock *self = xget(fd);

    mutex_init(&self->lock);
    condition_init(&self->cond);
    self->type = 0;
    self->pf = 0;
    ZERO(self->addr);
    ZERO(self->peer);
    self->fasync = false;
    self->fok = true;
    self->fclosed = false;

    self->owner = -1;
    INIT_LIST_HEAD(&self->sub_socks);
    INIT_LIST_HEAD(&self->sib_link);
    
    self->fd = fd;
    self->ref = 0;
    self->cpu_no = xcpu_choosed(fd);

    self->rcv.waiters = 0;
    self->rcv.buf = 0;
    self->rcv.wnd = DEF_RCVBUF;
    INIT_LIST_HEAD(&self->rcv.head);

    self->snd.waiters = 0;
    self->snd.buf = 0;
    self->snd.wnd = DEF_SNDBUF;
    INIT_LIST_HEAD(&self->snd.head);

    INIT_LIST_HEAD(&self->poll_entries);
    self->shutdown.f = xshutdown_task_f;
    INIT_LIST_HEAD(&self->shutdown.link);
    condition_init(&self->acceptq.cond);
    self->acceptq.waiters = 0;
    INIT_LIST_HEAD(&self->acceptq.head);
    INIT_LIST_HEAD(&self->acceptq.link);
}

static void xsock_exit(int fd) {
    struct xsock *self = xget(fd);
    struct list_head head = {};
    struct xmsg *pos, *nx;

    mutex_destroy(&self->lock);
    condition_destroy(&self->cond);
    self->type = -1;
    self->pf = -1;
    ZERO(self->addr);
    ZERO(self->peer);
    self->fasync = 0;
    self->fok = 0;
    self->fclosed = 0;

    self->owner = -1;
    BUG_ON(!list_empty(&self->sub_socks));
    BUG_ON(attached(&self->sib_link));
    
    self->fd = -1;
    self->cpu_no = -1;

    INIT_LIST_HEAD(&head);

    self->rcv.waiters = -1;
    self->rcv.buf = -1;
    self->rcv.wnd = -1;
    list_splice(&self->rcv.head, &head);

    self->snd.waiters = -1;
    self->snd.buf = -1;
    self->snd.wnd = -1;
    list_splice(&self->snd.head, &head);

    xmsg_walk_safe(pos, nx, &head) {
	xfreemsg(pos->vec.chunk);
    }

    /* It's possible that user call xclose() and xpoll_add()
     * at the same time. and attach_to_xsock() happen after xclose().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&self->poll_entries));
    self->acceptq.waiters = -1;
    condition_destroy(&self->acceptq.cond);

    /* TODO: destroy acceptq's connection */
}


struct xsock *xsock_alloc() {
    int fd = fd_alloc();
    struct xsock *self = xget(fd);
    xsock_init(fd);
    return self;
}

void xsock_free(struct xsock *self) {
    int fd = self->fd;
    xsock_exit(fd);
    fd_free(fd);
}
