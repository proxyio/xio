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
#include <utils/waitgroup.h>
#include <utils/taskpool.h>
#include "log.h"
#include "inproc/inproc.h"
#include "mix/mix.h"
#include "sio/sio.h"

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

struct xglobal xgb = {};

struct sockbase_vfptr *sockbase_vfptr_lookup (int pf, int type) {
	struct sockbase_vfptr *vfptr, *ss;

	walk_sockbase_vfptr_s (vfptr, ss, &xgb.sockbase_vfptr_head) {
		if (pf == vfptr->pf && vfptr->type == type)
			return vfptr;
	}
	return 0;
}

void __socket_init ()
{
	waitgroup_t wg;
	int fd;
	int i;
	struct list_head *protocol_head = &xgb.sockbase_vfptr_head;

	mutex_init (&xgb.lock);
	for (fd = 0; fd < PROXYIO_MAX_SOCKS; fd++)
		xgb.unused[fd] = fd;

	/* The priority of sockbase_vfptr: inproc > ipc > tcp */
	INIT_LIST_HEAD (protocol_head);
	list_add_tail (&inproc_listener_vfptr.item, protocol_head);
	list_add_tail (&inproc_connector_vfptr.item, protocol_head);
	list_add_tail (&ipc_listener_vfptr.item, protocol_head);
	list_add_tail (&ipc_connector_vfptr.item, protocol_head);
	list_add_tail (&tcp_listener_vfptr.item, protocol_head);
	list_add_tail (&tcp_connector_vfptr.item, protocol_head);
	list_add_tail (&mix_listener_vfptr.item, protocol_head);
}

void __socket_exit()
{
	BUG_ON (xgb.nsockbases);
}

int xalloc (int family, int socktype)
{
	struct sockbase_vfptr *vfptr = sockbase_vfptr_lookup (family, socktype);
	struct sockbase *sb;

	if (!vfptr) {
		errno = EPROTO;
		return -1;
	}
	if (!(sb = vfptr->open ()))
		return -1;
	sb->vfptr = vfptr;
	mutex_lock (&xgb.lock);
	BUG_ON (xgb.nsockbases >= PROXYIO_MAX_SOCKS);
	sb->fd = xgb.unused[xgb.nsockbases++];
	xgb.sockbases[sb->fd] = sb;
	atomic_incr (&sb->ref);
	mutex_unlock (&xgb.lock);
	BUG_ON (atomic_fetch (&sb->ref) != 1);
	SKLOG_DEBUG (sb, "sock %d alloc %s", sb->fd, pf_str[sb->vfptr->pf]);
	return sb->fd;
}

struct sockbase *xget (int fd) {
	struct sockbase *sb;
	mutex_lock (&xgb.lock);
	if (! (sb = xgb.sockbases[fd])) {
		mutex_unlock (&xgb.lock);
		return 0;
	}
	BUG_ON (!atomic_fetch (&sb->ref));
	atomic_incr (&sb->ref);
	mutex_unlock (&xgb.lock);
	return sb;
}

void xput (int fd)
{
	struct sockbase *sb = xgb.sockbases[fd];

	if (atomic_decr (&sb->ref) == 1) {
		mutex_lock (&xgb.lock);
		xgb.sockbases[sb->fd] = 0;
		xgb.unused[--xgb.nsockbases] = sb->fd;
		mutex_unlock (&xgb.lock);

		SKLOG_DEBUG (sb, "sock %d shutdown %s", sb->fd, pf_str[sb->vfptr->pf]);
		sb->vfptr->close (sb);
	}
}

static void snd_msgbuf_head_empty_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	ev_signal (&sb->sig, EV_SNDBUF_EMPTY);
}

static void snd_msgbuf_head_nonempty_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	ev_signal (&sb->sig, EV_SNDBUF_NONEMPTY);
}

static void snd_msgbuf_head_full_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	ev_signal (&sb->sig, EV_SNDBUF_FULL);
}

static void snd_msgbuf_head_nonfull_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	ev_signal (&sb->sig, EV_SNDBUF_NONFULL);
}

static void snd_msgbuf_head_add_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	ev_signal (&sb->sig, EV_SNDBUF_ADD);
}

static void snd_msgbuf_head_rm_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	ev_signal (&sb->sig, EV_SNDBUF_RM);
}



static void rcv_msgbuf_head_empty_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	ev_signal (&sb->sig, EV_RCVBUF_EMPTY);
}

static void rcv_msgbuf_head_nonempty_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	ev_signal (&sb->sig, EV_RCVBUF_NONEMPTY);
}

static void rcv_msgbuf_head_full_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	ev_signal (&sb->sig, EV_RCVBUF_FULL);
}

static void rcv_msgbuf_head_nonfull_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	ev_signal (&sb->sig, EV_RCVBUF_NONFULL);
}

static void rcv_msgbuf_head_add_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	ev_signal (&sb->sig, EV_RCVBUF_ADD);
}

static void rcv_msgbuf_head_rm_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	ev_signal (&sb->sig, EV_RCVBUF_RM);
}


static struct msgbuf_vfptr snd_msgbuf_vfptr = {
	.add = snd_msgbuf_head_add_ev_hndl,
	.rm = snd_msgbuf_head_rm_ev_hndl,
	.empty = snd_msgbuf_head_empty_ev_hndl,
	.nonempty = snd_msgbuf_head_nonempty_ev_hndl,
	.full = snd_msgbuf_head_full_ev_hndl,
	.nonfull = snd_msgbuf_head_nonfull_ev_hndl,
};

static struct msgbuf_vfptr rcv_msgbuf_vfptr = {
	.add = rcv_msgbuf_head_add_ev_hndl,
	.rm = rcv_msgbuf_head_rm_ev_hndl,
	.empty = rcv_msgbuf_head_empty_ev_hndl,
	.nonempty = rcv_msgbuf_head_nonempty_ev_hndl,
	.full = rcv_msgbuf_head_full_ev_hndl,
	.nonfull = rcv_msgbuf_head_nonfull_ev_hndl,
};

static void sockbase_signal_hndl (struct ev_sig *sig, int ev)
{
	struct sockbase *sb = cont_of (sig, struct sockbase, sig);
	if (sb->vfptr->notify)
		sb->vfptr->notify (sb, ev);
}


void sockbase_init (struct sockbase *sb)
{
	mutex_init (&sb->lock);
	condition_init (&sb->cond);
	ZERO (sb->addr);
	ZERO (sb->peer);
	sb->flagset.epipe = false;
	sb->flagset.non_block = false;
	sb->flagset.verbose = 0;
	sb->owner = 0;
	INIT_LIST_HEAD (&sb->sub_socks);
	INIT_LIST_HEAD (&sb->sib_link);

	atomic_init (&sb->ref);
	ev_sig_init (&sb->sig, sockbase_signal_hndl);
	sb->el = ev_get_loop_lla ();

	socket_mstats_init (&sb->stats);
	mstats_base_set_thres (&sb->stats.base, MSL_S, ST_EV_READ, 1);
	mstats_base_set_thres (&sb->stats.base, MSL_S, ST_EV_WRITE, 1);
	mstats_base_set_warnf (&sb->stats.base, MSL_S, socket_s_warn);

	msgbuf_head_init (&sb->rcv, 409600);
	msgbuf_head_ev_hndl (&sb->rcv, &rcv_msgbuf_vfptr);
	msgbuf_head_init (&sb->snd, 409600);
	msgbuf_head_ev_hndl (&sb->snd, &snd_msgbuf_vfptr);

	INIT_LIST_HEAD (&sb->poll_entries);
	condition_init (&sb->acceptq.cond);
	sb->acceptq.waiters = 0;
	INIT_LIST_HEAD (&sb->acceptq.head);
	INIT_LIST_HEAD (&sb->acceptq.link);
}

void sockbase_exit (struct sockbase *sb)
{
	struct list_head head = {};
	struct msgbuf *msg, *tmp;

	mutex_destroy (&sb->lock);
	condition_destroy (&sb->cond);
	ZERO (sb->addr);
	ZERO (sb->peer);
	sb->flagset.epipe = 0;
	sb->flagset.non_block = 0;
	sb->flagset.verbose = 0;
	sb->owner = 0;
	BUG_ON (!list_empty (&sb->sub_socks));
	BUG_ON (attached (&sb->sib_link));
	sb->fd = -1;
	INIT_LIST_HEAD (&head);

	ev_sig_term (&sb->sig);
	msgbuf_dequeue_all (&sb->rcv, &head);
	msgbuf_dequeue_all (&sb->snd, &head);

	walk_msg_s (msg, tmp, &head)
		msgbuf_free (msg);

	/* It's possible that user call xclose() and xpoll_add()
	 * at the same time. and attach_to_sock() happen after xclose().
	 * this is a user's bug.
	 */
	BUG_ON (!list_empty (&sb->poll_entries));
	sb->acceptq.waiters = -1;
	condition_destroy (&sb->acceptq.cond);
	atomic_destroy (&sb->ref);
}
