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

struct xmsg *recvq_pop(struct sockbase *self) {
    struct xmsg *msg = 0;
    struct sockbase_vfptr *vfptr = self->vfptr;
    i64 msgsz;
    u32 events = 0;

    mutex_lock(&self->lock);
    while (list_empty(&self->rcv.head) && !self->fasync) {
	self->rcv.waiters++;
	condition_wait(&self->cond, &self->lock);
	self->rcv.waiters--;
    }
    if (!list_empty(&self->rcv.head)) {
	DEBUG_OFF("%d", self->fd);
	msg = list_first(&self->rcv.head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	self->rcv.buf -= msgsz;
	events |= XMQ_POP;
	if (self->rcv.wnd - self->rcv.buf <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&self->rcv.head)) {
	    BUG_ON(self->rcv.buf);
	    events |= XMQ_EMPTY;
	}
    }

    if (events && vfptr->notify)
	vfptr->notify(self, RECV_Q, events);

    __xeventnotify(self);
    mutex_unlock(&self->lock);
    return msg;
}

void recvq_push(struct sockbase *self, struct xmsg *msg) {
    struct sockbase_vfptr *vfptr = self->vfptr;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&self->lock);
    if (list_empty(&self->rcv.head))
	events |= XMQ_NONEMPTY;
    if (self->rcv.wnd - self->rcv.buf <= msgsz)
	events |= XMQ_FULL;
    events |= XMQ_PUSH;
    self->rcv.buf += msgsz;
    list_add_tail(&msg->item, &self->rcv.head);    
    DEBUG_OFF("%d", self->fd);

    /* Wakeup the blocking waiters. */
    if (self->rcv.waiters > 0)
	condition_broadcast(&self->cond);

    if (events && vfptr->notify)
	vfptr->notify(self, RECV_Q, events);

    __xeventnotify(self);
    mutex_unlock(&self->lock);
}

int xrecv(int fd, char **xbuf) {
    int rc = 0;
    struct xmsg *msg = 0;
    struct sockbase *self;
    
    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    if (!(self = xget(fd))) {
	errno = EBADF;
	return -1;
    }
    if (!(msg = recvq_pop(self))) {
	errno = self->fok ? EAGAIN : EPIPE;
	rc = -1;
    } else {
	*xbuf = msg->vec.chunk;
    }
    xput(fd);
    return rc;
}



