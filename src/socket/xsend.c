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

struct xmsg *sendq_pop(struct sockbase *sb) {
    struct sockbase_vfptr *vfptr = sb->vfptr;
    struct xmsg *msg = 0;
    i64 msgsz;
    u32 events = 0;
    
    mutex_lock(&sb->lock);
    if (!list_empty(&sb->snd.head)) {
	DEBUG_OFF("xsock %d", sb->fd);
	msg = list_first(&sb->snd.head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	sb->snd.buf -= msgsz;
	events |= XMQ_POP;
	if (sb->snd.wnd - sb->snd.buf <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&sb->snd.head)) {
	    BUG_ON(sb->snd.buf);
	    events |= XMQ_EMPTY;
	}

	/* Wakeup the blocking waiters */
	if (sb->snd.waiters > 0)
	    condition_broadcast(&sb->cond);
    }

    if (events && vfptr->notify)
	vfptr->notify(sb, SEND_Q, events);

    __emit_pollevents(sb);
    mutex_unlock(&sb->lock);
    return msg;
}

int sendq_push(struct sockbase *sb, struct xmsg *msg) {
    int rc = -1;
    struct sockbase_vfptr *vfptr = sb->vfptr;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&sb->lock);
    while (!sb->fepipe && !can_send(sb) && !sb->fasync) {
	sb->snd.waiters++;
	condition_wait(&sb->cond, &sb->lock);
	sb->snd.waiters--;
    }
    if (can_send(sb)) {
	rc = 0;
	if (list_empty(&sb->snd.head))
	    events |= XMQ_NONEMPTY;
	if (sb->snd.wnd - sb->snd.buf <= msgsz)
	    events |= XMQ_FULL;
	events |= XMQ_PUSH;
	sb->snd.buf += msgsz;
	list_add_tail(&msg->item, &sb->snd.head);
	DEBUG_OFF("xsock %d", sb->fd);
    }

    if (events && vfptr->notify)
	vfptr->notify(sb, SEND_Q, events);

    __emit_pollevents(sb);
    mutex_unlock(&sb->lock);
    return rc;
}

int xsend(int fd, char *ubuf) {
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
    msg = cont_of(ubuf, struct xmsg, vec.chunk);
    if ((rc = sendq_push(sb, msg)) < 0) {
	errno = sb->fepipe ? EPIPE : EAGAIN;
    }
    xput(fd);
    return rc;
}

