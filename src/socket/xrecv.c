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

struct xmsg *recvq_pop(struct sockbase *sb) {
    struct xmsg *msg = 0;
    struct sockbase_vfptr *vfptr = sb->vfptr;
    i64 msgsz;
    u32 events = 0;

    mutex_lock(&sb->lock);
    while (!sb->fepipe && list_empty(&sb->rcv.head) && !sb->fasync) {
	sb->rcv.waiters++;
	condition_wait(&sb->cond, &sb->lock);
	sb->rcv.waiters--;
    }
    if (!list_empty(&sb->rcv.head)) {
	DEBUG_OFF("%d", sb->fd);
	msg = list_first(&sb->rcv.head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xmsg_iovlen(msg);
	sb->rcv.buf -= msgsz;
	events |= XMQ_POP;
	if (sb->rcv.wnd - sb->rcv.buf <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&sb->rcv.head)) {
	    BUG_ON(sb->rcv.buf);
	    events |= XMQ_EMPTY;
	}
    }

    if (events && vfptr->notify)
	vfptr->notify(sb, RECV_Q, events);

    __emit_pollevents(sb);
    mutex_unlock(&sb->lock);
    return msg;
}

void recvq_push(struct sockbase *sb, struct xmsg *msg) {
    struct sockbase_vfptr *vfptr = sb->vfptr;
    u32 events = 0;
    i64 msgsz = xmsg_iovlen(msg);

    mutex_lock(&sb->lock);
    if (list_empty(&sb->rcv.head))
	events |= XMQ_NONEMPTY;
    if (sb->rcv.wnd - sb->rcv.buf <= msgsz)
	events |= XMQ_FULL;
    events |= XMQ_PUSH;
    sb->rcv.buf += msgsz;
    list_add_tail(&msg->item, &sb->rcv.head);    
    DEBUG_OFF("%d", sb->fd);

    /* Wakeup the blocking waiters. */
    if (sb->rcv.waiters > 0)
	condition_broadcast(&sb->cond);

    if (events && vfptr->notify)
	vfptr->notify(sb, RECV_Q, events);

    __emit_pollevents(sb);
    mutex_unlock(&sb->lock);
}

int xrecv(int fd, char **ubuf) {
    int rc = 0;
    struct xmsg *msg = 0;
    struct sockbase *sb;
    
    if (!ubuf) {
	errno = EINVAL;
	return -1;
    }
    if (!(sb = xget(fd))) {
	errno = EBADF;
	return -1;
    }
    if (!(msg = recvq_pop(sb))) {
	errno = sb->fepipe ? EPIPE : EAGAIN;
	rc = -1;
    } else {
	*ubuf = msg->vec.xiov_base;
    }
    xput(fd);
    return rc;
}



