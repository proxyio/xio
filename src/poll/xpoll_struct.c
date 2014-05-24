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


#include <utils/timer.h>
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

struct xpitem *xpitem_alloc() {
    struct xpitem *itm = (struct xpitem *)mem_zalloc(sizeof(*itm));
    if (itm) {
	INIT_LIST_HEAD(&itm->lru_link);
	spin_init(&itm->lock);
	itm->ref = 0;
	pollbase_init(&itm->base, &xpollbase_vfptr);
    }
    return itm;
}

static void xpitem_destroy(struct xpitem *itm) {
    struct xpoll_t *self = itm->poll;

    BUG_ON(itm->ref != 0);
    spin_destroy(&itm->lock);
    mem_free(itm, sizeof(*itm));
    pput(self->id);
}


int xpitem_get(struct xpitem *itm) {
    int ref;
    spin_lock(&itm->lock);
    ref = itm->ref++;
    spin_unlock(&itm->lock);
    return ref;
}

int xpitem_put(struct xpitem *itm) {
    int ref;

    spin_lock(&itm->lock);
    ref = itm->ref--;
    BUG_ON(itm->ref < 0);
    spin_unlock(&itm->lock);
    if (ref == 1) {
	BUG_ON(attached(&itm->lru_link));
	xpitem_destroy(itm);
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

/* Find xpoll_item by socket fd. if fd == XPOLL_HEADFD, return head item */
struct xpitem *ffd(struct xpoll_t *self, int fd) {
    struct xpitem *itm, *nitm;

    walk_xpitem_safe(itm, nitm, &self->lru_head) {
	if (itm->base.ent.fd == fd || fd == XPOLL_HEADFD)
	    return itm;
    }
    return 0;
}

/* Find xpitem by xsock id and return with ref incr if exist. */
struct xpitem *getfd(struct xpoll_t *self, int fd) {
    struct xpitem *itm = 0;
    mutex_lock(&self->lock);
    if ((itm = ffd(self, fd)))
	xpitem_get(itm);
    mutex_unlock(&self->lock);
    return itm;
}

/* Create a new xpitem if the fd doesn't exist and get one ref for
 * caller. xpoll_add() call this.
 */
struct xpitem *addfd(struct xpoll_t *self, int fd) {
    struct xpitem *itm;

    mutex_lock(&self->lock);
    if ((itm = ffd(self, fd))) {
	mutex_unlock(&self->lock);
	errno = EEXIST;
	return 0;
    }
    if (!(itm = xpitem_alloc())) {
	mutex_unlock(&self->lock);
	errno = ENOMEM;
	return 0;
    }

    /* One reference for back for caller */
    itm->ref++;

    /* Cycle reference of xpoll_t and xpitem */
    itm->ref++;
    atomic_inc(&self->ref);

    itm->base.ent.fd = fd;
    BUG_ON(attached(&itm->lru_link));
    self->size++;
    list_add_tail(&itm->lru_link, &self->lru_head);
    mutex_unlock(&self->lock);

    return itm;
}

/* Remove the xpitem if the fd's xpitem exist. */
int rmfd(struct xpoll_t *self, int fd) {
    struct xpitem *itm;

    mutex_lock(&self->lock);
    if (!(itm = ffd(self, fd))) {
	mutex_unlock(&self->lock);
	errno = ENOENT;
	return -1;
    }
    BUG_ON(!attached(&itm->lru_link));
    self->size--;
    list_del_init(&itm->lru_link);
    mutex_unlock(&self->lock);
    xpitem_put(itm);
    return 0;
}
