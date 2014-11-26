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
#include "poll_struct.h"

struct pglobal pg = {};

void __poll_init(void) {
    int pollid;

    spin_init(&pg.lock);

    for (pollid = 0; pollid < PROXYIO_MAX_POLLS; pollid++) {
        pg.unused[pollid] = pollid;
    }
}

void __poll_exit(void) {
    spin_destroy(&pg.lock);
    BUG_ON(pg.npolls > 0);
}


struct poll_entry* poll_entry_alloc() {
    struct poll_entry* pfd = mem_zalloc(sizeof(struct poll_entry));

    if (pfd) {
        INIT_LIST_HEAD(&pfd->lru_link);
        spin_init(&pfd->lock);
        pfd->ref = 0;
        pollbase_init(&pfd->base, &xpollbase_vfptr);
    }

    return pfd;
}

static void poll_entry_destroy(struct poll_entry* pfd) {
    struct xpoll_t* self = pfd->owner;

    BUG_ON(pfd->ref != 0);
    spin_destroy(&pfd->lock);
    mem_free(pfd, sizeof(*pfd));
    poll_put(self->id);
}


int poll_entry_get(struct poll_entry* pfd) {
    int ref;
    spin_lock(&pfd->lock);
    ref = pfd->ref++;
    spin_unlock(&pfd->lock);
    return ref;
}

int poll_entry_put(struct poll_entry* pfd) {
    int ref;

    spin_lock(&pfd->lock);
    ref = pfd->ref--;
    BUG_ON(pfd->ref < 0);
    spin_unlock(&pfd->lock);

    if (ref == 1) {
        BUG_ON(attached(&pfd->lru_link));
        poll_entry_destroy(pfd);
    }

    return ref;
}

struct xpoll_t* poll_alloc() {
    struct xpoll_t* self = mem_zalloc(sizeof(struct xpoll_t));

    if (!self) {
        return 0;
    }

    self->uwaiters = 0;
    atomic_init(&self->ref);
    self->size = 0;
    mutex_init(&self->lock);
    condition_init(&self->cond);
    INIT_LIST_HEAD(&self->lru_head);

    BUG_ON(pg.npolls >= PROXYIO_MAX_POLLS);
    spin_lock(&pg.lock);
    self->id = pg.unused[pg.npolls++];
    pg.polls[self->id] = self;
    spin_unlock(&pg.lock);
    return self;
}

static void poll_destroy(struct xpoll_t* self) {
    mutex_destroy(&self->lock);
    mem_free(self, sizeof(struct xpoll_t));
}

struct xpoll_t* poll_get(int pollid) {
    struct xpoll_t* self;

    spin_lock(&pg.lock);

    if (!(self = pg.polls[pollid])) {
        spin_unlock(&pg.lock);
        return 0;
    }

    atomic_incr(&self->ref);
    spin_unlock(&pg.lock);
    return self;
}

void poll_put(int pollid) {
    struct xpoll_t* self = pg.polls[pollid];

    if (atomic_decr(&self->ref) ==  1) {
        spin_lock(&pg.lock);
        pg.unused[--pg.npolls] = pollid;
        poll_destroy(self);
        spin_unlock(&pg.lock);
    }
}

/* Find xpoll_item by socket fd. if fd == XPOLL_HEADFD, return head item */
struct poll_entry* find_poll_entry(struct xpoll_t* self, int fd) {
    struct poll_entry* pfd, *npfd;

    walk_poll_entry_s(pfd, npfd, &self->lru_head) {
        if (pfd->base.pollfd.fd == fd || fd == XPOLL_HEADFD) {
            return pfd;
        }
    }
    return 0;
}

/* Find poll_entry by socket fd and return with ref incr if exist. */
struct poll_entry* get_poll_entry(struct xpoll_t* self, int fd) {
    struct poll_entry* pfd = 0;
    mutex_lock(&self->lock);

    if ((pfd = find_poll_entry(self, fd))) {
        poll_entry_get(pfd);
    }

    mutex_unlock(&self->lock);
    return pfd;
}

/* Create a new poll_entry if the fd doesn't exist and get one ref for
 * caller. xpoll_add() call this.
 */
struct poll_entry* add_poll_entry(struct xpoll_t* self, int fd) {
    struct poll_entry* pfd;

    mutex_lock(&self->lock);

    if ((pfd = find_poll_entry(self, fd))) {
        mutex_unlock(&self->lock);
        errno = EEXIST;
        return 0;
    }

    if (!(pfd = poll_entry_alloc())) {
        mutex_unlock(&self->lock);
        errno = ENOMEM;
        return 0;
    }

    /* One reference for back for caller */
    pfd->ref++;

    /* Cycle reference of xpoll_t and poll_entry */
    pfd->ref++;
    atomic_incr(&self->ref);

    pfd->base.pollfd.fd = fd;
    BUG_ON(attached(&pfd->lru_link));
    self->size++;
    list_add_tail(&pfd->lru_link, &self->lru_head);
    mutex_unlock(&self->lock);

    return pfd;
}

/* Remove the poll_entry if the fd's poll_entry exist. */
int rm_poll_entry(struct xpoll_t* self, int fd) {
    struct poll_entry* pfd;

    mutex_lock(&self->lock);

    if (!(pfd = find_poll_entry(self, fd))) {
        mutex_unlock(&self->lock);
        errno = ENOENT;
        return -1;
    }

    BUG_ON(!attached(&pfd->lru_link));
    self->size--;
    list_del_init(&pfd->lru_link);
    mutex_unlock(&self->lock);
    poll_entry_put(pfd);
    return 0;
}
