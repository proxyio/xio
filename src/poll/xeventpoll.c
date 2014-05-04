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

#include <os/timesz.h>
#include <base.h>
#include "xeventpoll.h"

const char *xpoll_str[] = {
    "",
    "XPOLLIN",
    "XPOLLOUT",
    "XPOLLIN|XPOLLOUT",
    "XPOLLERR",
    "XPOLLIN|XPOLLERR",
    "XPOLLOUT|XPOLLERR",
    "XPOLLIN|XPOLLOUT|XPOLLERR",
};


static void
event_notify(struct xpoll_notify *un, struct xpoll_entry *ent, u32 ev);

struct xpoll_t *xpoll_create() {
    struct xpoll_t *self = xpoll_new();

    if (self) {
	self->notify.event = event_notify;
	xpoll_get(self);
    }
    return self;
}

static void
event_notify(struct xpoll_notify *un, struct xpoll_entry *ent, u32 ev) {
    struct xpoll_t *self = cont_of(un, struct xpoll_t, notify);

    mutex_lock(&self->lock);
    BUG_ON(!ent->event.care);
    if (!attached(&ent->lru_link)) {
	__detach_from_xsock(ent);
	mutex_unlock(&self->lock);
	xent_put(ent);
	return;
    }
    spin_lock(&ent->lock);
    if (ev) {
	DEBUG_OFF("xsock %d update events %s", ent->event.xd, xpoll_str[ev]);
	ent->event.happened = ev;
	list_move(&ent->lru_link, &self->lru_head);
	if (self->uwaiters)
	    condition_broadcast(&self->cond);
    } else if (ent->event.happened && !ev) {
	DEBUG_OFF("xsock %d disable events notify", ent->event.xd);
	ent->event.happened = 0;
	list_move_tail(&ent->lru_link, &self->lru_head);
    }
    spin_unlock(&ent->lock);
    mutex_unlock(&self->lock);
}

extern void xeventnotify(struct xsock *sk);

static int xpoll_add(struct xpoll_t *self, struct xpoll_event *event) {
    struct xpoll_entry *ent = xpoll_getent(self, event->xd);
    struct xsock *sk = xget(event->xd);

    if (!ent)
	return -1;

    /* Set up events callback */
    spin_lock(&ent->lock);
    ent->event.xd = event->xd;
    ent->event.care = event->care;
    ent->event.self = event->self;
    ent->notify = &self->notify;
    spin_unlock(&ent->lock);

    /* We hold a ref here. it is used for xsock */
    /* BUG case 1: it's possible that this entry was deleted by xpoll_rm() */
    attach_to_xsock(ent, sk->fd);

    xeventnotify(sk);
    return 0;
}

static int xpoll_rm(struct xpoll_t *self, struct xpoll_event *event) {
    struct xpoll_entry *ent = xpoll_putent(self, event->xd);

    if (!ent) {
	return -1;
    }

    /* Release the ref hold by xpoll_t */
    xent_put(ent);
    return 0;
}


static int xpoll_mod(struct xpoll_t *self, struct xpoll_event *event) {
    struct xsock *sk = xget(event->xd);
    struct xpoll_entry *ent = xpoll_find(self, event->xd);

    if (!ent)
	return -1;
    mutex_lock(&self->lock);
    spin_lock(&ent->lock);
    ent->event.care = event->care;
    spin_unlock(&ent->lock);
    mutex_unlock(&self->lock);

    xeventnotify(sk);

    /* Release the ref hold by caller */
    xent_put(ent);
    return 0;
}

int xpoll_ctl(struct xpoll_t *self, int op, struct xpoll_event *event) {
    int rc = -1;

    switch (op) {
    case XPOLL_ADD:
	rc = xpoll_add(self, event);
	return rc;
    case XPOLL_DEL:
	rc = xpoll_rm(self, event);
	return rc;
    case XPOLL_MOD:
	rc = xpoll_mod(self, event);
	return rc;
    default:
	errno = EINVAL;
    }
    return -1;
}

int xpoll_wait(struct xpoll_t *self, struct xpoll_event *ev_buf,
	       int size, int timeout) {
    int n = 0;
    struct xpoll_entry *ent, *nx;

    mutex_lock(&self->lock);

    /* If havn't any events here. we wait */
    ent = list_first(&self->lru_head, struct xpoll_entry, lru_link);
    if (!ent->event.happened && timeout > 0) {
	self->uwaiters++;
	condition_timedwait(&self->cond, &self->lock, timeout);
	self->uwaiters--;
    }
    xpoll_walk_ent(ent, nx, &self->lru_head) {
	if (!ent->event.happened || n >= size)
	    break;
	ev_buf[n++] = ent->event;
    }
    mutex_unlock(&self->lock);
    return n;
}


void xpoll_close(struct xpoll_t *self) {
    struct xpoll_entry *ent;

    while ((ent = xpoll_popent(self))) {
	/* Release the ref hold by xpoll_t */
	xent_put(ent);
    }
    /* Release the ref hold by user-caller */
    xpoll_put(self);
}
