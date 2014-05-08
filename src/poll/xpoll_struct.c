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
#include <socket/xgb.h>
#include "xeventpoll.h"

struct xpoll_entry *xent_new() {
    struct xpoll_entry *ent = (struct xpoll_entry *)mem_zalloc(sizeof(*ent));
    if (ent) {
	INIT_LIST_HEAD(&ent->xlink);
	INIT_LIST_HEAD(&ent->lru_link);
	spin_init(&ent->lock);
	ent->ref = 0;
    }
    return ent;
}

int xpoll_put(struct xpoll_t *self);

static void xent_destroy(struct xpoll_entry *ent) {
    struct xpoll_t *self = cont_of(ent->notify, struct xpoll_t, notify);

    BUG_ON(ent->ref != 0);
    spin_destroy(&ent->lock);
    mem_free(ent, sizeof(*ent));
    xpoll_put(self);
}


int xent_get(struct xpoll_entry *ent) {
    int ref;
    spin_lock(&ent->lock);
    ref = ent->ref++;
    /* open for debuging
     * if (ent->i_idx < sizeof(ent->incr_tid))
     * ent->incr_tid[ent->i_idx++] = gettid();
     */
    spin_unlock(&ent->lock);
    return ref;
}

int xent_put(struct xpoll_entry *ent) {
    int ref;

    spin_lock(&ent->lock);
    ref = ent->ref--;
    BUG_ON(ent->ref < 0);
    /* open for debuging
     * if (ent->d_idx < sizeof(ent->desc_tid))
     * ent->desc_tid[ent->d_idx++] = gettid();
     */
    spin_unlock(&ent->lock);
    if (ref == 1) {
	BUG_ON(attached(&ent->lru_link));
	BUG_ON(attached(&ent->xlink));
	xent_destroy(ent);
    }
    return ref;
}

struct xpoll_t *xpoll_new() {
    struct xpoll_t *self = (struct xpoll_t *)mem_zalloc(sizeof(*self));
    if (self) {
	self->uwaiters = 0;
	self->ref = 0;
	self->size = 0;
	mutex_init(&self->lock);
	condition_init(&self->cond);
	INIT_LIST_HEAD(&self->lru_head);
    }
    return self;
}

static void xpoll_destroy(struct xpoll_t *self) {
    DEBUG_OFF();
    mutex_destroy(&self->lock);
    mem_free(self, sizeof(struct xpoll_t));
}

int xpoll_get(struct xpoll_t *self) {
    int ref;
    mutex_lock(&self->lock);
    ref = self->ref++;
    mutex_unlock(&self->lock);
    return ref;
}

int xpoll_put(struct xpoll_t *self) {
    int ref;
    mutex_lock(&self->lock);
    ref = self->ref--;
    BUG_ON(self->ref < 0);
    mutex_unlock(&self->lock);
    if (ref == 1)
	xpoll_destroy(self);
    return ref;
}

struct xpoll_entry *__xpoll_find(struct xpoll_t *self, int fd) {
    struct xpoll_entry *ent, *nx;
    xpoll_walk_ent(ent, nx, &self->lru_head) {
	if (ent->event.xd == fd)
	    return ent;
    }
    return 0;
}

/* Find xpoll_entry by xsock id and return with ref incr if exist. */
struct xpoll_entry *xpoll_find(struct xpoll_t *self, int fd) {
    struct xpoll_entry *ent = 0;
    mutex_lock(&self->lock);
    if ((ent = __xpoll_find(self, fd)))
	xent_get(ent);
    mutex_unlock(&self->lock);
    return ent;
}


void __attach_to_po(struct xpoll_entry *ent, struct xpoll_t *self) {
    BUG_ON(attached(&ent->lru_link));
    self->size++;
    list_add_tail(&ent->lru_link, &self->lru_head);
}

void __detach_from_po(struct xpoll_entry *ent, struct xpoll_t *self) {
    BUG_ON(!attached(&ent->lru_link));
    self->size--;
    list_del_init(&ent->lru_link);
}


/* Create a new xpoll_entry if the fd doesn't exist and get one ref for
 * caller. xpoll_add() call this.
 */
struct xpoll_entry *xpoll_getent(struct xpoll_t *self, int fd) {
    struct xpoll_entry *ent;

    mutex_lock(&self->lock);
    if ((ent = __xpoll_find(self, fd))) {
	mutex_unlock(&self->lock);
	errno = EEXIST;
	return 0;
    }
    if (!(ent = xent_new())) {
	mutex_unlock(&self->lock);
	errno = ENOMEM;
	return 0;
    }

    /* One reference for back for caller */
    ent->ref++;

    /* Cycle reference of xpoll_t and xpoll_entry */
    ent->ref++;
    self->ref++;

    ent->event.xd = fd;
    __attach_to_po(ent, self);
    mutex_unlock(&self->lock);

    return ent;
}

/* Remove the xpoll_entry if the fd's ent exist. notice that don't release the
 * ref hold by xpoll_t. let caller do this.
 * xpoll_rm() call this.
 */
struct xpoll_entry *xpoll_putent(struct xpoll_t *self, int fd) {
    struct xpoll_entry *ent;

    mutex_lock(&self->lock);
    if (!(ent = __xpoll_find(self, fd))) {
	mutex_unlock(&self->lock);
	errno = ENOENT;
	return 0;
    }
    __detach_from_po(ent, self);
    mutex_unlock(&self->lock);

    return ent;
}


/* Pop the first xpoll_entry. notice that don't release the ref hold
 * by xpoll_t. let caller do this.
 * xpoll_close() call this.
 */
struct xpoll_entry *xpoll_popent(struct xpoll_t *self) {
    struct xpoll_entry *ent;

    mutex_lock(&self->lock);
    if (list_empty(&self->lru_head)) {
	mutex_unlock(&self->lock);
	errno = ENOENT;
	return 0;
    }
    ent = list_first(&self->lru_head, struct xpoll_entry, lru_link);
    __detach_from_po(ent, self);
    mutex_unlock(&self->lock);

    return ent;
}


void attach_to_xsock(struct xpoll_entry *ent, int fd) {
    struct sockbase *cn = xgb.sockbases[fd];

    mutex_lock(&cn->lock);
    BUG_ON(attached(&ent->xlink));
    list_add_tail(&ent->xlink, &cn->poll_entries);
    mutex_unlock(&cn->lock);
}


void __detach_from_xsock(struct xpoll_entry *ent) {
    BUG_ON(!attached(&ent->xlink));
    list_del_init(&ent->xlink);
}
