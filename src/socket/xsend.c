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
#include <utils/waitgroup.h>
#include <utils/taskpool.h>
#include "global.h"

struct msgbuf *snd_msgbuf_head_rm (struct sockbase *sb) {
	int rc;
	struct sockbase_vfptr *vfptr = sb->vfptr;
	struct msgbuf *msg = 0;
	u32 events = 0;

	mutex_lock (&sb->lock);
	if ((rc = msgbuf_head_out_msg (&sb->snd, &msg)) == 0) {
		events |= XMQ_POP;

		/* the first time when msgbuf_head is non-full */
		if (sb->snd.wnd - sb->snd.size <= msgbuf_len (msg))
			events |= XMQ_NONFULL;
		if (msgbuf_head_empty (&sb->snd))
			events |= XMQ_EMPTY;

		/* Wakeup the blocking waiters */
		if (sb->snd.waiters > 0)
			condition_broadcast (&sb->cond);
	}

	if (events && vfptr->notify)
		vfptr->notify (sb, SEND_Q, events);

	__emit_pollevents (sb);
	mutex_unlock (&sb->lock);
	return msg;
}

int snd_msgbuf_head_add (struct sockbase *sb, struct msgbuf *msg)
{
	struct sockbase_vfptr *vfptr = sb->vfptr;
	int rc = -1;
	u32 events = 0;

	mutex_lock (&sb->lock);
	while (!sb->fepipe && !msgbuf_can_in (&sb->snd) && !sb->fasync) {
		sb->snd.waiters++;
		condition_wait (&sb->cond, &sb->lock);
		sb->snd.waiters--;
	}
	if (msgbuf_can_in (&sb->snd) ) {
		rc = 0;
		if (msgbuf_head_empty (&sb->snd))
			events |= XMQ_NONEMPTY;

		/* the first time when msgbuf_head is full */
		if (sb->snd.wnd - sb->snd.size <= msgbuf_len (msg))
			events |= XMQ_FULL;
		events |= XMQ_PUSH;
		msgbuf_head_in_msg (&sb->snd, msg);
	}

	if (events && vfptr->notify)
		vfptr->notify (sb, SEND_Q, events);

	__emit_pollevents (sb);
	mutex_unlock (&sb->lock);
	return rc;
}

int xsend (int fd, char *ubuf)
{
	int rc = 0;
	struct msgbuf *msg = 0;
	struct sockbase *sb;

	if (!ubuf) {
		errno = EINVAL;
		return -1;
	}
	if (! (sb = xget (fd) ) ) {
		errno = EBADF;
		return -1;
	}
	msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);
	if ( (rc = snd_msgbuf_head_add (sb, msg) ) < 0) {
		errno = sb->fepipe ? EPIPE : EAGAIN;
	}
	xput (fd);
	return rc;
}

