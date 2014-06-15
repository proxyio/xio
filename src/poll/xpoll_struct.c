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
#include <socket/global.h>
#include "xeventpoll.h"

struct xp_global pg;


void xpoll_module_init()
{
	int pollid;

	spin_init (&pg.lock);
	for (pollid = 0; pollid < XIO_MAX_POLLS; pollid++)
		pg.unused[pollid] = pollid;
}

void xpoll_module_exit()
{
	spin_destroy (&pg.lock);
	BUG_ON (pg.npolls > 0);
}

struct poll_fd *poll_fd_alloc() {
	struct poll_fd *pfd = TNEW (struct poll_fd);
	if (pfd) {
		INIT_LIST_HEAD (&pfd->lru_link);
		spin_init (&pfd->lock);
		pfd->ref = 0;
		pollbase_init (&pfd->base, &xpollbase_vfptr);
	}
	return pfd;
}

static void poll_fd_destroy (struct poll_fd *pfd)
{
	struct xpoll_t *self = pfd->owner;

	BUG_ON (pfd->ref != 0);
	spin_destroy (&pfd->lock);
	mem_free (pfd, sizeof (*pfd) );
	pput (self->id);
}


int poll_fd_get (struct poll_fd *pfd)
{
	int ref;
	spin_lock (&pfd->lock);
	ref = pfd->ref++;
	spin_unlock (&pfd->lock);
	return ref;
}

int poll_fd_put (struct poll_fd *pfd)
{
	int ref;

	spin_lock (&pfd->lock);
	ref = pfd->ref--;
	BUG_ON (pfd->ref < 0);
	spin_unlock (&pfd->lock);
	if (ref == 1) {
		BUG_ON (attached (&pfd->lru_link) );
		poll_fd_destroy (pfd);
	}
	return ref;
}

struct xpoll_t *xpoll_new() {
	struct xpoll_t *self = TNEW (struct xpoll_t);
	if (self) {
		self->uwaiters = 0;
		atomic_init (&self->ref);
		self->size = 0;
		mutex_init (&self->lock);
		condition_init (&self->cond);
		INIT_LIST_HEAD (&self->lru_head);
	}
	return self;
}

struct xpoll_t *poll_alloc() {
	struct xpoll_t *self = xpoll_new();

	BUG_ON (pg.npolls >= XIO_MAX_POLLS);
	spin_lock (&pg.lock);
	self->id = pg.unused[pg.npolls++];
	pg.polls[self->id] = self;
	spin_unlock (&pg.lock);
	return self;
}

static void xpoll_destroy (struct xpoll_t *self)
{
	mutex_destroy (&self->lock);
	mem_free (self, sizeof (struct xpoll_t) );
}

struct xpoll_t *pget (int pollid) {
	struct xpoll_t *self;

	spin_lock (&pg.lock);
	if (! (self = pg.polls[pollid]) ) {
		spin_unlock (&pg.lock);
		return 0;
	}
	atomic_incr (&self->ref);
	spin_unlock (&pg.lock);
	return self;
}

void pput (int pollid)
{
	struct xpoll_t *self = pg.polls[pollid];

	if (atomic_decr (&self->ref) ==  1) {
		spin_lock (&pg.lock);
		pg.unused[--pg.npolls] = pollid;
		xpoll_destroy (self);
		spin_unlock (&pg.lock);
	}
}

/* Find xpoll_item by socket fd. if fd == XPOLL_HEADFD, return head item */
struct poll_fd *ffd (struct xpoll_t *self, int fd) {
	struct poll_fd *pfd, *npfd;

	walk_poll_fd_s (pfd, npfd, &self->lru_head) {
		if (pfd->base.ent.fd == fd || fd == XPOLL_HEADFD)
			return pfd;
	}
	return 0;
}

/* Find poll_fd by xsock id and return with ref incr if exist. */
struct poll_fd *getfd (struct xpoll_t *self, int fd) {
	struct poll_fd *pfd = 0;
	mutex_lock (&self->lock);
	if ( (pfd = ffd (self, fd) ) )
		poll_fd_get (pfd);
	mutex_unlock (&self->lock);
	return pfd;
}

/* Create a new poll_fd if the fd doesn't exist and get one ref for
 * caller. xpoll_add() call this.
 */
struct poll_fd *addfd (struct xpoll_t *self, int fd) {
	struct poll_fd *pfd;

	mutex_lock (&self->lock);
	if ( (pfd = ffd (self, fd) ) ) {
		mutex_unlock (&self->lock);
		errno = EEXIST;
		return 0;
	}
	if (! (pfd = poll_fd_alloc() ) ) {
		mutex_unlock (&self->lock);
		errno = ENOMEM;
		return 0;
	}

	/* One reference for back for caller */
	pfd->ref++;

	/* Cycle reference of xpoll_t and poll_fd */
	pfd->ref++;
	atomic_incr (&self->ref);

	pfd->base.ent.fd = fd;
	BUG_ON (attached (&pfd->lru_link) );
	self->size++;
	list_add_tail (&pfd->lru_link, &self->lru_head);
	mutex_unlock (&self->lock);

	return pfd;
}

/* Remove the poll_fd if the fd's poll_fd exist. */
int rmfd (struct xpoll_t *self, int fd)
{
	struct poll_fd *pfd;

	mutex_lock (&self->lock);
	if (! (pfd = ffd (self, fd) ) ) {
		mutex_unlock (&self->lock);
		errno = ENOENT;
		return -1;
	}
	BUG_ON (!attached (&pfd->lru_link) );
	self->size--;
	list_del_init (&pfd->lru_link);
	mutex_unlock (&self->lock);
	poll_fd_put (pfd);
	return 0;
}
