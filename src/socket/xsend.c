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

struct xmsg *sendq_pop(struct xsock *xsk) {
    struct xsock_protocol *l4proto = xsk->l4proto;
    struct xmsg *msg = 0;
    i64 msgsz;
    u32 events = 0;
    
    mutex_lock(&xsk->lock);
    if (!list_empty(&xsk->snd_head)) {
	DEBUG_OFF("xsock %d", xsk->fd);
	msg = list_first(&xsk->snd_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	xsk->snd -= msgsz;
	events |= XMQ_POP;
	if (xsk->snd_wnd - xsk->snd <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&xsk->snd_head)) {
	    BUG_ON(xsk->snd);
	    events |= XMQ_EMPTY;
	}

	/* Wakeup the blocking waiters */
	if (xsk->snd_waiters > 0)
	    condition_broadcast(&xsk->cond);
    }

    if (events && l4proto->notify)
	l4proto->notify(xsk->fd, SEND_Q, events);

    __xpoll_notify(xsk);
    mutex_unlock(&xsk->lock);
    return msg;
}

int sendq_push(struct xsock *xsk, struct xmsg *msg) {
    int rc = -1;
    struct xsock_protocol *l4proto = xsk->l4proto;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&xsk->lock);
    while (!can_send(xsk) && !xsk->fasync) {
	xsk->snd_waiters++;
	condition_wait(&xsk->cond, &xsk->lock);
	xsk->snd_waiters--;
    }
    if (can_send(xsk)) {
	rc = 0;
	if (list_empty(&xsk->snd_head))
	    events |= XMQ_NONEMPTY;
	if (xsk->snd_wnd - xsk->snd <= msgsz)
	    events |= XMQ_FULL;
	events |= XMQ_PUSH;
	xsk->snd += msgsz;
	list_add_tail(&msg->item, &xsk->snd_head);
	DEBUG_OFF("xsock %d", xsk->fd);
    }

    if (events && l4proto->notify)
	l4proto->notify(xsk->fd, SEND_Q, events);

    __xpoll_notify(xsk);
    mutex_unlock(&xsk->lock);
    return rc;
}

int xsend(int fd, char *xbuf) {
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
    msg = cont_of(xbuf, struct xmsg, vec.chunk);
    if ((rc = sendq_push(xsk, msg)) < 0) {
	errno = xsk->fok ? EAGAIN : EPIPE;
    }
    return rc;
}

