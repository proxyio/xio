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
#include "xg.h"

struct msgbuf *snd_msgbuf_head_rm (struct sockbase *sb) {
	int rc;
	struct sockbase_vfptr *vfptr = sb->vfptr;
	struct msgbuf *msg = 0;

	mutex_lock (&sb->lock);
	if ((rc = msgbuf_head_out_msg (&sb->snd, &msg)) == 0) {
		if (sb->snd.waiters > 0)
			condition_broadcast (&sb->cond);
	}
	__emit_pollevents (sb);
	mutex_unlock (&sb->lock);
	return msg;
}

int snd_msgbuf_head_add (struct sockbase *sb, struct msgbuf *msg)
{
	struct sockbase_vfptr *vfptr = sb->vfptr;
	int rc = -1;

	mutex_lock (&sb->lock);
	while (!sb->fepipe && !msgbuf_can_in (&sb->snd) && !sb->fasync) {
		sb->snd.waiters++;
		condition_wait (&sb->cond, &sb->lock);
		sb->snd.waiters--;
	}
	if (msgbuf_can_in (&sb->snd) ) {
		rc = 0;
		msgbuf_head_in_msg (&sb->snd, msg);
	}
	__emit_pollevents (sb);
	mutex_unlock (&sb->lock);
	return rc;
}

int xsend (int fd, char *ubuf)
{
	int rc = 0;
	struct sockbase *sb;

	BUG_ON (!ubuf);
	if (!(sb = xget (fd))) {
		errno = EBADF;
		return -1;
	}
	if (!sb->vfptr->send) {
		xput (fd);
		errno = EINVAL;
		return -1;
	}
	rc = sb->vfptr->send (sb, ubuf);
	xput (fd);
	return rc;
}

