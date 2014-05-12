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

void xpollbase_emit(struct pollbase *pb, u32 events) {
    struct xpitem *itm = cont_of(pb, struct xpitem, base);
    struct xpoll_t *self = itm->poll;

    mutex_lock(&self->lock);
    BUG_ON(!pb->event.care);
    if (!attached(&itm->lru_link)) {
	list_del_init(&itm->base.link);
	mutex_unlock(&self->lock);
	xpitem_put(itm);
	return;
    }
    spin_lock(&itm->lock);
    if (events) {
	DEBUG_OFF("xsock %d update events %s", pb->event.fd, xpoll_str[events]);
	pb->event.happened = events;
	list_move(&itm->lru_link, &self->lru_head);
	if (self->uwaiters)
	    condition_broadcast(&self->cond);
    } else if (pb->event.happened && !events) {
	DEBUG_OFF("xsock %d disable events notify", pb->event.fd);
	pb->event.happened = 0;
	list_move_tail(&itm->lru_link, &self->lru_head);
    }
    spin_unlock(&itm->lock);
    mutex_unlock(&self->lock);
}

void xpollbase_close(struct pollbase *pb) {
    struct xpitem *itm = cont_of(pb, struct xpitem, base);
    struct xpoll_t *self = itm->poll;

    xpoll_ctl(self->id, XPOLL_DEL, &pb->event);
    xpitem_put(itm);
}

struct pollbase_vfptr xpollbase_vfptr = {
    .emit = xpollbase_emit,
    .close = xpollbase_close,
};


int xpoll_create() {
    struct xpoll_t *self = poll_alloc();

    if (self) {
	pget(self->id);
    }
    return self->id;
}

extern void emit_pollevents(struct sockbase *sb);

static int xpoll_add(struct xpoll_t *self, struct xpoll_event *event) {
    struct xpitem *itm = xpoll_getitm(self, event->fd);
    struct sockbase *sb = xget(event->fd);

    if (!itm)
	return -1;
    if (!sb) {
	xpitem_put(itm);
	errno = EBADF;
	return -1;
    }

    /* Set up events callback */
    spin_lock(&itm->lock);
    itm->base.event.fd = event->fd;
    itm->base.event.care = event->care;
    itm->base.event.self = event->self;
    itm->poll = self;
    spin_unlock(&itm->lock);

    /* We hold a ref here. it is used for xsock */
    /* BUG case 1: it's possible that this entry was deleted by xpoll_rm() */
    add_pollbase(sb->fd, &itm->base);

    emit_pollevents(sb);
    xput(sb->fd);
    return 0;
}

static int xpoll_rm(struct xpoll_t *self, struct xpoll_event *event) {
    struct xpitem *itm = xpoll_putitm(self, event->fd);

    if (!itm)
	return -1;

    /* Release the ref hold by xpoll_t */
    xpitem_put(itm);
    return 0;
}


static int xpoll_mod(struct xpoll_t *self, struct xpoll_event *event) {
    struct sockbase *sb = xget(event->fd);
    struct xpitem *itm = xpoll_find(self, event->fd);

    if (!itm)
	return -1;
    if (!sb) {
	xpitem_put(itm);
	errno = EBADF;
	return -1;
    }

    mutex_lock(&self->lock);
    spin_lock(&itm->lock);
    itm->base.event.care = event->care;
    spin_unlock(&itm->lock);
    mutex_unlock(&self->lock);

    emit_pollevents(sb);

    /* Release the ref hold by caller */
    xpitem_put(itm);
    return 0;
}

int xpoll_ctl(int pollid, int op, struct xpoll_event *event) {
    struct xpoll_t *self = pget(pollid);
    int rc = 0;

    if (!self) {
	errno = EBADF;
	return -1;
    }
    switch (op) {
    case XPOLL_ADD:
	rc = xpoll_add(self, event);
	break;
    case XPOLL_DEL:
	rc = xpoll_rm(self, event);
	break;
    case XPOLL_MOD:
	rc = xpoll_mod(self, event);
	break;
    default:
	errno = EINVAL;
	rc = -1;
    }
    pput(pollid);
    return rc;
}

int xpoll_wait(int pollid, struct xpoll_event *ev_buf, int size, int timeout) {
    struct xpoll_t *self = pget(pollid);
    int n = 0;
    struct xpitem *itm, *nitm;

    if (!self) {
	errno = EBADF;
	return -1;
    }
    mutex_lock(&self->lock);
    /* If havn't any events here. we wait */
    itm = list_first(&self->lru_head, struct xpitem, lru_link);
    if (!itm->base.event.happened && timeout > 0) {
	self->uwaiters++;
	condition_timedwait(&self->cond, &self->lock, timeout);
	self->uwaiters--;
    }
    walk_xpitem_safe(itm, nitm, &self->lru_head) {
	if (!itm->base.event.happened || n >= size)
	    break;
	ev_buf[n++] = itm->base.event;
    }
    mutex_unlock(&self->lock);
    pput(pollid);
    return n;
}


int xpoll_close(int pollid) {
    struct xpoll_t *self = pget(pollid);
    struct xpitem *itm;

    if (!self) {
	errno = EBADF;
	return -1;
    }
    while ((itm = xpoll_popitm(self))) {
	/* Release the ref hold by xpoll_t */
	xpitem_put(itm);
    }
    /* Release the ref hold by user-caller */
    pput(pollid);
    pput(pollid);
    return 0;
}
