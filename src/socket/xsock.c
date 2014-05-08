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
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include "xgb.h"

const char *pf_str[] = {
    "",
    "PF_NET",
    "PF_IPC",
    "PF_NET|PF_IPC",
    "PF_INPROC",
    "PF_NET|PF_INPROC",
    "PF_IPC|PF_INPROC",
    "PF_NET|PF_IPC|PF_INPROC",
};

/* Default snd/rcv buffer size */
static int DEF_SNDBUF = 10485760;
static int DEF_RCVBUF = 10485760;

int xalloc(int family, int socktype) {
    struct sockbase_vfptr *vfptr = sockbase_vfptr_lookup(family, socktype);
    struct sockbase *sb;

    if (!vfptr) {
	errno = EPROTO;
	return -1;
    }
    BUG_ON(!vfptr->alloc);
    if (!(sb = vfptr->alloc()))
	return -1;
    sb->vfptr = vfptr;
    mutex_lock(&xgb.lock);
    BUG_ON(xgb.nsockbases >= XIO_MAX_SOCKS);
    sb->fd = xgb.unused[xgb.nsockbases++];
    xgb.sockbases[sb->fd] = sb;
    atomic_inc(&sb->ref);
    mutex_unlock(&xgb.lock);
    return sb->fd;
}

struct sockbase *xget(int fd) {
    struct sockbase *sb;
    mutex_lock(&xgb.lock);
    if (!(sb = xgb.sockbases[fd])) {
	mutex_unlock(&xgb.lock);
	return 0;
    }
    BUG_ON(!atomic_read(&sb->ref));
    atomic_inc(&sb->ref);
    mutex_unlock(&xgb.lock);
    return sb;
}

void xput(int fd) {
    struct sockbase *sb = xgb.sockbases[fd];
    struct xcpu *cpu = xcpuget(sb->cpu_no);

    BUG_ON(!sb);
    atomic_dec_and_lock(&sb->ref, mutex, xgb.lock) {
	while (efd_signal(&cpu->efd) < 0)
	    mutex_relock(&cpu->lock);
	sb->fclosed = true;
	list_add_tail(&sb->shutdown.link, &cpu->shutdown_socks);
	mutex_unlock(&xgb.lock);
    }
}

static void xshutdown_task_f(struct xtask *ts) {
    struct sockbase *self = cont_of(ts, struct sockbase, shutdown);
    struct list_head poll_entries = {};
    struct xpoll_t *po = 0;
    struct xpoll_entry *ent = 0, *nx;
    
    DEBUG_ON("xsock %d shutdown %s", self->fd, pf_str[self->vfptr->pf]);

    INIT_LIST_HEAD(&poll_entries);
    mutex_lock(&self->lock);
    list_splice(&self->poll_entries, &poll_entries);
    mutex_unlock(&self->lock);

    xsock_walk_ent(ent, nx, &poll_entries) {
	po = cont_of(ent->notify, struct xpoll_t, notify);
	xpoll_ctl(po, XPOLL_DEL, &ent->event);
	__detach_from_xsock(ent);
	xent_put(ent);
    }

    mutex_lock(&xgb.lock);
    xgb.unused[--xgb.nsockbases] = self->fd;
    mutex_unlock(&xgb.lock);
    self->vfptr->close(self);
}

void xsock_init(struct sockbase *self) {
    mutex_init(&self->lock);
    condition_init(&self->cond);
    ZERO(self->addr);
    ZERO(self->peer);
    self->fasync = false;
    self->fok = true;
    self->fclosed = false;

    self->owner = -1;
    INIT_LIST_HEAD(&self->sub_socks);
    INIT_LIST_HEAD(&self->sib_link);

    atomic_init(&self->ref);
    self->cpu_no = xcpu_choosed(self->fd);

    self->rcv.waiters = 0;
    self->rcv.buf = 0;
    self->rcv.wnd = DEF_RCVBUF;
    INIT_LIST_HEAD(&self->rcv.head);

    self->snd.waiters = 0;
    self->snd.buf = 0;
    self->snd.wnd = DEF_SNDBUF;
    INIT_LIST_HEAD(&self->snd.head);

    INIT_LIST_HEAD(&self->poll_entries);
    self->shutdown.f = xshutdown_task_f;
    INIT_LIST_HEAD(&self->shutdown.link);
    condition_init(&self->acceptq.cond);
    self->acceptq.waiters = 0;
    INIT_LIST_HEAD(&self->acceptq.head);
    INIT_LIST_HEAD(&self->acceptq.link);
}

void xsock_exit(struct sockbase *self) {
    struct list_head head = {};
    struct xmsg *pos, *nx;

    mutex_destroy(&self->lock);
    condition_destroy(&self->cond);
    ZERO(self->addr);
    ZERO(self->peer);
    self->fasync = 0;
    self->fok = 0;
    self->fclosed = 0;

    self->owner = -1;
    BUG_ON(!list_empty(&self->sub_socks));
    BUG_ON(attached(&self->sib_link));
    
    self->fd = -1;
    self->cpu_no = -1;

    INIT_LIST_HEAD(&head);

    self->rcv.waiters = -1;
    self->rcv.buf = -1;
    self->rcv.wnd = -1;
    list_splice(&self->rcv.head, &head);

    self->snd.waiters = -1;
    self->snd.buf = -1;
    self->snd.wnd = -1;
    list_splice(&self->snd.head, &head);

    xmsg_walk_safe(pos, nx, &head) {
	xfreemsg(pos->vec.chunk);
    }

    /* It's possible that user call xclose() and xpoll_add()
     * at the same time. and attach_to_xsock() happen after xclose().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&self->poll_entries));
    self->acceptq.waiters = -1;
    condition_destroy(&self->acceptq.cond);

    atomic_destroy(&self->ref);
    /* TODO: destroy acceptq's connection */
}
