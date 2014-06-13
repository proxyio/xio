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
#include <utils/taskpool.h>
#include "../xgb.h"

extern struct sockbase *getlistener (const char *addr);

/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static int snd_head_push (struct sockbase *sb)
{
	int rc = 0;
	int can = false;
	struct skbuf *msg;
	struct sockbase *peer = (cont_of (sb, struct inproc_sock, base) )->peer;

	// TODO: maybe the peer sock can't recv anymore after the check.
	mutex_unlock (&sb->lock);

	mutex_lock (&peer->lock);
	if (can_recv (peer) )
		can = true;
	mutex_unlock (&peer->lock);
	if (!can)
		return -1;
	if ( (msg = sendq_rm (sb) ) )
		recvq_add (peer, msg);
	mutex_lock (&sb->lock);
	return rc;
}

/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static int rcv_head_pop (struct sockbase *sb)
{
	int rc = 0;

	if (sb->snd.waiters)
		condition_signal (&sb->cond);
	return rc;
}


static struct sockbase *inp_alloc() {
	struct inproc_sock *self = TNEW (struct inproc_sock);

	if (self) {
		sockbase_init (&self->base);
		atomic_init (&self->ref);
		atomic_incr (&self->ref);
		return &self->base;
	}
	return 0;
}



/******************************************************************************
 *  sock_inproc_spec
 ******************************************************************************/

static int inp_connector_bind (struct sockbase *sb, const char *sock)
{
	int nfd = 0;
	struct sockbase *nsb = 0;
	struct sockbase *listener = getlistener (sock);
	struct inproc_sock *self = 0;
	struct inproc_sock *peer = 0;

	if (!listener) {
		errno = ECONNREFUSED;
		return -1;
	}
	if ( (nfd = xalloc (sb->vfptr->pf, sb->vfptr->type) ) < 0) {
		errno = EMFILE;
		xput (listener->fd);
		return -1;
	}
	nsb = xgb.sockbases[nfd];
	self = cont_of (sb, struct inproc_sock, base);
	peer = cont_of (nsb, struct inproc_sock, base);
	nsb->vfptr = sb->vfptr;
	strncpy (sb->peer, sock, TP_SOCKADDRLEN);
	strncpy (nsb->addr, sock, TP_SOCKADDRLEN);

	atomic_incr (&self->ref);
	atomic_incr (&peer->ref);
	self->peer = &peer->base;
	peer->peer = &self->base;

	if (acceptq_add (listener, nsb) < 0) {
		atomic_decr (&peer->ref);
		sb->vfptr->close (nsb);
		xput (listener->fd);
		errno = ECONNREFUSED;
		return -1;
	}
	xput (listener->fd);
	DEBUG_OFF ("%d accept new connection %d", sb->fd, nfd);
	return 0;
}

static void inp_peer_close (struct inproc_sock *peer)
{
	/* Destroy the sock and free sock id if i hold the last ref. */
	mutex_lock (&peer->base.lock);
	if (peer->base.rcv.waiters || peer->base.snd.waiters)
		condition_broadcast (&peer->base.cond);
	mutex_unlock (&peer->base.lock);

	if (atomic_decr (&peer->ref) == 1) {
		sockbase_exit (&peer->base);
		atomic_destroy (&peer->ref);
		mem_free (peer, sizeof (*peer) );
	}
}

static void inp_connector_close (struct sockbase *sb)
{
	struct inproc_sock *self = cont_of (sb, struct inproc_sock, base);
	struct inproc_sock *peer = 0;

	/* TODO: bug on here */
	if (self->peer) {
		peer = cont_of (self->peer, struct inproc_sock, base);
		inp_peer_close (peer);
	}
	if (atomic_decr (&self->ref) == 1) {
		sockbase_exit (&self->base);
		atomic_destroy (&self->ref);
		mem_free (self, sizeof (*self) );
	}
}


static void snd_head_notify (struct sockbase *sb, u32 events)
{
	if (events & XMQ_PUSH)
		snd_head_push (sb);
}

static void rcv_head_notify (struct sockbase *sb, u32 events)
{
	if (events & XMQ_POP)
		rcv_head_pop (sb);
}

static void inp_connector_notify (struct sockbase *sb, int type, u32 events)
{
	switch (type) {
	case RECV_Q:
		rcv_head_notify (sb, events);
		break;
	case SEND_Q:
		snd_head_notify (sb, events);
		break;
	default:
		BUG_ON (1);
	}
}

struct sockbase_vfptr inp_connector_spec = {
	.type = XCONNECTOR,
	.pf = XPF_INPROC,
	.alloc = inp_alloc,
	.bind = inp_connector_bind,
	.close = inp_connector_close,
	.notify = inp_connector_notify,
	.getopt = 0,
	.setopt = 0,
};
