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
    "PF_TCP",
    "PF_IPC",
    "PF_TCP|PF_IPC",
    "PF_INPROC",
    "PF_TCP|PF_INPROC",
    "PF_IPC|PF_INPROC",
    "PF_TCP|PF_IPC|PF_INPROC",
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

    atomic_dec_and_lock(&sb->ref, mutex, xgb.lock) {
	xgb.unused[--xgb.nsockbases] = sb->fd;
	mutex_unlock(&xgb.lock);
	sb->vfptr->close(sb);
    }
}

static void xshutdown_task_f(struct xtask *ts) {
    struct sockbase *sb = cont_of(ts, struct sockbase, shutdown);
    struct list_head poll_entries = {};
    struct xpoll_t *po = 0;
    struct xpoll_entry *ent = 0, *nent;
    
    DEBUG_ON("xsock %d shutdown %s", sb->fd, pf_str[sb->vfptr->pf]);

    INIT_LIST_HEAD(&poll_entries);
    mutex_lock(&sb->lock);
    list_splice(&sb->poll_entries, &poll_entries);
    mutex_unlock(&sb->lock);

    xsock_walk_ent(ent, nent, &poll_entries) {
	po = cont_of(ent->notify, struct xpoll_t, notify);
	xpoll_ctl(po, XPOLL_DEL, &ent->event);
	__detach_from_xsock(ent);
	xent_put(ent);
    }
    xput(sb->fd);
}

void xsock_init(struct sockbase *sb) {
    mutex_init(&sb->lock);
    condition_init(&sb->cond);
    ZERO(sb->addr);
    ZERO(sb->peer);
    sb->fasync = false;
    sb->fepipe = false;
    sb->owner = -1;
    INIT_LIST_HEAD(&sb->sub_socks);
    INIT_LIST_HEAD(&sb->sib_link);

    atomic_init(&sb->ref);
    sb->cpu_no = xcpu_choosed(sb->fd);

    sb->rcv.waiters = 0;
    sb->rcv.buf = 0;
    sb->rcv.wnd = DEF_RCVBUF;
    INIT_LIST_HEAD(&sb->rcv.head);

    sb->snd.waiters = 0;
    sb->snd.buf = 0;
    sb->snd.wnd = DEF_SNDBUF;
    INIT_LIST_HEAD(&sb->snd.head);

    INIT_LIST_HEAD(&sb->poll_entries);
    sb->shutdown.f = xshutdown_task_f;
    INIT_LIST_HEAD(&sb->shutdown.link);
    condition_init(&sb->acceptq.cond);
    sb->acceptq.waiters = 0;
    INIT_LIST_HEAD(&sb->acceptq.head);
    INIT_LIST_HEAD(&sb->acceptq.link);
}

void xsock_exit(struct sockbase *sb) {
    struct list_head head = {};
    struct xmsg *pos, *npos;

    mutex_destroy(&sb->lock);
    condition_destroy(&sb->cond);
    ZERO(sb->addr);
    ZERO(sb->peer);
    sb->fasync = 0;
    sb->fepipe = 0;
    sb->owner = -1;
    BUG_ON(!list_empty(&sb->sub_socks));
    BUG_ON(attached(&sb->sib_link));
    
    sb->fd = -1;
    sb->cpu_no = -1;

    INIT_LIST_HEAD(&head);

    sb->rcv.waiters = -1;
    sb->rcv.buf = -1;
    sb->rcv.wnd = -1;
    list_splice(&sb->rcv.head, &head);

    sb->snd.waiters = -1;
    sb->snd.buf = -1;
    sb->snd.wnd = -1;
    list_splice(&sb->snd.head, &head);

    xmsg_walk_safe(pos, npos, &head) {
	xfreemsg(pos->vec.chunk);
    }

    /* It's possible that user call xclose() and xpoll_add()
     * at the same time. and attach_to_xsock() happen after xclose().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&sb->poll_entries));
    sb->acceptq.waiters = -1;
    condition_destroy(&sb->acceptq.cond);

    atomic_destroy(&sb->ref);
    /* TODO: destroy acceptq's connection */
}
