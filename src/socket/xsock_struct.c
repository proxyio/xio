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
    struct xsock *xsk = cont_of(ts, struct xsock, shutdown);

    DEBUG_OFF("xsock %d shutdown protocol %s", xsk->fd, xprotocol_str[xsk->pf]);
    xsk->l4proto->close(xsk->fd);
}

static void xsock_init(int fd) {
    struct xsock *xsk = xget(fd);

    mutex_init(&xsk->lock);
    condition_init(&xsk->cond);
    xsk->type = 0;
    xsk->pf = 0;
    ZERO(xsk->addr);
    ZERO(xsk->peer);
    xsk->fasync = false;
    xsk->fok = true;
    xsk->fclosed = false;

    xsk->owner = -1;
    INIT_LIST_HEAD(&xsk->sub_socks);
    INIT_LIST_HEAD(&xsk->sib_link);
    
    xsk->fd = fd;
    xsk->cpu_no = xcpu_choosed(fd);
    xsk->rcv_waiters = 0;
    xsk->snd_waiters = 0;
    xsk->rcv = 0;
    xsk->snd = 0;
    xsk->rcv_wnd = DEF_RCVBUF;
    xsk->snd_wnd = DEF_SNDBUF;
    INIT_LIST_HEAD(&xsk->rcv_head);
    INIT_LIST_HEAD(&xsk->snd_head);
    INIT_LIST_HEAD(&xsk->xpoll_head);
    xsk->shutdown.f = xshutdown_task_f;
    INIT_LIST_HEAD(&xsk->shutdown.link);
    condition_init(&xsk->acceptq.cond);
    xsk->acceptq.waiters = 0;
    INIT_LIST_HEAD(&xsk->acceptq.head);
    INIT_LIST_HEAD(&xsk->acceptq.link);
}

static void xsock_exit(int fd) {
    struct xsock *xsk = xget(fd);
    struct list_head head = {};
    struct xmsg *pos, *nx;

    mutex_destroy(&xsk->lock);
    condition_destroy(&xsk->cond);
    xsk->type = -1;
    xsk->pf = -1;
    ZERO(xsk->addr);
    ZERO(xsk->peer);
    xsk->fasync = 0;
    xsk->fok = 0;
    xsk->fclosed = 0;

    xsk->owner = -1;
    BUG_ON(!list_empty(&xsk->sub_socks));
    BUG_ON(attached(&xsk->sib_link));
    
    xsk->fd = -1;
    xsk->cpu_no = -1;
    xsk->rcv_waiters = -1;
    xsk->snd_waiters = -1;
    xsk->rcv = -1;
    xsk->snd = -1;
    xsk->rcv_wnd = -1;
    xsk->snd_wnd = -1;

    INIT_LIST_HEAD(&head);
    list_splice(&xsk->rcv_head, &head);
    list_splice(&xsk->snd_head, &head);
    xmsg_walk_safe(pos, nx, &head) {
	xfreemsg(pos->vec.chunk);
    }

    /* It's possible that user call xclose() and xpoll_add()
     * at the same time. and attach_to_xsock() happen after xclose().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&xsk->xpoll_head));
    xsk->acceptq.waiters = -1;
    condition_destroy(&xsk->acceptq.cond);

    /* TODO: destroy acceptq's connection */
}


struct xsock *xsock_alloc() {
    int fd = fd_alloc();
    struct xsock *xsk = xget(fd);
    xsock_init(fd);
    return xsk;
}

void xsock_free(struct xsock *xsk) {
    int fd = xsk->fd;
    xsock_exit(fd);
    fd_free(fd);
}
