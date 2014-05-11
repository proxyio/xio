/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to who m
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

struct xp_global pg;


void xpoll_module_init() {
    int pollid;
    
    spin_init(&pg.lock);
    for (pollid = 0; pollid < XIO_MAX_POLLS; pollid++)
	pg.unused[pollid] = pollid;
}

void xpoll_module_exit() {
    spin_destroy(&pg.lock);
    BUG_ON(pg.npolls > 0);
}

struct xpitem *xent_new() {
    struct xpitem *ent = (struct xpitem *)mem_zalloc(sizeof(*ent));
    if (ent) {
	INIT_LIST_HEAD(&ent->lru_link);
	spin_init(&ent->lock);
	ent->ref = 0;
    }
    return ent;
}

static void xent_destroy(struct xpitem *ent) {
    struct xpoll_t *self = ent->poll;

    BUG_ON(ent->ref != 0);
    spin_destroy(&ent->lock);
    mem_free(ent, sizeof(*ent));
    pput(self->id);
}


int xent_get(struct xpitem *ent) {
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

int xent_put(struct xpitem *ent) {
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
	xent_destroy(ent);
    }
    return ref;
}

struct xpoll_t *xpoll_new() {
    struct xpoll_t *self = (struct xpoll_t *)mem_zalloc(sizeof(*self));
    if (self) {
	self->uwaiters = 0;
	atomic_init(&self->ref);
	self->size = 0;
	mutex_init(&self->lock);
	condition_init(&self->cond);
	INIT_LIST_HEAD(&self->lru_head);
    }
    return self;
}

struct xpoll_t *poll_alloc() {
    struct xpoll_t *self = xpoll_new();

    BUG_ON(pg.npolls >= XIO_MAX_POLLS);
    spin_lock(&pg.lock);
    self->id = pg.unused[pg.npolls++];
    pg.polls[self->id] = self;
    spin_unlock(&pg.lock);
    return self;
}

static void xpoll_destroy(struct xpoll_t *self) {
    mutex_destroy(&self->lock);
    mem_free(self, sizeof(struct xpoll_t));
}

struct xpoll_t *pget(int pollid) {
    struct xpoll_t *self;

    spin_lock(&pg.lock);
    if (!(self = pg.polls[pollid])) {
	spin_unlock(&pg.lock);
	return 0;
    }
    atomic_inc(&self->ref);
    spin_unlock(&pg.lock);
    return self;
}

void pput(int pollid) {
    struct xpoll_t *self = pg.polls[pollid];

    atomic_dec_and_lock(&self->ref, spin, pg.lock) {
	pg.unused[--pg.npolls] = pollid;
	xpoll_destroy(self);
	spin_unlock(&pg.lock);
    }
}

struct xpitem *__xpoll_find(struct xpoll_t *self, int fd) {
    struct xpitem *ent, *nx;
    xpoll_walk_ent(ent, nx, &self->lru_head) {
	if (ent->base.event.fd == fd)
	    return ent;
    }
    return 0;
}

/* Find xpitem by xsock id and return with ref incr if exist. */
struct xpitem *xpoll_find(struct xpoll_t *self, int fd) {
    struct xpitem *ent = 0;
    mutex_lock(&self->lock);
    if ((ent = __xpoll_find(self, fd)))
	xent_get(ent);
    mutex_unlock(&self->lock);
    return ent;
}


void __attach_to_po(struct xpitem *ent, struct xpoll_t *self) {
    BUG_ON(attached(&ent->lru_link));
    self->size++;
    list_add_tail(&ent->lru_link, &self->lru_head);
}

void __detach_from_po(struct xpitem *ent, struct xpoll_t *self) {
    BUG_ON(!attached(&ent->lru_link));
    self->size--;
    list_del_init(&ent->lru_link);
}


/* Create a new xpitem if the fd doesn't exist and get one ref for
 * caller. xpoll_add() call this.
 */
struct xpitem *xpoll_getent(struct xpoll_t *self, int fd) {
    struct xpitem *ent;

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
    pollbase_init(&ent->base, &xpollbase_vfptr);

    /* One reference for back for caller */
    ent->ref++;

    /* Cycle reference of xpoll_t and xpitem */
    ent->ref++;
    atomic_inc(&self->ref);

    ent->base.event.fd = fd;
    __attach_to_po(ent, self);
    mutex_unlock(&self->lock);

    return ent;
}

/* Remove the xpitem if the fd's ent exist. notice that don't release the
 * ref hold by xpoll_t. let caller do this.
 * xpoll_rm() call this.
 */
struct xpitem *xpoll_putent(struct xpoll_t *self, int fd) {
    struct xpitem *ent;

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


/* Pop the first xpitem. notice that don't release the ref hold
 * by xpoll_t. let caller do this.
 * xpoll_close() call this.
 */
struct xpitem *xpoll_popent(struct xpoll_t *self) {
    struct xpitem *ent;

    mutex_lock(&self->lock);
    if (list_empty(&self->lru_head)) {
	mutex_unlock(&self->lock);
	errno = ENOENT;
	return 0;
    }
    ent = list_first(&self->lru_head, struct xpitem, lru_link);
    __detach_from_po(ent, self);
    mutex_unlock(&self->lock);

    return ent;
}
