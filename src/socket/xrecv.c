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

struct xmsg *recvq_pop(struct xsock *xsk) {
    struct xmsg *msg = 0;
    struct xsock_protocol *l4proto = xsk->l4proto;
    i64 msgsz;
    u32 events = 0;

    mutex_lock(&xsk->lock);
    while (list_empty(&xsk->rcv_head) && !xsk->fasync) {
	xsk->rcv_waiters++;
	condition_wait(&xsk->cond, &xsk->lock);
	xsk->rcv_waiters--;
    }
    if (!list_empty(&xsk->rcv_head)) {
	DEBUG_OFF("%d", xsk->fd);
	msg = list_first(&xsk->rcv_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	xsk->rcv -= msgsz;
	events |= XMQ_POP;
	if (xsk->rcv_wnd - xsk->rcv <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&xsk->rcv_head)) {
	    BUG_ON(xsk->rcv);
	    events |= XMQ_EMPTY;
	}
    }

    if (events && l4proto->notify)
	l4proto->notify(xsk->fd, RECV_Q, events);

    __xpoll_notify(xsk);
    mutex_unlock(&xsk->lock);
    return msg;
}

void recvq_push(struct xsock *xsk, struct xmsg *msg) {
    struct xsock_protocol *l4proto = xsk->l4proto;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&xsk->lock);
    if (list_empty(&xsk->rcv_head))
	events |= XMQ_NONEMPTY;
    if (xsk->rcv_wnd - xsk->rcv <= msgsz)
	events |= XMQ_FULL;
    events |= XMQ_PUSH;
    xsk->rcv += msgsz;
    list_add_tail(&msg->item, &xsk->rcv_head);    
    DEBUG_OFF("%d", xsk->fd);

    /* Wakeup the blocking waiters. */
    if (xsk->rcv_waiters > 0)
	condition_broadcast(&xsk->cond);

    if (events && l4proto->notify)
	l4proto->notify(xsk->fd, RECV_Q, events);

    __xpoll_notify(xsk);
    mutex_unlock(&xsk->lock);
}

int xrecv(int fd, char **xbuf) {
    int rc = 0;
    struct xmsg *msg = 0;
    struct xsock *xsk = xget(fd);
    
    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    if (xsk->type != XCONNECTOR) {
	errno = EPROTO;
	return -1;
    }
    if (!(msg = recvq_pop(xsk))) {
	errno = xsk->fok ? EAGAIN : EPIPE;
	rc = -1;
    } else {
	*xbuf = msg->vec.chunk;
    }
    return rc;
}



