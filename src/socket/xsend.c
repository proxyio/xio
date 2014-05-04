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

struct xmsg *sendq_pop(struct xsock *self) {
    struct pfspec *proto = self->proto;
    struct xmsg *msg = 0;
    i64 msgsz;
    u32 events = 0;
    
    mutex_lock(&self->lock);
    if (!list_empty(&self->snd_head)) {
	DEBUG_OFF("xsock %d", self->fd);
	msg = list_first(&self->snd_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	self->snd -= msgsz;
	events |= XMQ_POP;
	if (self->snd_wnd - self->snd <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&self->snd_head)) {
	    BUG_ON(self->snd);
	    events |= XMQ_EMPTY;
	}

	/* Wakeup the blocking waiters */
	if (self->snd_waiters > 0)
	    condition_broadcast(&self->cond);
    }

    if (events && proto->notify)
	proto->notify(self->fd, SEND_Q, events);

    __xpoll_notify(self);
    mutex_unlock(&self->lock);
    return msg;
}

int sendq_push(struct xsock *self, struct xmsg *msg) {
    int rc = -1;
    struct pfspec *proto = self->proto;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&self->lock);
    while (!can_send(self) && !self->fasync) {
	self->snd_waiters++;
	condition_wait(&self->cond, &self->lock);
	self->snd_waiters--;
    }
    if (can_send(self)) {
	rc = 0;
	if (list_empty(&self->snd_head))
	    events |= XMQ_NONEMPTY;
	if (self->snd_wnd - self->snd <= msgsz)
	    events |= XMQ_FULL;
	events |= XMQ_PUSH;
	self->snd += msgsz;
	list_add_tail(&msg->item, &self->snd_head);
	DEBUG_OFF("xsock %d", self->fd);
    }

    if (events && proto->notify)
	proto->notify(self->fd, SEND_Q, events);

    __xpoll_notify(self);
    mutex_unlock(&self->lock);
    return rc;
}

int xsend(int fd, char *xbuf) {
    int rc = 0;
    struct xmsg *msg = 0;
    struct xsock *self = xget(fd);

    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    if (self->type != XCONNECTOR) {
	errno = EPROTO;
	return -1;
    }
    msg = cont_of(xbuf, struct xmsg, vec.chunk);
    if ((rc = sendq_push(self, msg)) < 0) {
	errno = self->fok ? EAGAIN : EPIPE;
    }
    return rc;
}

