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

#include <utils/timer.h>
#include <utils/base.h>
#include "xeventpoll.h"

const char *event_str[] = {
	"",
	"XPOLLIN",
	"XPOLLOUT",
	"XPOLLIN|XPOLLOUT",
	"XPOLLERR",
	"XPOLLIN|XPOLLERR",
	"XPOLLOUT|XPOLLERR",
	"XPOLLIN|XPOLLOUT|XPOLLERR",
};

void xpollbase_emit (struct pollbase *pb, u32 events)
{
	struct poll_entry *ent = cont_of (pb, struct poll_entry, base);
	struct xpoll_t *self = ent->owner;

	mutex_lock (&self->lock);
	BUG_ON (!pb->pollfd.events);
	if (!attached (&ent->lru_link) ) {
		list_del_init (&ent->base.link);
		mutex_unlock (&self->lock);
		poll_entry_put (ent);
		return;
	}
	spin_lock (&ent->lock);
	if (events) {
		DEBUG_OFF ("socket %d update events %s", pb->event.fd, event_str[events]);
		pb->pollfd.happened = events;
		list_move (&ent->lru_link, &self->lru_head);
		if (self->uwaiters)
			condition_broadcast (&self->cond);
	} else if (pb->pollfd.happened && !events) {
		DEBUG_OFF ("socket %d disable events notify", pb->event.fd);
		pb->pollfd.happened = 0;
		list_move_tail (&ent->lru_link, &self->lru_head);
	}
	spin_unlock (&ent->lock);
	mutex_unlock (&self->lock);
}

void xpollbase_close (struct pollbase *pb)
{
	struct poll_entry *ent = cont_of (pb, struct poll_entry, base);
	struct xpoll_t *self = ent->owner;

	xpoll_ctl (self->id, XPOLL_DEL, &pb->pollfd);
	poll_entry_put (ent);
}

struct pollbase_vfptr xpollbase_vfptr = {
	.emit = xpollbase_emit,
	.close = xpollbase_close,
};


int xpoll_create()
{
	struct xpoll_t *self = poll_alloc();

	if (self) {
		pget (self->id);
		poll_mstats_init (&self->stats);
	}
	return self->id;
}

extern void emit_pollevents (struct sockbase *sb);

static int xpoll_add (struct xpoll_t *self, struct poll_fd *pollfd)
{
	int rc;
	struct sockbase *sb = xget (pollfd->fd);
	struct poll_entry *ent;

	if (!sb) {
		errno = EBADF;
		return -1;
	}
	if (! (ent = add_poll_entry (self, pollfd->fd) ) ) {
		xput (sb->fd);
		return -1;
	}

	spin_lock (&ent->lock);
	ent->base.pollfd = *pollfd;
	ent->owner = self;
	spin_unlock (&ent->lock);

	/* BUG: condition race with xpoll_rm() */
	if ( (rc = add_pollbase (sb->fd, &ent->base) ) == 0) {
		emit_pollevents (sb);
	} else
		rm_poll_entry (self, pollfd->fd);
	xput (sb->fd);
	return rc;
}

static int xpoll_rm (struct xpoll_t *self, struct poll_fd *pollfd)
{
	int rc = rm_poll_entry (self, pollfd->fd);
	return rc;
}


static int xpoll_mod (struct xpoll_t *self, struct poll_fd *pollfd)
{
	struct sockbase *sb = xget (pollfd->fd);
	struct poll_entry *ent;

	if (!sb) {
		errno = EBADF;
		return -1;
	}
	if (! (ent = get_poll_entry (self, pollfd->fd) ) ) {
		xput (sb->fd);
		errno = ENOENT;
		return -1;
	}

	mutex_lock (&self->lock);
	spin_lock (&ent->lock);
	/* WANRING: only update events events here. */
	ent->base.pollfd.events = pollfd->events;
	spin_unlock (&ent->lock);
	mutex_unlock (&self->lock);

	emit_pollevents (sb);

	/* Release the ref hold by caller */
	xput (sb->fd);
	poll_entry_put (ent);
	return 0;
}

typedef int (*poll_ctl) (struct xpoll_t *self, struct poll_fd *pollfd);

const poll_ctl ctl_vfptr[] = {
	0,
	xpoll_add,
	xpoll_rm,
	xpoll_mod,
};

int xpoll_ctl (int pollid, int op, struct poll_fd *pollfd)
{
	struct xpoll_t *self = pget (pollid);
	int rc;

	if (!self) {
		errno = EBADF;
		return -1;
	}
	if (op >= NELEM (ctl_vfptr, poll_ctl) || !ctl_vfptr[op]) {
		pput (pollid);
		errno = EINVAL;
		return -1;
	}
	rc = ctl_vfptr[op] (self, pollfd);
	pput (pollid);
	return rc;
}

int xpoll_wait (int pollid, struct poll_fd *pollfds, int size, int to)
{
	struct xpoll_t *self = pget (pollid);
	int n = 0;
	struct poll_entry *ent, *tmp;

	if (!self) {
		errno = EBADF;
		return -1;
	}
	mutex_lock (&self->lock);

	/* WAITING any events happened */
	ent = list_first (&self->lru_head, struct poll_entry, lru_link);
	if (!ent->base.pollfd.happened && to > 0) {
		self->uwaiters++;
		condition_timedwait (&self->cond, &self->lock, to);
		self->uwaiters--;
	}
	walk_poll_entry_s (ent, tmp, &self->lru_head) {
		if (!ent->base.pollfd.happened || n >= size)
			break;
		pollfds[n++] = ent->base.pollfd;
		if (ent->base.pollfd.happened & XPOLLIN)
			poll_mstats_incr (&self->stats, ST_POLLIN);
		if (ent->base.pollfd.happened & XPOLLOUT)
			poll_mstats_incr (&self->stats, ST_POLLOUT);
		if (ent->base.pollfd.happened & XPOLLERR)
			poll_mstats_incr (&self->stats, ST_POLLERR);
	}
	mutex_unlock (&self->lock);
	pput (pollid);
	return n;
}


int xpoll_close (int pollid)
{
	int rc;
	struct xpoll_t *self = pget (pollid);

	if (!self) {
		errno = EBADF;
		return -1;
	}
	mutex_lock (&self->lock);
	self->shutdown_state = true;
	mutex_unlock (&self->lock);

	while ( (rc = rm_poll_entry (self, XPOLL_HEADFD) ) == 0) {
	}

	/* Release the ref hold by user-caller */
	pput (pollid);
	pput (pollid);
	return 0;
}
