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

int xd_alloc() {
    int xd;
    mutex_lock(&xgb.lock);
    BUG_ON(xgb.nsocks >= XIO_MAX_SOCKS);
    xd = xgb.unused[xgb.nsocks++];
    mutex_unlock(&xgb.lock);
    return xd;
}

void xd_free(int xd) {
    mutex_lock(&xgb.lock);
    xgb.unused[--xgb.nsocks] = xd;
    mutex_unlock(&xgb.lock);
}

struct xsock *xget(int xd) {
    return &xgb.socks[xd];
}

static void xshutdown_task_f(struct xtask *ts) {
    struct xsock *sx = cont_of(ts, struct xsock, shutdown);

    DEBUG_OFF("xsock %d shutdown protocol %s", sx->xd, xprotocol_str[sx->pf]);
    sx->l4proto->close(sx->xd);
}

static void xsock_init(int xd) {
    struct xsock *sx = xget(xd);

    mutex_init(&sx->lock);
    condition_init(&sx->cond);
    sx->type = 0;
    sx->pf = 0;
    ZERO(sx->addr);
    ZERO(sx->peer);
    sx->fasync = false;
    sx->fok = true;
    sx->fclosed = false;

    sx->parent = -1;
    INIT_LIST_HEAD(&sx->sub_socks);
    INIT_LIST_HEAD(&sx->sib_link);
    
    sx->xd = xd;
    sx->cpu_no = xcpu_choosed(xd);
    sx->rcv_waiters = 0;
    sx->snd_waiters = 0;
    sx->rcv = 0;
    sx->snd = 0;
    sx->rcv_wnd = DEF_RCVBUF;
    sx->snd_wnd = DEF_SNDBUF;
    INIT_LIST_HEAD(&sx->rcv_head);
    INIT_LIST_HEAD(&sx->snd_head);
    INIT_LIST_HEAD(&sx->xpoll_head);
    sx->shutdown.f = xshutdown_task_f;
    INIT_LIST_HEAD(&sx->shutdown.link);
    condition_init(&sx->acceptq.cond);
    sx->acceptq.waiters = 0;
    INIT_LIST_HEAD(&sx->acceptq.head);
    INIT_LIST_HEAD(&sx->acceptq.link);
}

static void xsock_exit(int xd) {
    struct xsock *sx = xget(xd);
    struct list_head head = {};
    struct xmsg *pos, *nx;

    mutex_destroy(&sx->lock);
    condition_destroy(&sx->cond);
    sx->type = -1;
    sx->pf = -1;
    ZERO(sx->addr);
    ZERO(sx->peer);
    sx->fasync = 0;
    sx->fok = 0;
    sx->fclosed = 0;

    sx->parent = -1;
    BUG_ON(!list_empty(&sx->sub_socks));
    BUG_ON(attached(&sx->sib_link));
    
    sx->xd = -1;
    sx->cpu_no = -1;
    sx->rcv_waiters = -1;
    sx->snd_waiters = -1;
    sx->rcv = -1;
    sx->snd = -1;
    sx->rcv_wnd = -1;
    sx->snd_wnd = -1;

    INIT_LIST_HEAD(&head);
    list_splice(&sx->rcv_head, &head);
    list_splice(&sx->snd_head, &head);
    xmsg_walk_safe(pos, nx, &head) {
	xfreemsg(pos->vec.chunk);
    }

    /* It's possible that user call xclose() and xpoll_add()
     * at the same time. and attach_to_xsock() happen after xclose().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&sx->xpoll_head));
    sx->acceptq.waiters = -1;
    condition_destroy(&sx->acceptq.cond);

    /* TODO: destroy acceptq's connection */
}


struct xsock *xsock_alloc() {
    int xd = xd_alloc();
    struct xsock *sx = xget(xd);
    xsock_init(xd);
    return sx;
}

void xsock_free(struct xsock *sx) {
    int xd = sx->xd;
    xsock_exit(xd);
    xd_free(xd);
}
