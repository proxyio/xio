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

const char *pf_str[] = {
	"",
	"PF_TCP",
	"PF_IPC",
	"PF_TCP|PF_IPC",
	"PF_INPROC",
	"PF_TCP|PF_INPROC",
	"PF_IPC|PF_INPROC",
	"PF_TCP|PF_IPC|PF_INPROC",
};

/* Default snd/rcv buffer size */
int default_sndbuf = 10485760;
int default_rcvbuf = 10485760;

int xalloc (int family, int socktype)
{
	struct sockbase_vfptr *vfptr = sockbase_vfptr_lookup (family, socktype);
	struct sockbase *sb;

	if (!vfptr) {
		errno = EPROTO;
		return -1;
	}
	BUG_ON (!vfptr->alloc);
	if (! (sb = vfptr->alloc() ) ) {
		return -1;
	}
	sb->vfptr = vfptr;
	mutex_lock (&xgb.lock);
	BUG_ON (xgb.nsockbases >= PROXYIO_MAX_SOCKS);
	sb->fd = xgb.unused[xgb.nsockbases++];
	xgb.sockbases[sb->fd] = sb;
	atomic_incr (&sb->ref);
	mutex_unlock (&xgb.lock);
	BUG_ON (atomic_fetch (&sb->ref) != 1);
	DEBUG_OFF ("sock %d alloc %s", sb->fd, pf_str[sb->vfptr->pf]);
	return sb->fd;
}

struct sockbase *xget (int fd) {
	struct sockbase *sb;
	mutex_lock (&xgb.lock);
	if (! (sb = xgb.sockbases[fd]) ) {
		mutex_unlock (&xgb.lock);
		return 0;
	}
	BUG_ON (!atomic_fetch (&sb->ref) );
	atomic_incr (&sb->ref);
	mutex_unlock (&xgb.lock);
	return sb;
}

void xput (int fd)
{
	struct sockbase *sb = xgb.sockbases[fd];

	mutex_lock (&xgb.lock);
	if (atomic_decr (&sb->ref) == 1) {
		xgb.sockbases[sb->fd] = 0;
		xgb.unused[--xgb.nsockbases] = sb->fd;
		DEBUG_OFF ("sock %d shutdown %s", sb->fd, pf_str[sb->vfptr->pf]);
		sb->vfptr->close (sb);
	}
	mutex_unlock (&xgb.lock);
}

void sockbase_init (struct sockbase *sb)
{
	mutex_init (&sb->lock);
	condition_init (&sb->cond);
	ZERO (sb->addr);
	ZERO (sb->peer);
	sb->fasync = false;
	sb->fepipe = false;
	sb->owner = 0;
	INIT_LIST_HEAD (&sb->sub_socks);
	INIT_LIST_HEAD (&sb->sib_link);

	atomic_init (&sb->ref);
	sb->evl = ev_get_loop (rand ());
	socket_mstats_init (&sb->stats);

	msgbuf_head_init (&sb->rcv, default_rcvbuf);
	msgbuf_head_init (&sb->evl_rcv, default_rcvbuf);
	msgbuf_head_init (&sb->snd, default_sndbuf);

	INIT_LIST_HEAD (&sb->poll_entries);
	condition_init (&sb->acceptq.cond);
	sb->acceptq.waiters = 0;
	INIT_LIST_HEAD (&sb->acceptq.head);
	INIT_LIST_HEAD (&sb->acceptq.link);
}

void sockbase_exit (struct sockbase *sb)
{
	struct list_head head = {};
	struct msgbuf *msg, *tmp;

	mutex_destroy (&sb->lock);
	condition_destroy (&sb->cond);
	ZERO (sb->addr);
	ZERO (sb->peer);
	sb->fasync = 0;
	sb->fepipe = 0;
	sb->owner = 0;
	BUG_ON (!list_empty (&sb->sub_socks) );
	BUG_ON (attached (&sb->sib_link) );
	sb->fd = -1;
	INIT_LIST_HEAD (&head);

	msgbuf_dequeue_all (&sb->rcv, &head);
	msgbuf_dequeue_all (&sb->snd, &head);

	walk_msg_s (msg, tmp, &head)
		msgbuf_free (msg);

	/* It's possible that user call xclose() and xpoll_add()
	 * at the same time. and attach_to_sock() happen after xclose().
	 * this is a user's bug.
	 */
	BUG_ON (!list_empty (&sb->poll_entries) );
	sb->acceptq.waiters = -1;
	condition_destroy (&sb->acceptq.cond);
	atomic_destroy (&sb->ref);
}
