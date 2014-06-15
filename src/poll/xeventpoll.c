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
	struct xpitem *itm = cont_of (pb, struct xpitem, base);
	struct xpoll_t *self = itm->poll;

	mutex_lock (&self->lock);
	BUG_ON (!pb->ent.events);
	if (!attached (&itm->lru_link) ) {
		list_del_init (&itm->base.link);
		mutex_unlock (&self->lock);
		xpitem_put (itm);
		return;
	}
	spin_lock (&itm->lock);
	if (events) {
		DEBUG_OFF ("socket %d update events %s", pb->event.fd, event_str[events]);
		pb->ent.happened = events;
		list_move (&itm->lru_link, &self->lru_head);
		if (self->uwaiters)
			condition_broadcast (&self->cond);
	} else if (pb->ent.happened && !events) {
		DEBUG_OFF ("socket %d disable events notify", pb->event.fd);
		pb->ent.happened = 0;
		list_move_tail (&itm->lru_link, &self->lru_head);
	}
	spin_unlock (&itm->lock);
	mutex_unlock (&self->lock);
}

void xpollbase_close (struct pollbase *pb)
{
	struct xpitem *itm = cont_of (pb, struct xpitem, base);
	struct xpoll_t *self = itm->poll;

	xpoll_ctl (self->id, XPOLL_DEL, &pb->ent);
	xpitem_put (itm);
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

static int xpoll_add (struct xpoll_t *self, struct poll_ent *ent)
{
	int rc;
	struct sockbase *sb = xget (ent->fd);
	struct xpitem *itm;

	if (!sb) {
		errno = EBADF;
		return -1;
	}
	if (! (itm = addfd (self, ent->fd) ) ) {
		xput (sb->fd);
		return -1;
	}

	spin_lock (&itm->lock);
	itm->base.ent = *ent;
	itm->poll = self;
	spin_unlock (&itm->lock);

	/* BUG: condition race with xpoll_rm() */
	if ( (rc = add_pollbase (sb->fd, &itm->base) ) == 0) {
		emit_pollevents (sb);
	} else
		rmfd (self, ent->fd);
	xput (sb->fd);
	return rc;
}

static int xpoll_rm (struct xpoll_t *self, struct poll_ent *ent)
{
	int rc = rmfd (self, ent->fd);
	return rc;
}


static int xpoll_mod (struct xpoll_t *self, struct poll_ent *ent)
{
	struct sockbase *sb = xget (ent->fd);
	struct xpitem *itm;

	if (!sb) {
		errno = EBADF;
		return -1;
	}
	if (! (itm = getfd (self, ent->fd) ) ) {
		xput (sb->fd);
		errno = ENOENT;
		return -1;
	}

	mutex_lock (&self->lock);
	spin_lock (&itm->lock);
	/* WANRING: only update events events here. */
	itm->base.ent.events = ent->events;
	spin_unlock (&itm->lock);
	mutex_unlock (&self->lock);

	emit_pollevents (sb);

	/* Release the ref hold by caller */
	xput (sb->fd);
	xpitem_put (itm);
	return 0;
}

typedef int (*poll_ctl) (struct xpoll_t *self, struct poll_ent *ent);

const poll_ctl ctl_vfptr[] = {
	0,
	xpoll_add,
	xpoll_rm,
	xpoll_mod,
};

int xpoll_ctl (int pollid, int op, struct poll_ent *ent)
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
	rc = ctl_vfptr[op] (self, ent);
	pput (pollid);
	return rc;
}

int xpoll_wait (int pollid, struct poll_ent *ents, int size, int to)
{
	struct xpoll_t *self = pget (pollid);
	int n = 0;
	struct xpitem *itm, *nitm;

	if (!self) {
		errno = EBADF;
		return -1;
	}
	mutex_lock (&self->lock);

	/* WAITING any events happened */
	itm = list_first (&self->lru_head, struct xpitem, lru_link);
	if (!itm->base.ent.happened && to > 0) {
		self->uwaiters++;
		condition_timedwait (&self->cond, &self->lock, to);
		self->uwaiters--;
	}
	walk_xpitem_s (itm, nitm, &self->lru_head) {
		if (!itm->base.ent.happened || n >= size)
			break;
		ents[n++] = itm->base.ent;
		if (itm->base.ent.happened & XPOLLIN)
			poll_mstats_incr (&self->stats, ST_POLLIN);
		if (itm->base.ent.happened & XPOLLOUT)
			poll_mstats_incr (&self->stats, ST_POLLOUT);
		if (itm->base.ent.happened & XPOLLERR)
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

	while ( (rc = rmfd (self, XPOLL_HEADFD) ) == 0) {
	}

	/* Release the ref hold by user-caller */
	pput (pollid);
	pput (pollid);
	return 0;
}
